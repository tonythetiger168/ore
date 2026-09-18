# AI options for the mining SoC

Three very different things get called "AI" here. Rank them by
cost/benefit. Bitmain itself runs Sophon -- the dual-use narrative is
validated -- but they kept AI chips as a SEPARATE line. Ours is different:
shared silicon, shared power/thermal, open.

## Tier 1 -- AI FOR the miner (software only, zero HW cost, DO NOW)

Runs on the existing Rocket core. No NRE, no re-verification.

1. DVFS policy learning: replace the static Pareto pick with an online
   bandit over (vsel, freq) arms; reward = accepted-share rate / watt.
   Extends sw/dvfs_daemon.c::pick_pareto (pure function, host-testable).
2. Predictive maintenance: per-engine fault classifier over toggle counts
   (0xBC), temps, error rates -> early warning before SMART skip fires.
   Feeds engine_manager.c FAULT_MSK decisions.
3. Thermal MPC: replace the linear derate_steps() policy with a simple
   model-predictive controller over the LIQUID_COOLING loop (fan/pump PWM
   + VSEL jointly). Payoff: higher sustained outlet temp in heat-reuse
   mode (every extra degree of water temp = more heating value).
4. Security anomaly detection on stratum traffic (share-rate baseline,
   pool-behavior drift) -- the Antbleed-class tripwire, ML-flavored.

## Tier 2 -- AI accelerator ON the SoC (config-level, Phase-2 option)

Chipyard ships Gemmini (Berkeley systolic-array NPU, ONNX/TVM flow,
FireSim + Spike support). Adding it is a CONFIG, not a project:

    class RocketMiningAiConfig extends Config(
      new WithMiningAccel(...) ++            // trait mixed in via scripts/integrate_into_chipyard.sh
      new gemmini.DefaultGemminiConfig ++          // check version drift
      new WithNSmallCores(1) ++ new WithTinyControlRocket ++
      new chipyard.config.AbstractConfig)

Caveats:
- MMIO map: Gemmini defaults near 0x1000_0000 -- CHECK collision with our
  mining block at 0x1002_0000 and rebase one of them.
- Area: a 16x16 Gemmini tile is several mm^2 at 28nm -- shares the
  tapeout area budget with engines; sweep engines-per-area.
- Shared infrastructure (the real win): liquid cooling, DVFS rails,
  SMART masking, KAT test culture, and the firesim/fpga bring-up flow
  all serve BOTH accelerators.

Product effect:
- Line A becomes "the two-accelerator open SoC": mining engine (tiny,
  fully verifiable, KAT-gated -- perfect teaching vehicle) + a real NPU
  (the industry-standard open one). No competitor sells this bundle.
- Line C scheduling: mine when profitable, run local inference always.
  The box earns in two currencies; heat-reuse economics improve further.

## Tier 3 -- AI chip product line (DO NOT)

Separate AI silicon means fighting Qualcomm/Rockchip/Hailo in a market
with their own 100x scale advantages -- the same losing arithmetic as
chasing Bitmain on J/TH, with worse margins. Bitmain's Sophon line is the
cautionary tale: they had the capital and still stayed niche. Our AI
exists to make the mining product smarter (Tier 1) and the dev kit more
valuable (Tier 2) -- never as the product itself.

## Gate

Tier 2 enters the roadmap as an OPTION at Phase 2 (A-16 design freeze):
enable only if (a) Gemmini integrates without disturbing the verified
mining paths, (b) the AI toolchain demo (ONNX -> Gemmini -> FireSim) runs
end-to-end, (c) area budget still fits >= 48 engines. Otherwise stay
Tier 1 only -- the ML-on-miner features carry most of the value anyway.
