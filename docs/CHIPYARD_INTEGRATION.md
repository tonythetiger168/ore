# Chipyard installation & integration guide

## Fresh install (Ubuntu 22.04+, 8+ cores / 16GB+ RAM / 60GB disk recommended)

    git clone https://github.com/ucb-bar/chipyard.git
    cd chipyard
    ./scripts/init-submodules-no-riscv-tools.sh
    ./scripts/build-toolchains.sh riscv-tools   # 1-3 h, ~20GB
    source ./env.sh

    # verify
    cd sims/verilator
    make CONFIG=RocketConfig

Minimal-RAM build (5GB): JAVA_OPTS="-Xmx2g" sbt compile; expect long
compiles; Verilator sim of Rocket works, Boom needs more.

## Integrate this repo

    scripts/integrate_into_chipyard.sh <chipyard-dir>

Copies hw/*.scala into the chipyard subproject and patches
generators/chipyard/src/main/scala/DigitalTop.scala to mix in
CanHavePeripheryMiningAccel (one line, same pattern as CanHavePeripheryGCD).

## Build our configs

    cd sims/verilator
    make CONFIG=BoomMiningConfig        # elaboration check
    make CONFIG=RocketMiningConfig
    # chiseltest: sbt "testOnly mining.Sha256PipeTest mining.MiningEngineTest"

## CONFIRMED against chipyard main @371ab92 (2026-09)

- SubsystemInjectorKey is REMOVED from rocket-chip. Attachment is now a
  CanHavePeriphery* trait mixed into DigitalTop (reference pattern:
  generators/chipyard/src/main/scala/example/GCD.scala). Older guides
  mentioning SubsystemInjector are obsolete.
- pbus.coupleTo still exists (rocket-chip .../subsystem/BusWrapper.scala).
- Package org.chipsalliance.cde.config confirmed in this checkout
  (older chipyard used freechips.rocketchip.config -- adjust imports then).
- MMIO base 0x10020000 must not collide with Gemmini's default window
  (hw/AiConfig.scala note) if the AI option is enabled.

## Sandbox survival note

The dev sandbox used for this project resets its filesystem between
sessions. Always keep the repo artifacts (zip/bundle) in durable storage;
restore with: unzip mining_soc_github_v13.zip && git clone x.bundle


## First-compile troubleshooting (from real attempts on chipyard main @371ab92)

If `sbt chipyard/compile` shows errors in OUR files, these were the real
ones found before (all fixed in v1.3.4, listed in case of version drift):

1. `eng.clock := ...` in LowPowerAdditions.scala -- Chisel 6 forbids manual
   clock wiring to module instances. Remove the line; ICG cells belong in
   the integration layer (documented in the file).
2. regmapper signatures (RegField.r / RegWriteFn / RegFieldDesc) -- diff
   against generators/rocket-chip/src/main/scala/regmapper/RegField.scala.
3. IntSourcePortSimple parameter names -- check
   freechips/rocketchip/interrupts/Parameters.scala.
4. boom package moved to boom.v3.common / boom.v4.common (we use v3).
5. WithNSmallCores lives in freechips.rocketchip.rocket (not subsystem).
6. Attach devices via a CanHavePeriphery* trait mixed into DigitalTop.scala
   (NOT SubsystemInjector -- removed; NOT Subsystem.scala).
