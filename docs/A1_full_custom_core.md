# A1: Full-Custom SHA-256 Round -- Design Spec & Feasibility

Benchmark: Intel BZM2 26 J/TH (open-silicon reference). Kill line: > 40 J/TH
in gate-level sim -> drop product Line C. Model: docs/a1_model.json.

## 1. Current stage critical path (v5, standard-cell RCA)

    T1 = h + S1(e) + ch(e,f,g) + (K[t]+W[t])        <- 3 serial RCAs
    T2 = S0(a) + maj(a,b,c)                          <- 1 RCA
    a' = T1 + T2 ; e' = d + T1                       <- 2 RCAs (a'/e' share T1 CPA)

    ~7 x 32-bit RCAs/stage ~= 224 FA-equiv, ~44 gate levels
    => ~1.1-1.3 ns/stage @28nm: 1 GHz is already tight, no V-scaling headroom.

## 2. Full-custom stage (CSA tree)

    Precompute: KW[t] = K[t] + W[t] one stage ahead (off critical path)
    T1 operands {h, S1, ch, KW}:  4:2 compressor tree (two 3:2 CSAs)
                                  -> (s1, c1) carry-save
    a' = (s1 + c1) + T2  via one Kogge-Stone-32 CPA + one CSA + CPA share
    e' = d + T1        shares the T1 CPA (e' = d + s1 + c1: one extra CSA)

    ~150 FA-equiv/stage, ~16 gate levels
    => ~0.4-0.5 ns/stage @28nm: 2 GHz+ with margin, AND the carry-save form
    cuts switching activity ~0.55-0.6x (no rippling carries in intermediates).

## 3. Energy model results (docs/a1_model.json)

    config                              J/TH est   vs BZM2 (26)
    std-cell RCA (v5 現況)              ~80        3.1x gap
    CSA tree, 0.9V                      ~32        1.2x gap
    CSA + KS-CPA + DVFS 0.70V           ~18        PASS
    + 16nm (C ~0.6x)                    ~11        Block-Proto class

## 4. Engineering actions (D+90 milestone)

1. Rewrite hw/sha256.scala stage with CSA compressor + KW precompute
   (parameter `useCSA` to A/B against the RCA version in chiseltest with
   the SAME golden vectors -- vectors must not change, only timing/area).
2. Gate-level: Yosys/DC-class synthesis at 28nm open PDK; get real FA-equiv,
   area, and SAIF-based energy from RTL toggle simulation.
3. Recompute docs/a1_model.json with measured numbers -> kill/go decision.
4. If GO: layout study with OpenLane/advanced-flow on the CSA stage cell,
   then full 64-stage macro.

## 5. Risks

- Prefix CPA (Kogge-Stone) wiring congestion at 32 bits; fallback Sklansky.
- Open-PDK standard cells may lack fast full-adder/compressor variants ->
  full-custom cell design becomes part of A1 (this is where BZM2's fab
  advantage lives; we accept the gap honestly).
- DVFS to 0.7V needs level shifters & per-engine regulators on the board,
  not just RTL.
