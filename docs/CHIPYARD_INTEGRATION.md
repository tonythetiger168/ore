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
generators/chipyard/src/main/scala/Subsystem.scala to mix in
CanHavePeripheryMiningAccel (one line, same pattern as CanHavePeripheryGCD).

## Build our configs

    cd sims/verilator
    make CONFIG=BoomMiningConfig        # elaboration check
    make CONFIG=RocketMiningConfig
    # chiseltest: sbt "testOnly mining.Sha256PipeTest mining.MiningEngineTest"

## CONFIRMED against chipyard main @371ab92 (2026-09)

- SubsystemInjectorKey is REMOVED from rocket-chip. Attachment is now a
  CanHavePeriphery* trait mixed into ChipyardSubsystem (reference pattern:
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
