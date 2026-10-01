# BUSINESS.md -- 2026 mining-hardware landscape, positioning & strategy

Last updated: 2026-10 (v1.5.0 market revision). Figures from public spec
sheets / SEC filings / retail listings; ours are engineering estimates
(marked ~). Where the 2026-09 survey was overtaken by events, the 2026-10
correction is marked **[v1.5.0]**.

## TL;DR

The 2026 efficiency ladder runs 9.5 -> 26 J/TH. We sit at ~60-100 J/TH.
We will never win SHA-256 on efficiency. **[v1.5.0]** The 2026-09 premise
"openness sells at a 30-40% efficiency penalty (Block Proto's 15 EH/s)"
is REVERSED: Core Scientific paid $41.9M in Q2 2026 to TERMINATE the
Proto contract as industrial self-mining capitulated (hashprice ~$28-30,
miners ~$80k/BTC cash cost vs ~$68-70k price). Openness is a necessary
condition, not a business model -- you cannot sell into a market that is
losing money. What the market DID validate in 2026: (a) hash-to-heat
(Canaan won a Nordic district-heating bid; MARA Finland integrated 2
systems <30 days), and (b) counter-cyclical demand for open silicon
education/research as miners pivot to AI/HPC. Strategy: become the best
OPEN platform, reposition Line C from "open mining appliance" to
"hash-to-heat equipment", and ride the two markets that are growing.

## 1. The 2026 field (all players)

### 1.0 **[v1.5.0]** 2026 Q3 shakeout -- the market moved

| Signal | Data point | Source |
|---|---|---|
| Hashprice | ~$28-30 /PH/s/day, post-halving all-time low | CoinShares Q1 2026 |
| Miner economics | weighted cash cost ~$79,995/BTC vs price ~$68-70k -> losing ~$10-19k per coin | CoinShares Q4 2025 |
| Network hashrate | ~1,160-1,300 EH/s peak -> ~920 EH/s; three consecutive negative difficulty adjustments | Hashrate Index / CoinShares |
| AI/HPC pivot | $70B+ AI/HPC contracts signed by listed miners; AI ~30% of revenue, est. 70% by end-2026 | CoinShares |
| Block Proto collapse | Core Scientific paid $41.9M to terminate the Proto contract (~15 EH/s of 3nm deliveries canceled); self-mining gross margin -56%; exiting self-mining | CORZ 10-Q 2026-06-30 |
| Other exits | Keel ceased Bitcoin mining entirely 2026-06 (Q2 gross margin -285%) | CoinShares 2026-09 |
| Efficiency frontier | S23 Hyd 9.5 J/TH shipping ($15k); Auradine 3rd-gen Teraflux 9.8 J/TH volume Q3 2026 ($153M Series C); Bitdeer A3 sub-10 J/TH H1 2026 | spec sheets / PR |
| Heat-reuse validation | Canaan won competitive Nordic district-heating hash-to-heat bid (2026-05); MARA Finland 2 district-heating systems integrated in <30 days (MW-scale); Mintgreen 12-yr heat-purchase agreement (7,000 homes) | PR / District Energy mag |
| Market size | crypto-mining hardware +$12.05B 2023-2027 (CAGR 11.35%, NA 41%); overall mining $2.2B(2024)->$3.3B(2030), CAGR 6.9% | Technavio / R&M |

### 1.1 Player table (updated 2026-10)

| Player / model        | TH/s    | J/TH  | Node | Notes |
|-----------------------|---------|-------|------|-------|
| Bitmain S23 Hyd 3U    | 1,160   | 9.5   | 3nm  | flagship, Jan 2026 |
| Bitmain S23 Hyd       | 563-580 | 9.5   | 3nm  | $15k listing |
| Bitmain S23 (air)     | 305-318 | 11.0  | 3nm  | EUR 7,499 EU listing |
| Auradine Teraflux 3g  | --      | 9.8   | 3nm  | samples Q2'26, volume Q3'26; SMART hashboards |
| Bitdeer SEALMINER A3  | --      | <10   | --   | volume H1 2026 |
| Bitmain S21 XP Hyd    | 473     | 12.0  | 3nm  |     |
| Canaan A16 XP         | 300     | 12.8  | --   | holds spec to 35C ambient |
| MicroBT M70S          | 226-264 | ~13.5 | --   | best air efficiency |
| Block Proto Rig **[v1.5.0: anchor customer exited]** | 819 | 14.1 | 3nm | 15 EH/s order CANCELED 2026 Q2 ($41.9M termination fee PAID by customer); open fleet SW + SV2 remains the playbook reference |
| Canaan Avalon Q       | 90      | 18.6  | --   | home/quiet segment |
| Canaan Mini 3         | 37.5    | 21.3  | --   | $799 home miner |
| Canaan Nano 3S        | 6       | 23.3  | --   | $249, 33-40 dB -- home segment entry is CLOGGED |
| Intel BZM2 (open)     | 0.137/chip | 26 | --  | OPEN SILICON benchmark, 2.5W/chip (discontinued; reference value endures) |
| Bitaxe/Nerdaxe        | ~1-3    | ~15-20| --   | hobbyist, BM1370 + open firmware |
| **Ours (FPGA / 28nm)** | **~0.003-0.008** | **see model below** | FPGA/28nm | **only fully-open SoC reference** |

## 2. What the field teaches (2026-10 revision)

1. **[v1.5.0] "Openness sells at an efficiency penalty" is REFUTED as a
   standalone thesis.** Block Proto: 14.1 J/TH, and its ~15 EH/s order was
   canceled by the customer (who paid $41.9M to walk away) when self-mining
   margins went deeply negative. Lesson: openness is a FEATURE buyers want,
   but only in markets that make money. Sell openness where the buyer's
   economics do not depend on hashprice.
2. **Open silicon has a benchmark: Intel BZM2 at 26 J/TH.** Below that, an
   open chip is a curiosity; at/above it, it becomes the reference design
   for researchers and sovereign/minimum-trust deployments. (Unchanged.)
3. **Per-chip fault tolerance is a differentiator** (Auradine SMART: keeps
   running with dead chips; raised $153M in 2026). Our per-engine
   independence gives this FOR FREE -- architecturally we are already
   there; it needs only firmware. **[v1.5.0]** New adjacent market:
   second-life/refurbished rigs (S19/S21-class) need derating & longevity
   firmware -- our DVFS stack fits.
4. **The consumer/home segment is CLOSED** (Canaan Nano 3S: $249, 23.3
   J/TH occupies the entry). Avoid unless bundled with heat-reuse value.
5. **Efficiency ladder = capital ladder.** (Unchanged.) **[v1.5.0]** Add:
   in heat-reuse mode the ladder INVERTS -- heat is the product, BTC the
   byproduct, and a 55-60C outlet at COP-equivalent >1 beats resistive
   heating regardless of J/TH. This is Line C's true market.
6. **[v1.5.0] Mining is not dying, it is MORPHING**: US hashrate share
   still +~2pp/qtr; survivors move to stranded/intermittent power
   (ERCOT load-balancing, 10MW containerized), hosted mining fills the gap,
   and district-heating operators become hardware buyers. The buyer of
   mining silicon in 2027+ is increasingly a HEAT operator or a
   research/education institution, not an industrial miner.

## 3. Our position

| Dimension                | vs Bitmain/MicroBT/Canaan | vs Block Proto | vs Intel BZM2 |
|--------------------------|---------------------------|----------------|---------------|
| Efficiency               | lose 6-10x                | lose 4-7x      | lose 2.5-4x   |
| Openness of silicon      | win (they are closed)     | win (their ASIC is closed) | tie-ish (ours is fully open + auditable end-to-end) |
| System openness          | win vs stock; Braiins partially opens Avalon | tie (adopt their playbook) | win (we have full SoC + Linux) |
| Algorithm flexibility    | win                       | win            | win           |
| Ops/fleet software       | lose badly                | lose (build it)| tie (both minimal) |
| Volume cost              | lose badly                | lose           | lose           |
| **[v1.5.0]** Heat-reuse system design (cold plate/CDU/control policy) | win (docs/LIQUID_COOLING.md + dvfs thermal_policy) | tie | win |
| **[v1.5.0]** Auditability for public-procurement buyers | win | partial (closed ASIC) | win |

## 4. Improvement plan (revised against named benchmarks)

Key relation: **J/TH = 128 x E_round[pJ]**. (unchanged)

| scenario (E_round est.)             | J/TH  | market threshold crossed |
|-------------------------------------|-------|--------------------------|
| std-cell 28nm                       | ~416  | none                     |
| std-cell + operand isolation        | ~300  | heat-reuse only          |
| full-custom CSA 28nm                | ~200  | heat-reuse only          |
| full-custom 16nm                    | ~117  | heat-reuse viable        |
| full-custom 16nm near-threshold     | ~75   | S9-class; heat-reuse comfortable |

| # | Action                                            | Benchmark |
|---|---------------------------------------------------|-----------|
| A1| Full-custom SHA-256 round (CSA tree, hand-placed) | enables 28nm ~200 J/TH |
| A2| Node 28nm -> 16nm open PDK + near-Vt operation    | ~75 J/TH ceiling |
| A3| DVFS + per-engine clock gating + SMART-style fault-tolerant firmware | Auradine-style reliability + second-life rig market |
| A4| Stratum V2                                        | see docs/stratum_v2.md + skeleton |
| A5| Done: midstate/state3, verified endianness, KAT   | --        |

Kill criteria **[v1.5.0 revised]**:
- 28nm any style is a VALIDATION vehicle only; Line C at 28nm = heat-reuse
  pilot only.
- Line C hash-to-heat appliance requires 16nm-class <= ~120 J/TH at
  gate-level simulation (RELAXED from <=100: in heat-reuse mode the
  economics are dominated by heat revenue, not J/TH; <=100 remains the
  stretch target).
- D+90 gate: replace E_round estimates with measured numbers (PTPX or
  open equivalent) before any go/no-go. (unchanged)

## 5. Product-line strategy **[v1.5.0 repositioning]**

- **Line A: Research/education dev kit** -- FPGA board now, ASIC later,
  $300-800. The only fully-open mining SoC in existence is the pitch.
  **[v1.5.0]** Counter-cyclical strengthening: as miners pivot to AI/HPC,
  chip-education demand (RISC-V/Chipyard skills) rises; reposition as the
  open-silicon teaching vehicle for the AI era. Add sovereign/audit
  buyers (strategic BTC reserve narratives, auditable supply chains).
- **Line B: PoW research platform** -- SoC + LPDDR for memory-hard/new
  PoW. Nobody else in the table can retarget algorithms at all.
  **[v1.5.0]** New catalysts: post-quantum migration research,
  memory-hard PoW, sovereign chains.
- **Line C: Hash-to-heat equipment** **[v1.5.0: RENAMED from "open
  mining appliance"]** -- customers are district-heating operators,
  greenhouses, DHW pre-heat, off-grid/stranded-heat sites -- the buyers
  Canaan (Nordic bid) and MARA (Finland) just proved exist. Copy Block
  Proto's playbook (modular boards, open fleet mgmt, SV2) for the SYSTEM,
  but the moat is auditable open silicon for public-procurement buyers.
  Only if A1+A3 meet the revised kill criteria. We do NOT compete with
  S23-class grid mining -- ever.

## 6. Ops software catch-up (biggest gap vs Block/Auradine)

1. Fleet manager: per-engine hashrate/temperature telemetry, V/f autotune,
   pool failover (port cgminer/braiins concepts to our MMIO map).
2. Stratum V2 (A4): encrypted/authenticated pool protocol, better latency.
3. Redundancy firmware: engine hot-skip on fault (SMART-style) -- the RTL
   already isolates engines; firmware just masks the dead one.
4. **[v1.5.0]** thermal_policy() heat-follow mode (already host-tested)
   productized for Line C pilots.

## 7. Milestones

(Expanded into the full product catalog and gated roadmap in PRODUCTS.md,
v1.5.0 revision 2026-10.)

- D+30 : chiseltest green (RocketMiningConfig); Verilator genesis smoke;
         stratum client testnet connect via UART proxy.
- D+90 : FPGA genesis soak clean; A1 netlist + gate-level energy numbers
         CALIBRATE tapeout/energy_model.py -> Line C go/no-go vs the
         revised 16nm/<=120 J/TH kill line.
- D+180: 16nm shuttle application; dev-kit FPGA boards to 3 pilot
         universities (Line A pre-orders); SV2 client beta;
         **[v1.5.0]** 1 paid LOI for a 50-100 kW hash-to-heat pilot
         (commercial rigs + ore control firmware; does NOT wait for our
         silicon).

## 8. Risks **[v1.5.0 updated]**

- Efficiency ladder keeps moving (S24-class ~8 J/TH rumored): the gap at
  every rung widens; niche economics stay fragile. (unchanged)
- Open PDK / shuttle access for 16nm unconfirmed; 28nm fallback keeps us
  at BZM2-class at best. (unchanged)
- **[v1.5.0]** BTC < $70k / hashprice stays at record lows: now the BASE
  CASE. Revenue must not depend on mining economics (education, heat,
  licensing). Hardware efficiency is no longer the business.
- **[v1.5.0]** Canaan/Bitmain enter hash-to-heat in force and squeeze
  Line C: differentiation is full-stack auditability (public procurement);
  fallback is licensing our thermal/DVFS stack instead of boxes.
- Open-source DFT maturity remains the tapeout schedule risk. (unchanged)
- Jurisdictional restrictions on mining hardware imports (Line C).
  (unchanged; heat-reuse positioning SOFTENS this -- we sell heating
  equipment with a mining byproduct)
