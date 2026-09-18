# BENCHMARKS.md -- 對標設計 (named-benchmark tracking)

| ID | Metric | Benchmark | Method | Current | Target | Due |
|----|--------|-----------|--------|---------|--------|-----|
| B1 | Energy efficiency | Intel BZM2 26 J/TH | gate-level sim + SAIF (calibrate tapeout/energy_model.py) | ~416 std-cell 28nm / ~200 fc-28nm / ~75 fc-16nm-nearVt (Model A est; see docs/ENERGY_MODEL_RECONCILIATION.md) | 28nm: heat-reuse only; 16nm: <=100 gates Line C | D+90 |
| B2 | Chip hashrate | BZM2 0.137 TH/s | RTL sim throughput | 0.008 TH/s (8 eng @1GHz) | >=0.15 | D+180 |
| B3 | System openness | Block Proto (fleet SW + SV2) | feature checklist | SoC+stratum V1 | fleet mgr + SV2 | D+180 |
| B4 | Engine fault tolerance | Auradine SMART | fault-injection demo | per-engine isolation (RTL ready) | firmware demo | D+180 |
| B5 | Correctness | self-imposed | chiseltest + genesis KAT | 9 bugs fixed, all green | stay green every release | always |
| B6 | End-to-end openness | unique positioning | % silicon artifacts public | 100% (this repo) | hold | always |

## B1 kill criterion
If the D+90 gate-level number at 28nm is > 40 J/TH -> drop product Line C
(open mining appliance). Lines A (dev kit) and B (PoW research) continue
regardless -- they do not depend on efficiency.

## B2 notes
0.15 TH/s needs ~32-64 engines @ 1-2 GHz or CSA-stage 2GHz operation;
drives the floorplan (engine array = sea-of-macro tiling) and package
power/thermal co-design.

## Measurement hygiene
- Every efficiency claim must cite: node, voltage, frequency, toggle
  source (SAIF vs default), temperature corner.
- Efficiency numbers without those five fields are marketing, not data.
