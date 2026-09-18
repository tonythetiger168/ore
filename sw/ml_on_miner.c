
// ml_on_miner.c -- Tier-1 ML for the miner (docs/AI_ACCELERATION.md).
// Two components, both PURE and host-verified (build with -DML_TEST):
//   1. dvfs_bandit: Gaussian Thompson sampling over (vsel,freq) arms.
//      Reward = accepted-share-rate / watt (from pick_pareto telemetry).
//   2. ewma_fault: per-engine error-rate anomaly detector (predictive
//      maintenance; feeds engine_manager FAULT_MSK).
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

// ---- Gaussian Thompson sampling bandit ------------------------------------
typedef struct { double mu, sigma2, n; } arm_t;

void bandit_init(arm_t *a, int k) {
    for (int i = 0; i < k; i++) { a[i].mu = 0.0; a[i].sigma2 = 1.0; a[i].n = 0.0; }
}

// sample theta ~ N(mu, sigma2/(n+1)) ; returns chosen arm
int bandit_pick(arm_t *a, int k, double (*rng)(void)) {
    int best = 0; double best_t = -1e30;
    for (int i = 0; i < k; i++) {
        double u1 = rng() + 1e-12, u2 = rng();
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        double th = a[i].mu + z * sqrt(a[i].sigma2 / (a[i].n + 1.0));
        if (th > best_t) { best_t = th; best = i; }
    }
    return best;
}

void bandit_update(arm_t *a, int arm, double reward) {
    a[arm].n += 1.0;
    double d = reward - a[arm].mu;
    a[arm].mu += d / a[arm].n;
    double d2 = reward - a[arm].mu;
    if (a[arm].n > 1.0) a[arm].sigma2 = ((a[arm].n - 2.0) * a[arm].sigma2 + d * d2) / (a[arm].n - 1.0);
    if (a[arm].sigma2 < 1e-6) a[arm].sigma2 = 1e-6;
}

// ---- EWMA fault detector ---------------------------------------------------
typedef struct { double mean, var, lam; uint32_t n; } ewma_t;

void ewma_init(ewma_t *e, double lam) { e->mean = 0; e->var = 0; e->lam = lam; e->n = 0; }

// returns z-score (>4 alarms); call with one error-rate sample per window
#define EWMA_WARMUP 50   // no alarms until statistics settle
double ewma_fault(ewma_t *e, double x) {
    if (e->n == 0) { e->mean = x; e->var = 0; e->n = 1; return 0; }
    double d = x - e->mean;
    double inc = e->lam * d;
    e->mean += inc;
    e->var = (1.0 - e->lam) * (e->var + e->lam * d * d);
    e->n++;
    if (e->n < EWMA_WARMUP) return 0.0;                 // warmup guard
    double sd = sqrt(e->var / (2.0 - e->lam) + 1e-12);   // EWMA std approx
    return sd > 0 ? (x - e->mean) / sd : 0.0;
}

#ifdef ML_TEST
#include <stdio.h>
// deterministic rng for reproducible test (LCG)
static uint64_t s = 0x12345;
static double rng(void) { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return (double)(s >> 11) / (1ULL << 53); }

int main(void) {
    // ---- bandit convergence test: known optimum at arm 3
    enum { K = 8 };
    arm_t arms[K];
    bandit_init(arms, K);
    double pulls[K];
    for (int i = 0; i < K; i++) pulls[i] = 0.0;
    int late_hits = 0, late_n = 0;
    for (int t = 0; t < 3000; t++) {
        int a = bandit_pick(arms, K, rng);
        // synthetic environment: reward peak at arm 3, sigma noise
        double r = exp(-((a - 3.0) * (a - 3.0)) / 2.0) * 0.9 + 0.05 * (rng() - 0.5);
        bandit_update(arms, a, r);
        pulls[a]++;
        if (t >= 2800) { late_n++; if (a == 3) late_hits++; }
    }
    printf("arm pulls: "); for (int i = 0; i < K; i++) printf("%d ", (int)pulls[i]);
    printf("\nlate-phase optimal-arm rate: %d/%d = %.2f\n", late_hits, late_n, (double)late_hits / late_n);
    if (late_hits < (int)(late_n * 0.8)) { printf("FAIL: bandit did not converge\n"); return 1; }
    // arms[3] posterior should have highest mean
    int best = 0; for (int i = 1; i < K; i++) if (arms[i].mu > arms[best].mu) best = i;
    if (best != 3) { printf("FAIL: posterior argmax %d\n", best); return 1; }
    printf("[ok] bandit converges to true optimum (arm 3)\n");

    // ---- EWMA fault injection test
    ewma_t e; ewma_init(&e, 0.05);
    int alarms = 0, false_alarms = 0;
    for (int t = 0; t < 500; t++) {
        double x = (t < 400) ? 0.001 + 0.0005 * (rng() - 0.5)      // healthy
                             : 0.020 + 0.002 * (rng() - 0.5);       // fault at t=400
        double z = ewma_fault(&e, x);
        if (z > 4.0) { alarms++; if (t < 400) false_alarms++; }
    }
    printf("alarms=%d (expect >0), false_alarms=%d (expect 0)\n", alarms, false_alarms);
    if (alarms == 0 || false_alarms > 0) { printf("FAIL: ewma detector\n"); return 1; }
    printf("[ok] ewma fault detector (injected shift detected, no false alarms)\n");
    return 0;
}
#endif
