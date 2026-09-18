# Production test plan (Line A/C)

## Flow

1. Wafer probe (ATE): scan ATPG (DFT.md) + KAT-BIST per die (genesis
   vector, 70 cycles x engines) -> per-engine fault map -> OTP/e-fuse.
2. Package / final test: KAT + toggle-count sanity + calorimetric check
   for liquid modules (LIQUID_COOLING.md sec 7: dT x flow vs P, 5%).
3. Burn-in: HTOL 500h at Vmax, KAT-BIST as workload (G2 gate).
4. Binning: DVFS sweep (dvfs_daemon pick_pareto) bins each unit by
   Vmin @ target hashrate; bin printed on label, fleet software uses it.

## Golden vectors as test patterns

tests/genesis_kat.json IS the production test vector set (sim_vectors for
fast pass, real_target/real_nonce for soak). Any change to RTL or
firmware that breaks these is a test-program change -- same review bar.

## Acceptance criteria (per unit)

- KAT passes on >= (nEngines - 2) engines (SMART masking covers stragglers)
- Hashrate within 5% of bin label at reference V/f
- Leak/flow/thermal alarm paths exercised (forced-sensor selftest)
