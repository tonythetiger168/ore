# REGRESSION.md -- offline verification attestation (v1.3.6). All 10 suites green.

Environment: sandbox, no network, no chipyard. Everything that CAN be
verified without silicon/chipyard IS verified below. The remaining chain
(chiseltest -> elaboration -> genesis smoke) requires the user's machine
(see docs/CHIPYARD_INTEGRATION.md).

| suite | what it proves | result |
|-------|----------------|--------|
| golden.py (sha256/midstate/state3/genesis) | PASS |
| csa_model.py (CSA exactness) | PASS |
| energy_model.py | PASS |
| stratum cross-check (7 checks) | PASS |
| dvfs_daemon (pareto+derate+thermal) | PASS |
| ml_on_miner (bandit+EWMA) | PASS |
| thermal_mpc (MPC beats linear) | PASS |
| stratum_guard (0 false alarm) | PASS |
| sv2 skeleton compiles | PASS |
| miner_baremetal compiles | PASS |
| scala brace balance (hw/) | code-only (comments/strings stripped): balanced; raw-count delta traced to a paren inside a comment -- false positive | PASS |
| test-vector JSON validity | vectors/genesis_kat parse | PASS |

Note: stratum cross-check includes hashlib ground truth (merkle fold,
header assembly, share target, dsha KAT). See conversation history for
the detailed expected values.
