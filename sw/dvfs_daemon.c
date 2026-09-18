
// dvfs_daemon.c -- boot-time V/f sweep + Pareto point selection + thermal
// derating for the mining SoC. Pairs with sw/engine_manager.c (telemetry)
// and MMIO reg 0xB8 (V-select) / 0xB0 (per-engine clk_en).
//
// The Pareto picker (pick_pareto) is PURE: host-testable without hardware.
// Build check: gcc -O1 -DDVFS_TEST dvfs_daemon.c -o dvfs_test && ./dvfs_test
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t  vsel;        // V-select index 0..7
    uint32_t freq_mhz;
    double   hashrate;    // measured TH/s
    double   power_w;     // measured rail power
    double   err_rate;    // share/reject error ratio 0..1
    double   temp_c;
} dvfs_point_t;

// candidates: array of measured points; n: count
// returns index of best point: min J/TH s.t. err < err_max and temp < tmax
int pick_pareto(const dvfs_point_t *c, int n, double err_max, double tmax) {
    int best = -1; double best_j = 0;
    for (int i = 0; i < n; i++) {
        if (c[i].err_rate > err_max) continue;
        if (c[i].temp_c   > tmax)    continue;
        if (c[i].hashrate <= 0)      continue;
        double j = c[i].power_w / c[i].hashrate;      // J/TH
        if (best < 0 || j < best_j) { best = i; best_j = j; }
    }
    return best;
}

// temperature derating: given current temp and the chosen point, return the
// V-select step-down count (0 = hold, 1.. = drop N steps). Linear policy.
int derate_steps(double temp_c, double tmax, double hyst) {
    if (temp_c < tmax - hyst) return 0;
    if (temp_c < tmax)        return 0;       // in hysteresis band: hold
    int over = (int)(temp_c - tmax);
    return over / 5 + 1;                       // one step per 5C over
}


// ---- thermal policy (pure; host-tested) -----------------------------------
// Inputs from sensor block (MMIO 0xC0+, see docs/LIQUID_COOLING.md):
// coolant in/out temps, per-zone junction estimate, dewpoint, flow, leak.
// Outputs: vsel delta (steps), alarm level (0 none, 1 warn, 2 shutdown).
typedef struct {
    double t_in, t_out, tj_est, dewpoint;
    int flow_ok, leak, reuse_mode;
} thermal_t;

typedef struct { int vsel_delta, alarm; const char *reason; } thermal_action_t;

thermal_action_t thermal_policy(thermal_t s, double tj_max, double t_out_target) {
    if (s.leak)     return (thermal_action_t){ 0, 2, "LEAK: initiate shutdown" };
    if (!s.flow_ok) return (thermal_action_t){ 0, 2, "FLOW LOSS: initiate shutdown" };
    if (s.t_in < s.dewpoint + 3.0)
                    return (thermal_action_t){-1, 1, "condensation guard: derate + alarm" };
    if (s.tj_est > tj_max)
                    return (thermal_action_t){-2, 1, "Tj over limit: derate hard" };
    if (s.reuse_mode && s.t_out < t_out_target - 5.0 && s.tj_est < tj_max - 10.0)
                    return (thermal_action_t){+1, 0, "heat-reuse: headroom to harvest" };
    return (thermal_action_t){ 0, 0, NULL };
}

#ifdef DVFS_TEST
#include <stdio.h>
#include <math.h>
int main(void) {
    // mock sweep: 8 V-select levels x 3 freq bins
    dvfs_point_t pts[8] = {
        {0, 800,  42.0, 1550, 0.001, 61},   // high V: fast but hot
        {1, 800,  40.1, 1290, 0.001, 58},
        {2, 800,  37.5, 1080, 0.001, 55},
        {3, 800,  33.0,  860, 0.001, 51},   // Pareto-optimal (J/TH ~26)
        {4, 800,  26.0,  780, 0.004, 47},
        {5, 800,  17.0,  640, 0.012, 43},   // err_rate too high
        {6, 800,   9.0,  540, 0.05,  40},
        {7, 800,   3.0,  470, 0.21,  38},
    };
    int b = pick_pareto(pts, 8, 0.01, 65.0);
    printf("picked idx %d (expect 3), J/TH=%.2f\n", b, pts[b].power_w / pts[b].hashrate);
    if (b != 3) return 1;
    if (derate_steps(70, 65, 3) < 1) return 1;
    if (derate_steps(60, 65, 3) != 0) return 1;
    printf("[ok] pareto + derate logic\n");

    thermal_t s;
    s = (thermal_t){45, 55, 65, 17, 1, 0, 0};
    thermal_action_t a = thermal_policy(s, 80, 58);
    printf("normal: d=%d alarm=%d\n", a.vsel_delta, a.alarm);
    if (a.vsel_delta != 0 || a.alarm != 0) return 1;
    s.leak = 1;
    a = thermal_policy(s, 80, 58);
    if (a.alarm != 2) return 1;
    s.leak = 0; s.flow_ok = 0;
    a = thermal_policy(s, 80, 58);
    if (a.alarm != 2) return 1;
    s.flow_ok = 1; s.t_in = 15;   // below dewpoint+3
    a = thermal_policy(s, 80, 58);
    if (a.vsel_delta != -1 || a.alarm != 1) return 1;
    s.t_in = 45; s.tj_est = 85;   // over Tj
    a = thermal_policy(s, 80, 58);
    if (a.vsel_delta != -2 || a.alarm != 1) return 1;
    s.tj_est = 65; s.reuse_mode = 1; s.t_out = 45;  // heat-reuse headroom
    a = thermal_policy(s, 80, 58);
    if (a.vsel_delta != 1 || a.alarm != 0) return 1;
    printf("[ok] thermal policy (leak/flow/condensation/Tj/reuse)\n");
    return 0;
}
#endif
