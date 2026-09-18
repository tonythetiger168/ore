// ---------------------------------------------------------------------------
// miner_driver.c -- Linux userspace driver for the mining accelerator.
// mmap the MMIO block and scan nonce windows handed down by a stratum client.
//
// Register map (base = 0x10020000, see hw/MiningAccel.scala):
//   0x00 W  ctrl        bit0=start  bit1=load_template  bit2=clear
//   0x04 R  status      bit0=busy  bit1=done
//   0x08 RW irq_en
//   0x0C RW nonce_start
//   0x10 RW block1_w[0..15]      (header bytes  0..63, big-endian words)
//   0x50 RW b2_w0..2             (merkle tail | time | bits)
//   0x5C RW nonce_stride         (per-engine range split, default 0x02000000)
//   0x60 RW target[255:0]        (8 x 32-bit, word 0 = MSW)
//   0x80 R  nonce_found
//   0x84 R  digest_found[255:0]
//   0xA4 R  found (sticky; write ctrl.bit2 to clear)
//
// IRQ: source 0 on the PLIC. This driver uses polling for simplicity.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define MINING_BASE   0x10020000UL
#define MAP_SIZE      0x1000

#define R_CTRL        0x00
#define R_STATUS      0x04
#define R_IRQ_EN      0x08
#define R_NONCE_START 0x0C
#define R_BLOCK1      0x10
#define R_B2W         0x50
#define R_NONCE_STRIDE 0x5C
#define R_TARGET      0x60
#define R_NONCE_FOUND 0x80
#define R_DIGEST      0x84
#define R_FOUND       0xA4

static volatile uint32_t *regs;

static inline void reg_wr(uint32_t off, uint32_t v) { regs[off / 4] = v; }
static inline uint32_t reg_rd(uint32_t off)         { return regs[off / 4]; }

static int driver_init(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("/dev/mem"); return -1; }
    void *p = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                   fd, MINING_BASE);
    if (p == MAP_FAILED) { perror("mmap"); return -1; }
    regs = (volatile uint32_t *)p;
    return 0;
}

// header must be the full 80-byte block header as it appears on the wire
// (little-endian fields, e.g. straight from the stratum notify payload).
static void load_template(const uint8_t header[80], const uint8_t target[32]) {
    // bytes 0..63 -> big-endian words
    for (int i = 0; i < 16; i++) {
        uint32_t w = ((uint32_t)header[4*i] << 24) | ((uint32_t)header[4*i+1] << 16) |
                     ((uint32_t)header[4*i+2] << 8)  |  (uint32_t)header[4*i+3];
        reg_wr(R_BLOCK1 + 4*i, w);
    }
    // bytes 64..75: merkle tail | time | bits
    for (int i = 0; i < 3; i++) {
        uint32_t w = ((uint32_t)header[64+4*i] << 24) | ((uint32_t)header[64+4*i+1] << 16) |
                     ((uint32_t)header[64+4*i+2] << 8)  |  (uint32_t)header[64+4*i+3];
        reg_wr(R_B2W + 4*i, w);
    }
    // target, big-endian: digest word a = MSB. Write MSW first at 0x60.
    for (int i = 0; i < 8; i++) {
        uint32_t w = ((uint32_t)target[4*i] << 24) | ((uint32_t)target[4*i+1] << 16) |
                     ((uint32_t)target[4*i+2] << 8)  |  (uint32_t)target[4*i+3];
        reg_wr(R_TARGET + 4*i, w);
    }
    reg_wr(R_CTRL, 0x2);   // load_template: engines compute midstate (~70 cycles)
    while (reg_rd(R_STATUS) & 0x1) { /* wait for midstate */ }
}

// returns true and fills nonce_out if a share was found before the window
// was exhausted.
static bool scan_nonces(uint32_t start, uint32_t *nonce_out) {
    reg_wr(R_NONCE_START, start);
    reg_wr(R_CTRL, 0x1);                       // start
    while (1) {
        uint32_t st = reg_rd(R_STATUS);
        if (reg_rd(R_FOUND)) {
            *nonce_out = reg_rd(R_NONCE_FOUND);
            reg_wr(R_CTRL, 0x4);               // clear
            return true;
        }
        if (!(st & 0x1))                       // busy==0 -> window exhausted
            return false;
    }
}

// ---------------------------------------------------------------------------
// Integration with a stratum client (skeleton):
//   on_notify()    -> parse job, build 80-byte header, load_template()
//   submit_share() -> pack [found_nonce + header minus nonce] for stratum
// ---------------------------------------------------------------------------
extern int  stratum_connect(const char *url, const char *user);
extern int  stratum_on_notify(uint8_t header[80], uint8_t target[32]);
extern void stratum_submit(uint32_t nonce, const uint8_t header[80]);

int main(int argc, char **argv) {
    if (driver_init() != 0) return 1;

    uint8_t header[80], target[32];
    if (stratum_connect(argv[1], argv[2]) != 0) return 1;

    uint32_t next_start = 0;
    for (;;) {
        // blocking: fills header/target from a stratum "mining.notify"
        if (stratum_on_notify(header, target) != 0) break;
        load_template(header, target);

        // walk the 2^32 nonce space in per-engine strides until share or
        // a new job arrives (poll stratum fd between windows -- omitted)
        for (;;) {
            uint32_t nonce;
            if (scan_nonces(next_start, &nonce)) {
                stratum_submit(nonce, header);   // found a share
            }
            next_start += 8 * 0x02000000u;       // 8 engines x stride
            if (next_start == 0) break;          // wrapped the whole space
        }
    }
    return 0;
}
