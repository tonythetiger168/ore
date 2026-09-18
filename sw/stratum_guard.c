
// stratum_guard.c -- Tier-1 item 4 (docs/AI_ACCELERATION.md): anomaly
// sentinel on the pool link. Detects share-rate collapse (pool-side fault,
// hijack, or Antbleed-class suppression) via a sustained-drop test over an
// EWMA baseline. Pure + host-verified (-DGUARD_TEST).
#include <stdint.h>

typedef struct { double mean, lam, baseline; uint32_t n, low_run, alarm; } guard_t;

void guard_init(guard_t *g, double lam) {
    g->mean = 0; g->lam = lam; g->baseline = 0; g->n = 0; g->low_run = 0; g->alarm = 0;
}

// shares: accepted shares observed this window (normalized per window time)
// returns 1 when ALARM latched (sustained collapse); latches until reset
int guard_feed(guard_t *g, double shares, double drop_frac, uint32_t sustain) {
    if (g->n < 100) {                       // baseline build-up, never alarm
        g->mean = g->n == 0 ? shares : g->mean + g->lam * (shares - g->mean);
        g->n++;
        if (g->n == 100) g->baseline = g->mean;
        return g->alarm;
    }
    if (shares < g->baseline * (1.0 - drop_frac)) {
        if (++g->low_run >= sustain) g->alarm = 1;
    } else {
        g->low_run = 0;
        // slow baseline tracking to follow legitimate difficulty shifts
        g->baseline += g->lam * 0.1 * (shares - g->baseline);
    }
    return g->alarm;
}

#ifdef GUARD_TEST
#include <stdio.h>
static double rs = 3;
static double frand(void){ rs=rs*1103515245+12345; return ((unsigned)rs%1000)/1000.0; }

int main(void) {
    guard_t g; guard_init(&g, 0.05);
    // 300 healthy windows ~ N(10, 1): no alarm expected
    for (int t = 0; t < 300; t++) {
        int a = guard_feed(&g, 10.0 + (frand() - 0.5) * 2.0, 0.5, 5);
        if (t > 150 && a) { printf("FAIL: false alarm at t=%d\n", t); return 1; }
    }
    if (g.alarm) { printf("FAIL: false alarm in healthy phase\n"); return 1; }
    // collapse to 10% of baseline for 20 windows: must alarm
    for (int t = 0; t < 20; t++) guard_feed(&g, 1.0, 0.5, 5);
    if (!g.alarm) { printf("FAIL: collapse not detected\n"); return 1; }
    printf("[ok] stratum guard: 0 false alarms in 300 healthy windows, "
           "collapse alarmed and latched\n");
    return 0;
}
#endif
