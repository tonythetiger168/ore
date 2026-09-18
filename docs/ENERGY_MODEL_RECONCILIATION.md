# Energy-model reconciliation (two models, one decision)

There are currently TWO energy models in this repo. They disagree by ~7x
at the optimistic end. This note records the discrepancy and the rule for
resolving it. Per our standing rule: no number enters a go/no-go decision
until it matches an independent anchor.

## Model A -- tapeout/energy_model.py (anchor-based)

    J/TH = 128 x E_round[pJ]

E_round back-solved from PUBLIC silicon:
    S9 (BM1387, 16nm, full-custom)  98 J/TH -> 0.766 pJ/round
    Intel BZM2 (open, ~7nm)         26 J/TH -> 0.203 pJ/round
    BM1370 (3nm, full-custom)       15 J/TH -> 0.117 pJ/round

Projections (with 1.3x pipeline overhead):
    std-cell 28nm                 ~416 J/TH
    full-custom CSA 28nm          ~200 J/TH
    full-custom 16nm              ~117 J/TH
    full-custom 16nm near-Vt      ~75 J/TH   <- realistic ceiling

## Model B -- docs/a1_model.json (factor-based, optimistic)

    baseline "std-cell 28nm = 80 J/TH", then CSA x0.55, V^2(0.7/0.9)^2,
    16nm C x0.6 -> 10.7 J/TH at 16nm.

## Why Model B is not credible as-is

1. Its 80 J/TH std-cell-28nm baseline IMPLICITLY BEATS the S9 anchor:
   S9 was full-custom at 16nm and managed 98 J/TH. Standard cells at the
   SLOWER 28nm node cannot beat full-custom 16nm. The 80 figure is a
   leftover of our early optimistic estimate, now retired.
2. Chained multiplication of optimistic factors (0.55 x 0.6 x 0.6 ~ 0.2)
   applied to an already-optimistic baseline yields ~10.7 J/TH -- i.e.
   "better than Bitmain's 3nm BM1370 (15 J/TH) with 28nm-class design".
   Extraordinary claims need gate-level evidence, not factor chains.

## Rule

- Use Model A ranges for all planning and for BENCHMARKS.md B1 targets.
- Model B's STRUCTURAL insights (CSA tree stage, shared-CPA a'/e', DVFS
  headroom) remain valid and feed the A1 design spec; its NUMBERS do not.
- D+90 gate stays: replace E_round with PTPX/SAIF-measured numbers.
- BENCHMARKS.md B1 "Current ~60-100" must be re-baselined to Model A's
  ~416 (std-cell) / ~200 (full-custom 28nm) pending measurement.

## Also flagged

docs/stratum_v2.md and sw/sv2_protocol.md use different frame-header
byte layouts. The authoritative check is the official SV2 spec test
vectors (stratum-mining/sv2-spec); BOTH documents are gated on that.
