# PRODUCTS.md -- product catalog & roadmap

2026-09 baseline. Numbers are planning estimates, marked ~. Gates are
quantitative; a missed gate kills or re-scopes the line, not the company.
See BUSINESS.md for market rationale and BENCHMARKS.md for metrics.

## 1. Product catalog

### Line A -- Research / Education Dev Kit  (cash engine, PRIMARY)

| SKU | Form | Silicon | Specs (target) | Price | Customer |
|-----|------|---------|----------------|-------|----------|
| A-FPGA | PCIe/USB3 FPGA board | VCU118-class | 8-20 engines, 3-4 Ghash/s, full MMIO + ILA | $399-599 | universities, labs, hobbyists |
| A-28 | ASIC dev board | 28nm shuttle chip | Rocket + 8 engines, genesis-KAT certified | $899-1,499 | researchers teaching real silicon |
| A-16 | ASIC dev board | 16nm | + near-Vt, DVFS sweep tooling | $1,499-2,499 | advanced labs, sovereign/audit buyers |

Value prop: the ONLY end-to-end open mining SoC (math -> RTL -> driver ->
stratum -> silicon). Includes course materials, golden models, chiseltest
suites. Differentiation: auditability + pedagogy; zero overlap with Bitmain.

### Line B -- PoW Research Platform  (strategic option)

| SKU | Form | Specs | Price | Customer |
|-----|------|-------|-------|----------|
| B-1 | SoM + LPDDR4 | Rocket/Boom + SHA-256d engines + memory-hard PoW engine (Scrypt-class scratchpad 128KB/core), firmware SDK for new PoW algos | $2,499-4,999 | PoW algorithm designers, security researchers, chains |

Value prop: retargetable PoW hardware in SoC form. Nobody else sells this.
Risk: market is small and research-budget-driven.

### Line C -- Open Mining Appliance  (conditional, gated)

| SKU | Form | Specs | Price | Customer |
|-----|------|-------|-------|----------|
| C-1 | Modular 2U liquid-cooled unit; 128-chip cold plates, UQD, leak tray, BMC; heat-reuse loop 55-60C outlet (docs/LIQUID_COOLING.md) | 16nm fc, near-Vt; Line A DVFS; SMART-style per-engine skip; thermal_policy() in dvfs_daemon; open fleet mgmt + SV2 | BOM+30% | heat-reuse / off-grid / audit-sensitive hosting |

Value prop: Block-Proto-style openness WITH open silicon. GATED: only if
Line A-16 measures <= 100 J/TH at rail (see kill criteria). Positioned at
heat-reuse economics, not grid mining.

### Line D -- Services & Licensing  (margin, zero inventory)

- Training/workshops on open mining-silicon flow (universities, 2-5 days).
- Custom engine variants (alt-PoW, different algorithms) as contract work.
- NOTE: core repo stays open (its value IS openness); services sell labor,
  not the RTL.

## 2. Roadmap (2026-09 -> 2028)

### Phase 0 -- Prove the foundation  (2026 Q3-Q4)  [IN PROGRESS]
- chiseltest green on v10 kit (Sha256Pipe / MiningEngine / CSA sketch)
- Verilator genesis smoke (miner_baremetal) on RocketMiningConfig
- FireSim boot + stratum testnet connect via UART proxy
- B1-B5 baselines recorded in BENCHMARKS.md
- **Gate G0**: all of the above green, else Phase 1 slips one quarter.

### Phase 1 -- Line A-FPGA ships  (2027 H1)
- A-FPGA board bring-up doc (fpga/ templates already in repo), ILA bring-up
  recipes, 24h genesis soak
- Public docs site: courseware derived from this repo's README/docs
- Energy model calibration: PTPX/SAIF numbers replace E_round estimates
  (kills the last optimistic assumptions)
- 28nm shuttle submission (validation vehicle; Caravel wrapper sketch in
  tapeout/)
- **Gate G1 (2027-06)**: A-FPGA >= 20 paid pre-orders from >= 5 institutions;
  energy model measured-vs-estimated within 2x. Fail -> Line A-28 re-scoped
  to FPGA-only + services.

### Phase 2 -- Silicon dev kit  (2027 H2)
- A-28 silicon back, bring-up (genesis KAT on real silicon = marketing gold)
- A-16 shuttle submission with full-custom CSA core + near-Vt corners
  (hw/FullCustomStage.scala + docs/LOW_POWER_DESIGN.md sign-off plan)
- B-1 architecture spec frozen (LPDDR controller integration, memory-hard
  engine variant)
- **Gate G2 (2027-12)**: A-28 silicon passes KAT + HTOL 500h; A-16
  pre-simulation <= 120 J/TH (trend toward the <= 100 Line C bar).
  Fail -> Line C formally cancelled, focus A + B + D.

### Phase 3 -- Scale the winners  (2028)
- A-16 ship; DVFS sweep tooling productized (sw/dvfs_daemon.c + fleet UI)
- B-1 prototype to 2 design partners (PoW research)
- Line C pilot: 50-unit heat-reuse field trial IF G3 passes
- **Gate G3 (2028-06)**: A-16 rail-measured <= 100 J/TH AND heat-reuse
  pilot LCOE beats resistive heating by >= 20%. Fail -> C stays cancelled;
  A+B+D carry the company.

## 3. Resource plan (per phase)

| Phase | HW eng | SW/firmware | Verification | Ops/docs | Cash need |
|-------|--------|-------------|--------------|----------|-----------|
| P0 | 1 | 1 | 1 | 0.5 | <$50k (existing tools) |
| P1 | 1.5 | 1 | 1 | 1 | ~$150k (boards, shuttle, docs) |
| P2 | 3 | 1.5 | 1.5 | 1 | ~$600k (16nm shuttle+NRE) |
| P3 | 3 | 2 | 1 | 2 | revenue-funded |

## 4. Revenue model (rough, 2027-2028)

- 2027: A-FPGA 150 units @ $499 = ~$75k + workshops $40k
- 2028: A-16 300 units @ $1,999 = ~$600k + B-1 40 units @ $3,499 = $140k
  + services $150k. Line C excluded until G3.

## 5. Risk register (top 5)

| Risk | P | Mitigation |
|------|---|------------|
| Open 16nm PDK/shuttle unavailable | M | G2 fallback: 28nm-only roadmap; Line C cancelled by construction |
| DFT/scan maturity in open tools | M | Fund DFT early (P1); commercial sign-off as backup budget line |
| Bitmain/Auradine open more of their stack | L-M | Line A/B (education/research) are structurally insulated |
| Energy numbers stay >= 2x off model | M | G1 gate; business lives on A+B+D, not on efficiency |
| Key person dependency | M | Docs-first culture (this repo IS the bus factor hedge) |
