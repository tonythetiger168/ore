# DFT (design for test) plan -- the open-flow weak point, tackled first

Our standing tapeout rule: no unverified logic on silicon. DFT is where
open tools are weakest, so this plan starts in Phase 1 (see PRODUCTS.md).

## Architecture

- Scan style: full scan per engine block + wrapper chains around SRAMs
  (none in v1 engines -- registers only; Line B scratchpads will need
  SRAM bypass/march logic).
- Chains: one chain per engine keeps shift length short and enables
  per-engine diagnosis (fault map -> engine_manager FAULT_MSK -- the same
  masking used for SMART-style skip doubles as repair).
- ATPG: stuck-at (target >= 99.5% coverage per engine), transition-at-speed
  for eng_clk paths (at-speed via PLL in test mode), IDDQ optional.
- Boundary: RISC-V debug (dtm) + JTAG pads give chip-level access; scan
  control via a JTAG-TAP side instruction or a dedicated test controller.

## Test modes

1. SCAN: engines isolated, chains stitched, standard ATPG.
2. KAT-BIST: self-contained genesis known-answer test (the bare-metal
   miner_baremetal flow) executable via JTAG without booting Linux --
   doubles as burn-in workload (deterministic, high toggle).
3. THERMAL-BIST: all engines at Vmax to force hot spots during HTOL.

## Open-toolchain reality check

- Scan insertion in fully-open flows (yosys-based) is immature; options:
  (a) fund/contribute DFT support, (b) use foundry/ shuttle-provided
  scan flow, (c) budget commercial synthesis+DFT for the tapeout run.
  Decision needed by G1 (2027-06); default is (b)+(c) hybrid.
- Whatever the flow, DELIVERABLES at tapeout: scan netlist + ATPG
  patterns + coverage report + BIST binaries in the repo (tests/ dir).

## Production hooks

- Wafer probe: scan + KAT-BIST per die; record per-engine fault map in
  e-fuses/OTP -> engine_manager programs FAULT_MSK at first boot.
- Final test: KAT + calorimetric (LIQUID_COOLING.md sec. 7).
