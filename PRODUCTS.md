# PRODUCTS.md -- product catalog & roadmap

2026-10 v1.5.0 baseline (supersedes 2026-09 where marked). Numbers are
planning estimates, marked ~. Gates are quantitative; a missed gate kills
or re-scopes the line, not the company. See BUSINESS.md (v1.5.0) for the
market rationale -- 2026 Q3 invalidated the "openness sells at an
efficiency penalty" premise (Block Proto's 15 EH/s order was terminated
by the customer) and validated hash-to-heat (Canaan Nordic bid, MARA
Finland) and counter-cyclical education demand.

## 1. Product catalog

### Line A -- Research / Education Dev Kit  (cash engine, PRIMARY, counter-cyclical)

| SKU | Form | Silicon | Specs (target) | Price | Customer |
|-----|------|---------|----------------|-------|----------|
| A-FPGA | PCIe/USB3 FPGA board | VCU118-class | 8-20 engines, 3-4 Ghash/s, full MMIO + ILA | $399-599 | universities, labs, hobbyists |
| A-28 | ASIC dev board | 28nm shuttle chip | Rocket + 8 engines, genesis-KAT certified | $899-1,499 | researchers teaching real silicon |
| A-16 | ASIC dev board | 16nm | + near-Vt, DVFS sweep tooling | $1,499-2,499 | advanced labs, sovereign/audit buyers, second-life rig refurbishers |

Value prop: the ONLY end-to-end open mining SoC (math -> RTL -> driver ->
stratum -> silicon). Includes course materials, golden models, chiseltest
suites. **[v1.5.0]** Repositioned as the open-silicon teaching vehicle
for the AI era (pipeline RTL + DVFS + thermal firmware + stratum = one
Chipyard/RISC-V course in a box). Zero overlap with Bitmain.

### Line B -- PoW Research Platform  (strategic option, strengthened)

| SKU | Form | Specs | Price | Customer |
|-----|------|-------|-------|----------|
| B-1 | SoM + LPDDR4 | Rocket/Boom + SHA-256d engines + memory-hard PoW engine (Scrypt-class scratchpad 128KB/core), firmware SDK for new PoW algos | $2,499-4,999 | PoW algorithm designers, security researchers, sovereign chains |

Value prop: retargetable PoW hardware in SoC form. Nobody else sells this.
**[v1.5.0]** Catalysts added: post-quantum migration research, memory-hard
PoW, sovereign-chain custom algorithms. Risk unchanged: market is small
and research-budget-driven.

### Line C -- Hash-to-Heat Equipment  (v1.5.0 RENAMED from "Open Mining Appliance"; conditional, gated)

| SKU | Form | Specs | Price | Customer |
|-----|------|-------|-------|----------|
| C-1 | Modular 2U liquid-cooled unit; 128-chip cold plates, UQD, leak tray, BMC; heat-reuse loop 55-60C outlet (docs/LIQUID_COOLING.md) | 16nm fc, near-Vt; Line A DVFS; SMART-style per-engine skip; thermal_policy() heat-follow mode in dvfs_daemon; open fleet mgmt + SV2 | BOM+30% | **district-heating operators, greenhouses, DHW pre-heat, off-grid/stranded-heat sites** |

Value prop: **[v1.5.0]** NOT "a worse Antminer for industrial miners"
(that market is capitulating -- see BUSINESS.md 1.0). We sell HEATING
equipment whose byproduct is BTC: heat-revenue-first economics (12-yr
heat-purchase agreements per the Mintgreen model), 55-60C outlet,
COP-equivalent >1 vs resistive heating. Market proof: Canaan won a
Nordic district-heating bid (2026-05); MARA integrated 2 Finland systems
in <30 days; 90% of global district heating is still fossil.
Differentiation vs Canaan/Bitmain entering the same market: end-to-end
auditable open silicon -- decisive for public procurement. GATED: only
if A1+A3 measure <=120 J/TH at gate level (revised from <=100; heat mode
relaxes the bar, <=100 remains the stretch). Mining-only revenue models
for Line C are explicitly OUT.

### Line A2 -- AI option (Phase-2 gated, see docs/AI_ACCELERATION.md)

Tier 1 (ML for DVFS/predictive-maintenance/thermal-MPC) ships with A-16
firmware at no hardware cost. Tier 2 (Gemmini NPU on-die) is a config
option gated at the A-16 freeze: enables Line A to sell as the only
two-accelerator open SoC dev kit. Tier 3 (AI chip product line) is
explicitly out of scope.

### Line D -- Services & Licensing  (margin, zero inventory; v1.5.0 expanded)

- Training/workshops on open mining-silicon flow (universities, 2-5 days).
- Custom engine variants (alt-PoW, different algorithms) as contract work.
- **[v1.5.0]** DVFS / thermal-MPC firmware (sw/dvfs_daemon.c) LICENSED to
  second-life rig refurbishers and hosted-mining operators: the 2026
  shakeout leaves fleets of S19/S21-class hardware that needs derating,
  longevity tuning and thermal-aware throttling -- exactly what our
  near-Vt + bandit-DVFS stack does.
- **[v1.5.0]** Hash-to-heat system-integration consulting (cold plate,
  CDU, control policy -- docs/LIQUID_COOLING.md is the deliverable skeleton).
- NOTE: core repo stays open (its value IS openness); services sell labor,
  not the RTL.

## 2. Roadmap (2026 Q4 -> 2029, v1.5.0)

### Phase 0 -- Prove the foundation  (2026 Q4)  [IN PROGRESS]
- chiseltest green on v10 kit (Sha256Pipe / MiningEngine / CSA sketch)
- Verilator genesis smoke (miner_baremetal) on RocketMiningConfig
- FireSim boot + stratum testnet connect via UART proxy
- B1-B5 baselines recorded in BENCHMARKS.md
- GitHub published, v1.4.0 release + Apache-2.0 license detect [DONE]
- **Gate G0 (2026-12)**: all of the above green, else Phase 1 slips one
  quarter.

### Phase 1 -- Line A-FPGA ships  (2027 H1)
- A-FPGA board bring-up doc (fpga/ templates already in repo), ILA bring-up
  recipes, 24h genesis soak
- Public docs site: courseware derived from this repo's README/docs,
  repositioned for AI-era chip education
- Energy model calibration: PTPX/SAIF numbers replace E_round estimates
  (kills the last optimistic assumptions)
- 28nm shuttle submission (validation vehicle; Caravel wrapper sketch in
  tapeout/)
- **Gate G1 (2027-06)**: A-FPGA >= 20 paid pre-orders from >= 5
  institutions; energy model measured-vs-estimated within 2x. Fail ->
  Line A-28 re-scoped to FPGA-only + services.

### Phase 2 -- Silicon dev kit + hash-to-heat pilot (2027 H2)
- A-28 silicon back, bring-up (genesis KAT on real silicon = marketing gold)
- A-16 shuttle submission with full-custom CSA core + near-Vt corners
  (hw/FullCustomStage.scala + docs/LOW_POWER_DESIGN.md sign-off plan)
- B-1 architecture spec frozen (LPDDR controller integration, memory-hard
  engine variant)
- **[v1.5.0]** Hash-to-heat pilot on COMMERCIAL rigs (does not wait for
  our silicon): 50-100 kW demo with 1-2 district-heating/greenhouse
  operators using ore control firmware (thermal_policy heat-follow mode)
  to prove the heat economics and collect field data for C-1 design.
- **Gate G2 (2027-12)**: A-28 silicon passes KAT + HTOL 500h; A-16
  pre-simulation <= 120 J/TH (trend toward the <=100 stretch);
  **[v1.5.0]** >= 1 PAID LOI for the heat pilot. Fail -> Line C
  formally cancelled, focus A + B + D.

### Phase 3 -- Scale the winners  (2028)
- A-16 ship; DVFS sweep tooling productized (sw/dvfs_daemon.c + fleet UI);
  first Line D firmware-licensing deal for second-life fleets
- B-1 prototype to 2 design partners (PoW research)
- Line C-1 pilot: 100-unit hash-to-heat field trial with our 16nm silicon
  IF G3 passes
- **Gate G3 (2028-06)**: A-16 rail-measured <= 120 J/TH AND heat-pilot
  LCOE beats resistive heating by >= 20%. Fail -> C stays cancelled;
  A+B+D carry the company.

### Phase 4 -- Platformization  (2029, v1.5.0 new)
- Open mining SoC becomes the reference design for auditable heat
  infrastructure (the BZM2-of-heat-reuse position)
- Line B first-mover if a post-quantum PoW migration starts
- Licensing/services revenue >= hardware revenue (zero-inventory margin)

## 3. Resource plan (per phase)

| Phase | HW eng | SW/firmware | Verification | Ops/docs | Cash need |
|-------|--------|-------------|--------------|----------|-----------|
| P0 | 1 | 1 | 1 | 0.5 | <$50k (existing tools) |
| P1 | 1.5 | 1 | 1 | 1 | ~$150k (boards, shuttle, docs) |
| P2 | 3 | 1.5 | 1.5 | 1 | ~$600k (16nm shuttle+NRE; pilot demo rigs ~$30k line) |
| P3 | 3 | 2 | 1 | 2 | revenue-funded |
| P4 | 2 | 2 | 0.5 | 2 | revenue-funded |

## 4. Revenue model (rough, 2027-2029, v1.5.0)

- 2027: A-FPGA 150 units @ $499 = ~$75k + workshops $40k
- 2028: A-16 300 units @ $1,999 = ~$600k + B-1 40 units @ $3,499 = $140k
  + services/licensing $150k (incl. first second-life firmware deal).
  Line C excluded until G3.
- 2029: Line C-1 200 units BOM+30% if G3 passed, else licensing-only;
  services become the margin engine.

## 5. Risk register (top 6, v1.5.0)

| Risk | P | Mitigation |
|------|---|------------|
| BTC < $70k / hashprice at record lows persists | H | Base case; revenue independent of mining economics (education, heat, licensing) |
| Open 16nm PDK/shuttle unavailable | M | G2 fallback: 28nm-only roadmap; heat pilots don't need our silicon |
| DFT/scan maturity in open tools | M | Fund DFT early (P1); commercial sign-off as backup budget line |
| Canaan/Bitmain push into hash-to-heat and squeeze Line C | M | Differentiate on auditability (public procurement); fallback to licensing the thermal/DVFS stack, not boxes |
| Energy numbers stay >= 2x off model | M | G1 gate; business lives on A+B+D, not on efficiency |
| Key person dependency | M | Docs-first culture (this repo IS the bus factor hedge) |
