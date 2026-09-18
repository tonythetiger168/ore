package mining

import org.chipsalliance.cde.config.Config
import freechips.rocketchip.subsystem._

// ---------------------------------------------------------------------------
// Rocket-based mining SoC control core.
//
// Rocket is the better engineering choice for the control role: in-order,
// ~5x smaller than SmallBoom, trivial timing closure even at 130nm, fast RTL
// simulation. The engines do the hashing; this core only runs the stratum
// client / IRQ handling.
//
// NOTE: field names drift between rocket-chip releases -- check
// generators/rocket-chip/src/main/scala/subsystem/Configs.scala and
// .../rocket/RocketCoreParams.scala in your Chipyard.
// ---------------------------------------------------------------------------

// Trim the control core: no FPU, small caches. (mulDiv kept for software.)
class WithTinyControlRocket extends Config((site, here, up) => {
  case RocketTilesKey =>
    up(RocketTilesKey, site).map { r =>
      r.copy(
        core = r.core.copy(
          fpu = None,      // integer-only control core: saves area
          useVM = true     // keep MMU: runs Linux
          // mulDiv = Some(freechips.rocketchip.rocket.MulDivParams(...))
        ),
        icache = r.icache.map(_.copy(nSets = 64, nWays = 2)),  // 16 KiB
        dcache = r.dcache.map(_.copy(nSets = 64, nWays = 2))   // 16 KiB
      )
    }
})

// Primary tapeout-oriented config: 1x small Rocket + 8 mining engines.
class RocketMiningConfig extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 8)) ++
  new WithNSmallCores(1) ++         // use WithNBigCores(1) for more headroom
  new WithTinyControlRocket ++
  new chipyard.config.AbstractConfig)

// Optional dual-control-core variant: one core mines, one runs monitoring /
// DVFS / OTA updates. Still tiny compared to the engine array.
class RocketMiningDualConfig extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 8)) ++
  new WithNSmallCores(2) ++
  new WithTinyControlRocket ++
  new chipyard.config.AbstractConfig)
