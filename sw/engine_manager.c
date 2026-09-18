// ---------------------------------------------------------------------------
// engine_manager.c -- A3: SMART-style per-engine health monitor + masking
// + DVFS hooks (benchmark B4 vs Auradine SMART).
//
// The RTL already isolates engines (per-engine pipes, staggered windows);
// this manager adds: per-engine expected-found-rate model, dead/slow engine
// detection, and masking (that engine's window is collapsed to empty).
//
// RTL TODO uncovered while writing this: add a per-engine ENABLE bit to
// CTRL so masking does not require window surgery. Until then, mask = set
// the engine's [nonceStart,nonceEnd) window to empty via stride tricks.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "miner_driver.h"   // register layer (same map as miner_driver.c)

#define N_ENG   8
#define EWMA_A  4           // 1/4 weight for new samples

typedef struct {
    uint64_t expected;      // model: nonces scanned since job start
    uint64_t hits;          // found count (only valid if target is winnable)
    uint32_t last_nonce;    // liveness probe
    uint8_t  state;         // 0=ok 1=suspect 2=dead 3=masked
} eng_health_t;

static eng_health_t H[N_ENG];

void em_init(void) { memset(H, 0, sizeof H); }

// call periodically (e.g. every 100 ms) while a job runs
void em_poll(uint32_t job_start_nonce, uint32_t window_per_eng) {
    for (int i = 0; i < N_ENG; i++) {
        // liveness: nonce counter of engine i advanced?
        uint32_t cur = em_read_engine_nonce(i);   // TODO: expose per-engine
                                                  // current nonce in STATUS2
        if (cur == H[i].last_nonce && H[i].state < 2) H[i].state = 1; // suspect
        else if (H[i].state == 1) H[i].state = 0;
        H[i].last_nonce = cur;

        // found-rate model: with target all-ones (benchmark mode) every
        // engine must report ~window_per_eng hits per window; real targets
        // use Poisson expectation from pool difficulty instead.
        // TODO: wire em_found_count(i) to per-engine HIT counters in STATUS.
    }
}

// mask a dead engine: collapse its window (until RTL gains per-engine EN bit)
void em_mask(int i) {
    H[i].state = 3;
    em_set_engine_window(i, 0, 0);   // empty window -> engine idles at done
}

// DVFS hook table: V/f per engine bin, filled from calibration sweep.
// Values are BOARD-level (regulator commands over I2C/PWM), not MMIO.
typedef struct { uint32_t mv; uint32_t mhz; uint32_t est_jth_x100; } vf_point_t;
static vf_point_t VF[N_ENG];
void em_dvfs_calibrate(int i) {
    // sweep 0.65-1.30V in 50 mV steps at fixed work, record hashes/W;
    // pick argmax into VF[i]. TODO: power measurement interface (INA233).
    (void)i;
}
