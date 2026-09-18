#!/usr/bin/env python3
"""First-order energy model for the mining SoC.

Core relation (Bitcoin double-SHA256, midstate-optimized):
    E_hash  = 128 compression rounds (inner 64 + outer 64)
    J/TH    = E_hash [J/1e12] = 128 * E_round [pJ]      (exact conversion)

So the WHOLE energy game reduces to one number: energy per compression
round. This model tabulates public anchors back-solved to E_round, then
projects our design under different styles/nodes, including pipeline
overhead (W-schedule regs, state flops, clock distribution: ~30%).
"""
import sys

OVERHEAD = 1.30   # pipeline regs + clock tree + W-schedule storage

ANCHORS = [
    # name,                         J/TH,  node_nm, note
    ("Antminer S9  (BM1387, 2016)",  98.0, 16, "full-custom; era reference"),
    ("Intel BZM2  (open silicon)",   26.0,  7, "open benchmark"),
    ("BM1370 chip (Bitmain, 2024)",  15.0,  3, "chip-level, voltage-dependent"),
    ("S23 Hyd     (2026 flagship)",   9.5,  3, "system-level"),
]

SCENARIOS = [
    # label,                          E_round[pJ],  style
    ("ours: std-cell 28nm, 0.9V",             2.50, "standard cells, no tricks"),
    ("ours: std-cell + operand isolation",    1.80, "gating idle sigma/ch inputs"),
    ("ours: full-custom CSA 28nm",            1.20, "carry-save adder tree"),
    ("ours: full-custom 16nm",                0.70, "node + full custom"),
    ("ours: full-custom 16nm near-Vt 0.6V",   0.45, "deep pipeline pays off here"),
]

THRESHOLDS = [
    (300.0, "heat-reuse niche viable (electricity ~free)"),
    (100.0, "S9-class; still 10x behind 2026 shipping"),
    (26.0,  "BZM2-class: credible open silicon"),
    (15.0,  "BM1370-class: barely relevant at scale"),
]

def verdict(jth):
    for t, msg in THRESHOLDS:
        if jth <= t:
            return "<= %-5.1f  %s" % (t, msg)
    return ">  300   no viable mining market"

print("=" * 78)
print("ANCHORS (public specs back-solved to E_round)")
print("=" * 78)
print("%-34s %7s %6s %8s" % ("design", "J/TH", "node", "E_round"))
for name, jth, node, note in ANCHORS:
    print("%-34s %7.1f %4dnm %7.3f pJ/round   %s" % (name, jth, node, jth / 128, note))

print()
print("=" * 78)
print("OUR PROJECTIONS  (J/TH = 128 * E_round * overhead %.2f)" % OVERHEAD)
print("=" * 78)
print("%-40s %9s %10s  %s" % ("scenario", "E_round", "J/TH", "first threshold crossed"))
for label, er, style in SCENARIOS:
    jth = 128 * er * OVERHEAD
    print("%-40s %8.2fpJ %9.1f  %s" % (label, er, jth, verdict(jth)))

print()
print("KEY CONCLUSIONS")
print("-" * 78)
concl = [
 "1. J/TH = 128 x E_round[pJ]. Full-custom at 28nm gets ~200 J/TH -- the",
 "   earlier 60-100 J/TH estimate for standard-cell 28nm was OPTIMISTIC",
 "   (S9 full-custom 16nm was already 98 J/TH). Standard-cell 28nm is",
 "   realistically ~300-500 J/TH.",
 "2. BZM2-class (26 J/TH) is NOT reachable at 28nm. It needs <=16nm",
 "   full-custom. Update roadmap: 28nm is a VALIDATION vehicle only.",
 "3. Kill-criterion revision: at 28nm full-custom, the only market left",
 "   is heat-reuse (<300 J/TH threshold). Line C mining appliance must",
 "   therefore be 16nm-class from day one, or be reframed as dev kit.",
 "4. Near-threshold + deep pipeline (our 64-stage design) is the one",
 "   structural advantage we can exploit: E ~ V^2, and our short per-",
 "   stage logic allows lower V at the same f than shallow pipelines.",
 "5. Calibrate this model with gate-level sim numbers (PrimeTime/PTPX",
 "   or open equivalents) at D+90; replace E_round estimates with",
 "   measured ones before any go/no-go decision.",
]
print("\n".join(concl))

# ---------------------------------------------------------------------------
# V-scaling scenarios (dynamic energy ~ V^2; anchor: fc-16nm near-Vt 0.6V ->
# E_round 0.45 pJ). 128-stage variant: more pipeline flops (overhead 1.45)
# but shorter per-stage logic allows ~12% lower V at the same f.
# ---------------------------------------------------------------------------
print()
print("=" * 78)
print("V-SCALING (E_round = 0.45pJ x (V/0.6)^2, fc-16nm; +15% leak margin <=0.6V)")
print("=" * 78)
print("%-46s %8s" % ("scenario", "J/TH"))
for V, oh, note in [(0.90, 1.30, "nominal"),
                    (0.75, 1.30, ""),
                    (0.60, 1.30, "near-Vt"),
                    (0.55, 1.30, "deep near-Vt"),
                    (0.60, 1.45, "128-stage(2r/stage), V headroom unused"),
                    (0.53, 1.45, "128-stage + lower V (-12% path)")]:
    er = 0.45 * (V / 0.6) ** 2
    jth = 128 * er * oh
    if V <= 0.6: jth *= 1.15
    print("%-46s %8.1f  %s" % ("V=%.2f  oh=%.2f" % (V, oh), jth, note))
print()
print("PIPELINE TRADEOFF: 128-stage costs +15% flops but V x0.883 ->")
print("net = 0.883^2 x 1.30/1.45 = 0.70 (~30% less J/TH). For mining,")
print("DEEP PIPELINE + LOW V beats shallow pipeline + high V.")

# ---------------------------------------------------------------------------
# Fixed-overhead amortization: J/TH_chip = (n*P_eng + P_fixed) / (n*HR_eng)
# Engine-only J/TH (fc-16nm near-Vt, incl. overhead+leak) ~ 75-86; the
# question is how much the control SoC + L2 + PLL + IO tax it.
# ---------------------------------------------------------------------------
print()
print("=" * 78)
print("ENGINE-COUNT SWEEP  (P_eng=74.9mW @1GHz incl. pipe overhead+leak;")
print("P_fixed ~ 0.7W est: Rocket+SRAM+L2+PLL+pads+static)")
print("=" * 78)
P_ENG = 0.0749        # W per engine at 1 GHz (86 J/TH x 0.001 TH/s)
P_FIX = 0.7           # W, control complex + static
print("%-10s %9s %9s %9s" % ("engines", "P_chip W", "TH/s", "J/TH(chip)"))
for n in [8, 16, 32, 64, 128, 256]:
    p = P_ENG * n + P_FIX
    th = n * 0.001
    print("%-10d %9.2f %9.3f %9.1f" % (n, p, th, p / th))
print()
print("Rule of thumb: P_engines >= 3-5 x P_fixed amortizes the tax.")
print("n=8  -> 2.5x penalty | n=64 -> 1.19x | n=128 -> 1.09x (diminishing)")


