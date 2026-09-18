# Low-power design for the mining SoC

Audience: this project's #1 metric is J/TH = 128 x E_round[pJ]. Every
technique below is ranked by its effect on that number. See
tapeout/energy_model.py for the quantified ladder.

## 0. First principles

A mining ASIC runs its engines at ~100% duty cycle when hashing. So:

- STATIC/leakage power matters at near-threshold and at idle; DYNAMIC
  switching energy per hash is the product metric.
- Idle tricks (clock gating when off) save watts, not J/TH. They matter
  for the CONTROL SoC (Rocket core idles >99% of the time) and for
  faulted/throttled engines -- not for the steady-state number.
- The two big levers on E/hash: capacitance C (circuit style) and
  voltage V (E ~ C*V^2 + leakage). Frequency drops out of the equation.

## 1. Ranked levers (model numbers, fc-16nm base)

| # | Lever                          | Expected effect | Where applied |
|---|--------------------------------|-----------------|---------------|
| 1 | Near-threshold V (0.9->0.6V)   | 2.25x (168->86 J/TH) | sign-off corners + PLL/LDO range |
| 2 | Full-custom CSA datapath       | ~2x vs std-cell | A1 core design |
| 3 | Deep pipeline + lowest-V closure | ~15-30% (model-sensitive; verify at D+90) | 128-stage variant |
| 4 | Operand/data gating            | 5-15% of stage logic | Sha256Stage iso_en |
| 5 | Clock gating (idle/faulted/throttled engines) | watts, not J/TH | wrapper + firmware |
| 6 | Multi-Vt (HVT on leakage paths)| 10-30% of leakage | synthesis script |
| 7 | Power domains (control SoC vs engines) | isolation + DVFS flexibility | floorplan |

## 2. Circuit-level techniques

### 2.1 Voltage strategy (the biggest lever)
- Design must CLOSE TIMING across 0.55-0.9V (or 0.5-0.8V at 16nm).
  This is a corner-signoff commitment, not an ECO: cell libraries at
  low V, PLL range, LDO headroom, IR-drop re-checked at low V.
- DVFS is per-CHIP at first (single engine rail); per-engine rails only
  make sense at 16nm+ with bump/package budget. Firmware sweeps
  V x f at bring-up and stores per-chip optimum (Braiins/EnergyTune
  style). MMIO V-select reg in LowPowerAdditions.scala.
- Near-Vt leakage: budget +15% energy margin (already in the model).
  Multi-Vt: HVT for non-critical nets, SVT/LVT only where the path needs it.

### 2.2 Capacitance reduction
- Carry-save adder tree: avoid full carry-propagate until the final
  add; share compressors across the a'/e' symmetry of the SHA-256 round
  (see docs/A1_full_custom_core.md structural insights -- numbers there
  are governed by docs/ENERGY_MODEL_RECONCILIATION.md).
- Minimum-size gates on non-critical fanout; gate sizing by stage
  (later pipeline stages have more slack -> smaller cells).
- Retiming to balance stage logic so no stage is oversized.

### 2.3 Activity reduction (operand/data gating)
- Fully-unrolled pipeline computes every cycle during streaming -- good.
- Gains exist in the NON-streaming windows: template load passes inject
  garbage through both pipes for 128 cycles; tag-valid logic masks
  results but the switching still happens. Add stage-level iso_en
  (force adder inputs to 0) enabled outside sRun. Sketch in
  hw/LowPowerAdditions.scala.
- W-schedule window: 16x32 shift registers toggle every cycle. When
  iso_en is low, freeze the window shift (shiftEn) -- removes a
  512-bit/cycle toggle source during idle.

### 2.4 Clock gating
- Engines: gate the two hash pipes when the engine is not in sRun
  (clock = en & (busy | load_pulse)). Register file stays on the bus
  clock -- gated engines must still accept template/start pulses, so
  wake on write: en := en | pulse; hardware clears after FSM idle.
- Fault-skip (SMART-style, pairs with sw/engine_manager.c): mask a
  faulted engine by clearing its clk_en -- no need to power-gate for
  v1 (leakage of one idle engine array is small at 28nm).

### 2.5 Control SoC (Rocket)
- Rocket idles >99% of the time (waits on found IRQ). Enable its core
  clock gate in WFI, keep TileLink/PLIC alive. Whole control complex
  should be <1-2% of chip power -- if it isn't, the engines are
  undersized. Separate power domain so engines can be DVFS'd without
  the core.

## 3. Physical design

- IR drop: N engines toggle simultaneously; power grid sized for
  worst-case di/dt. Add decap rows between engine columns.
- Grid pitch matched to engine tiling (sea-of-identical-logic) so the
  array floorplans without channel congestion.
- Clock tree: per-engine branch gates; balance within array to avoid
  skew-induced EDA churn. 16k+ pipeline flops: use clock-tree
  synthesis with useful-skew at the block level.

## 4. Firmware / measurement (closes the loop)

- DVFS daemon: boot-time sweep (V,f) x hashrate-error-rate, pick
  Pareto-optimal point per temperature bin; derate on hotspot.
  Extend sw/engine_manager.c (engine_manager already does masking/
  telemetry -- add the V-select hook once the MMIO reg lands).
- Measurement methodology: SAIF from gate-level sim x library pJ/toggle
  (PTPX; open path: Verilator toggle counts x per-gate cap estimates --
  coarser but free). Feed MEASURED E_round back into
  tapeout/energy_model.py before any go/no-go (D+90 gate).
- In-silicon: per-engine activity proxy = toggle counter on one stage
  (cheap) -> sanity-check the model against real silicon.

## 5. What NOT to do (honest list)

- Don't expect clock gating to move J/TH. It saves watts at idle.
- Don't multiply optimistic factors into the model (see the
  reconciliation doc -- chained 0.55x0.6x0.6 claims got us 10.7 J/TH
  nonsense once).
- Don't per-engine-rail at 28nm shuttle: bond-wire/package budget and
  LDO count kill it; single rail + firmware V-select is the right v1.
- 128-stage only wins if real V reduction > ~14% (leak-adjusted); make
  the call from gate sim, not the first-order model.
