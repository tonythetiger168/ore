# BUSINESS.md -- 2026 mining-hardware landscape, positioning & strategy

Last updated: 2026-09. Figures from public spec sheets / SEC filings /
retail listings; ours are engineering estimates (marked ~).

## TL;DR

The 2026 efficiency ladder runs 9.5 -> 26 J/TH. We sit at ~60-100 J/TH.
We will never win SHA-256 on efficiency. But the field proves openness is
a monetizable feature (Block Proto: worse J/TH than Bitmain, 15 EH/s of
orders anyway) and open silicon has a living benchmark (Intel BZM2, 26
J/TH). Strategy: become the best OPEN platform, with Block-Proto-style
system openness + BZM2-class efficiency as the engineering target.

## 1. The 2026 field (all players)

| Player / model        | TH/s    | J/TH  | Node | Notes |
|-----------------------|---------|-------|------|-------|
| Bitmain S23 Hyd 3U    | 1,160   | 9.5   | 3nm  | flagship, Jan 2026 |
| Bitmain S23 Hyd       | 580     | 9.5   | 3nm  |     |
| Bitmain S23           | 318     | 11.0  | 3nm  | air |
| Auradine Teraflux ng  | 240-900 | 9.8-11| 3nm  | US-made, samples Q2'26, ship Q3'26 |
| Bitmain S21 XP Hyd    | 473     | 12.0  | 3nm  |     |
| Canaan A16 XP         | 300     | 12.8  |      | holds spec to 35C ambient |
| MicroBT M70S          | 226-264 | ~13.5 |      | best air efficiency |
| Bitmain S21 Pro       | 234     | 15.0  | 5nm  | still shipping |
| Auradine AH3880       | 600     | ~14.5 | 3nm  | SMART redundant hashboards |
| Block Proto Rig       | 819     | 14.65 | 3nm  | OPEN ARCHITECTURE: tool-free hashboards, open fleet SW, Stratum V2; Core Scientific ~15 EH/s |
| MicroBT M70/M73       | 214-526 | ~14.5 |      | M73 hydro trades efficiency for density |
| MicroBT M66S          | 298     | 18.0  |      |     |
| Canaan Avalon Q       | 90      | 18.6  |      | home/quiet segment |
| Canaan Mini 3         | 37.5    | 21.3  |      | $799 home miner |
| Canaan Nano 3S        | 6       | 23.3  |      | $249, 33-40 dB |
| Intel BZM2 (open)     | 0.137/chip | 26 |      | OPEN SILICON benchmark, 2.5W/chip |
| Bitaxe/Nerdaxe        | ~1-3    | ~15-20|      | hobbyist, BM1370 + open firmware |
| **Ours (FPGA / 28nm)** | **~0.003-0.008** | **see model below** | FPGA/28nm | **only fully-open SoC reference** |

## 2. What the field teaches

1. **Openness sells at an efficiency penalty.** Block Proto: 14.65 J/TH
   (worse than S23's 11), yet won a ~15 EH/s deployment on modularity,
   open fleet software and Stratum V2. Market tolerates ~30-40% efficiency
   penalty for operability/transparency.
2. **Open silicon has a benchmark: Intel BZM2 at 26 J/TH.** Below that, an
   open chip is a curiosity; at/above it, it becomes the reference design
   for researchers and sovereign/minimum-trust deployments.
3. **Per-chip fault tolerance is a differentiator** (Auradine SMART: keeps
   running with dead chips). Our per-engine independence gives this FOR
   FREE -- architecturally we are already there; it needs only firmware.
4. **The consumer/home segment is thin** (Canaan Nano 3S: $249, 23.3 J/TH)
   and contested. Avoid unless bundled with heat-reuse value.
5. **Efficiency ladder = capital ladder.** 9.5 J/TH needs 3nm + huge NRE;
   26 J/TH is reachable at 28nm-class with full-custom design; 60-100 J/TH
   is standard-cell prototyping. Pick the rung you can afford, then own it.

## 3. Our position

| Dimension                | vs Bitmain/MicroBT/Canaan | vs Block Proto | vs Intel BZM2 |
|--------------------------|---------------------------|----------------|---------------|
| Efficiency               | lose 6-10x                | lose 4-7x      | lose 2.5-4x   |
| Openness of silicon      | win (they are closed)     | win (their ASIC is closed) | tie-ish (ours is fully open + auditable end-to-end) |
| System openness          | win vs stock; Braiins partially opens Avalon | tie (adopt their playbook) | win (we have full SoC + Linux) |
| Algorithm flexibility    | win                       | win            | win           |
| Ops/fleet software       | lose badly                | lose (build it)| tie (both minimal) |
| Volume cost              | lose badly                | lose           | lose          |

## 4. Improvement plan (revised against named benchmarks)

### A. Engineering -- model-based targets (run tapeout/energy_model.py)

Key relation: **J/TH = 128 x E_round[pJ]**. All anchors back-solve to a
single energy-per-round number; the whole efficiency game is that number.

| scenario (E_round est.)             | J/TH  | market threshold crossed |
|-------------------------------------|-------|--------------------------|
| std-cell 28nm                       | ~416  | none                     |
| std-cell + operand isolation        | ~300  | heat-reuse only          |
| full-custom CSA 28nm                | ~200  | heat-reuse only          |
| full-custom 16nm                    | ~117  | heat-reuse only          |
| full-custom 16nm near-threshold     | ~75   | S9-class; still 8x behind S23 |

| # | Action                                            | Benchmark |
|---|---------------------------------------------------|-----------|
| A1| Full-custom SHA-256 round (CSA tree, hand-placed) | enables 28nm ~200 J/TH |
| A2| Node 28nm -> 16nm open PDK + near-Vt operation    | ~75 J/TH ceiling |
| A3| DVFS + per-engine clock gating + SMART-style fault-tolerant firmware | Auradine-style reliability |
| A4| Stratum V2                                        | see sw/sv2_protocol.md + skeleton |
| A5| Done: midstate/state3, verified endianness, KAT   | --        |

REVISED kill criteria (supersedes the old 40 J/TH line):
- 28nm any style is a VALIDATION vehicle only; Line C at 28nm = heat-reuse.
- Line C mining appliance requires 16nm-class <= ~100 J/TH in gate-level
  simulation, else it stays a dev kit / research product.
- D+90 gate: replace E_round estimates with measured numbers (PTPX or
  open equivalent) before any go/no-go.

### B. Product lines (unchanged spine, sharper differentiation)

- **Line A: Research/education dev kit** -- FPGA board now, ASIC later,
  $300-800. The only fully-open mining SoC in existence is the pitch.
- **Line B: PoW research platform** -- SoC + LPDDR for memory-hard/new
  PoW. Nobody else in the table can retarget algorithms at all.
- **Line C: Open-architecture mining appliance** -- copy Block Proto's
  playbook (modular boards, open fleet mgmt, SV2) but with OPEN SILICON
  as the moat. Only if A1+A3 meet kill criteria. Differentiator vs Proto:
  auditable supply chain, no closed ASIC.

### C. Ops software catch-up (biggest gap vs Block/Auradine)

1. Fleet manager: per-engine hashrate/temperature telemetry, V/f autotune,
   pool failover (port cgminer/braiins concepts to our MMIO map).
2. Stratum V2 (A4): encrypted/authenticated pool protocol, better latency.
3. Redundancy firmware: engine hot-skip on fault (SMART-style) -- the RTL
   already isolates engines; firmware just masks the dead one.

## 5. Milestones

(Expanded into the full product catalog and gated roadmap in PRODUCTS.md.) (leveraging the v5 kit)

- D+30 : chiseltest green (RocketMiningConfig); Verilator genesis smoke;
         stratum client testnet connect via UART proxy.
- D+90 : FPGA genesis soak clean; A1 netlist + gate-level energy numbers
         CALIBRATE tapeout/energy_model.py -> Line C go/no-go vs the
         revised 16nm/<=100 J/TH kill line.
- D+180: 16nm shuttle application; dev-kit FPGA boards to 3 pilot
         universities (Line A pre-orders); SV2 client beta.

## 6. Risks

- Efficiency ladder keeps moving (S24-class ~8 J/TH rumored): the gap at
  every rung widens; niche economics stay fragile.
- Open PDK / shuttle access for 16nm unconfirmed; 28nm fallback keeps us
  at BZM2-class at best.
- Block Proto could open more of its stack, eroding Line C differentiation.
- Open-source DFT maturity remains the tapeout schedule risk.
- Jurisdictional restrictions on mining hardware imports (Line C).
