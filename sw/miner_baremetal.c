// ---------------------------------------------------------------------------
// miner_baremetal.c -- OS-less bring-up smoke test (runs under pk/bbl or
// raw HTIF in sim). Loads the Bitcoin genesis template with target=all-ones,
// scans from nonce 0, and checks the engine reports nonce 0 with the
// golden digest from tests/genesis_kat.json (sim_vectors[0]).
//
// Exit via HTIF tohost: value 1 = pass, 3 = fail. In Verilator sim:
//   make CONFIG=RocketMiningConfig run-binary BINARY=miner_baremetal.riscv
// ---------------------------------------------------------------------------
#include <stdint.h>

#define REG8(addr) (*(volatile uint8_t *)(addr))
#define REG32(addr) (*(volatile uint32_t *)(addr))

#define MINING_BASE   0x10020000UL
#define R_CTRL        (MINING_BASE + 0x00)
#define R_STATUS      (MINING_BASE + 0x04)
#define R_NONCE_START (MINING_BASE + 0x0C)
#define R_BLOCK1      (MINING_BASE + 0x10)   // 16 words
#define R_B2W         (MINING_BASE + 0x50)   // 3 words
#define R_TARGET      (MINING_BASE + 0x60)   // 8 words, MSW first
#define R_NONCE_FOUND (MINING_BASE + 0x80)
#define R_DIGEST      (MINING_BASE + 0x84)   // 8 words, LE limbs (bswapped)
#define R_FOUND       (MINING_BASE + 0xA4)

// Bitcoin genesis block template (tests/genesis_kat.json), header bytes
// 0..63 and 64..75 as big-endian words, nonce slot zeroed.
static const uint32_t B1[16]  = { 0x01000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x3ba3edfdU, 0x7a7b12b2U, 0x7ac72c3eU, 0x67768f61U, 0x7fc81bc3U, 0x888a5132U, 0x3a9fb8aaU };
static const uint32_t B2W[3]  = { 0x4b1e5e4aU, 0x29ab5f49U, 0xffff001dU };
// sim_vectors[0].outer_words -- engine stores byte-reversed words as LE limbs
static const uint32_t EXP[8]  = { 0xbf483998U, 0xa9b44cbfU, 0x5a113973U, 0xe34da96bU, 0x5cf3c775U, 0x7d75ac3bU, 0xd7c6b30aU, 0xf5a7c12bU };

static volatile uint64_t tohost   __attribute__((aligned(64), section(".tohost")));
static volatile uint64_t fromhost __attribute__((aligned(64), section(".fromhost")));

static void exit_with(int code) {
    tohost = ((uint64_t)code << 1) | 1;   // HTIF exit: 0=pass 1=fail
    for (;;) { }                          // spin; sim host terminates on tohost
}

static uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000ffU) << 24) | ((x & 0x0000ff00U) << 8) |
           ((x & 0x00ff0000U) >> 8)  | ((x & 0xff000000U) >> 24);
}

int main(void) {
    // template
    for (int i = 0; i < 16; i++) REG32(R_BLOCK1 + 4*i) = B1[i];
    for (int i = 0; i < 3;  i++) REG32(R_B2W    + 4*i) = B2W[i];
    for (int i = 0; i < 8;  i++) REG32(R_TARGET + 4*i) = 0xffffffffU; // all-ones: nonce 0 hits
    REG32(R_NONCE_START) = 0;

    REG32(R_CTRL) = 0x2;                       // load_template (midstate pass)
    while (REG32(R_STATUS) & 0x1) { }          // wait busy==0

    REG32(R_CTRL) = 0x1;                       // start scan at nonce 0
    while (!(REG32(R_FOUND) & 0x1)) { }        // wait hit

    uint32_t nonce = REG32(R_NONCE_FOUND);
    int ok = (nonce == 0);
    for (int i = 0; i < 8 && ok; i++) {
        uint32_t got = REG32(R_DIGEST + 4*i);
        ok = (got == bswap32(EXP[i]));         // LE-limb storage check
    }
    REG32(R_CTRL) = 0x4;                       // clear
    exit_with(ok ? 0 : 1);
    return 0;
}
