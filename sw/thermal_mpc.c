
// thermal_mpc.c -- 1-step lookahead thermal controller (docs/AI_ACCELERATION.md
// Tier-1 item 3). Upgrades dvfs_daemon.c::thermal_policy's linear derating.
// Pure + host-verified (-DMPC_TEST): tracks hot-water outlet target while
// respecting Tj_max, and must BEAT the linear policy on captured heat value.
#include <stdint.h>

typedef struct { double a, b, k; } plant_t;   // see plant_step for semantics

typedef struct { double pump_u; int vsel_delta; } therm_act_t;

// Plant (per control step):
//   T_out' = T_out + a*P - b*u*(T_out - T_in)     water energy balance
//   Tj     = T_out + k*P                          junction gradient above water
// VSEL shedding takes effect on the NEXT step (actuator delay, 1 tick).
static double plant_step(double *t_out, double t_in, double u, double P,
                         double *p_next, int vsel_delta, plant_t pl) {
    double Pnow = *p_next;                        // last step's shed lands now
    *p_next = P * (1.0 + 0.08 * vsel_delta);      // this step's shed lands next
    *t_out = *t_out + pl.a * Pnow - pl.b * u * (*t_out - t_in);
    return *t_out + pl.k * Pnow;                  // Tj
}

// MPC law: invert model for pump to hit T_out target next step; if the
// resulting 1-step-ahead Tj would exceed budget, pre-shed VSEL NOW
// (anticipation -- this is the entire advantage over the reactive policy).
therm_act_t mpc_policy(double t_out, double t_in, double tj, double P,
                       double t_out_target, double tj_max, plant_t pl) {
    (void)tj;
    therm_act_t r = { 0.5, 0 };
    double dt = t_out - t_in; if (dt < 1e-3) dt = 1e-3;
    double need = (t_out + pl.a * P - t_out_target) / (pl.b * dt);
    r.pump_u = need < 0 ? 0.0 : need > 1 ? 1.0 : need;
    double t_out_next = t_out + pl.a * P - pl.b * r.pump_u * dt;
    double tj_next = t_out_next + pl.k * P;
    if (tj_next > tj_max)            r.vsel_delta = -2;
    else if (tj_next > tj_max - 4.0) r.vsel_delta = -1;
    else if (tj_next < tj_max - 12.0 && t_out_next < t_out_target - 2.0) r.vsel_delta = +1;
    return r;
}

// reference: current linear policy (reactive shed only at the limit)
therm_act_t linear_policy(double t_out, double t_in, double tj, double P,
                          double t_out_target, double tj_max, plant_t pl) {
    (void)t_out; (void)t_in; (void)P; (void)t_out_target; (void)pl;
    therm_act_t r = { 0.7, 0 };
    if (tj > tj_max - 10.0) r.pump_u = 1.0;
    if (tj > tj_max)        r.vsel_delta = -2;
    return r;
}

#ifdef MPC_TEST
#include <stdio.h>
static double rs = 7;
static double frand(void){ rs = rs*1103515245+12345; return ((unsigned)rs%1000)/1000.0-0.5; }

int main(void) {
    plant_t pl = { 0.01, 0.8, 0.02 };       // feasible: Tj=T_out+0.02P <= 80+ margin
    double tj_budget = 80.0, target = 58.0;
    double to_m = 50, to_l = 50, pn_m = 300, pn_l = 300, tj_m = 0, tj_l = 0;
    double vm = 0, vl = 0; int n = 0, viol = 0;
    for (int t = 0; t < 600; t++) {
        double P = 300 + 400 * (t > 200 ? 1.0 : (double)t / 200.0);   // heat wave
        double tin = 45 + 3 * frand();
        therm_act_t am = mpc_policy(to_m, tin, tj_m, P, target, tj_budget, pl);
        therm_act_t al = linear_policy(to_l, tin, tj_l, P, target, tj_budget, pl);
        tj_m = plant_step(&to_m, tin, am.pump_u, P, &pn_m, am.vsel_delta, pl);
        tj_l = plant_step(&to_l, tin, al.pump_u, P, &pn_l, al.vsel_delta, pl);
        if (tj_m > tj_budget) viol++;
        double cm = to_m < target ? to_m : target;      // captured heat value
        double cl = to_l < target ? to_l : target;
        vm += cm; vl += cl; n++;
    }
    printf("captured heat value  MPC=%.2f  linear=%.2f  (delta %+.2f)\n", vm/n, vl/n, (vm-vl)/n);
    printf("Tj violations (MPC): %d\n", viol);
    if (viol > 0)              { printf("FAIL: budget violations\n"); return 1; }
    if (vm < vl + 0.5)         { printf("FAIL: no beat over linear\n"); return 1; }
    printf("[ok] MPC: 0 violations, anticipates heat wave, beats linear policy\n");
    return 0;
}
#endif
