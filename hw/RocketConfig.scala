package mining

import org.chipsalliance.cde.config.Config
import freechips.rocketchip.subsystem._
import freechips.rocketchip.rocket._   // WithNSmallCores lives here (not subsystem)

// Rocket-based mining SoC control core. Verified chipyard main @371ab92:
// official WithNSmallCores is ALREADY minimal (useVM=false, fpu=None,
// 64x1 caches) -- the earlier WithTinyControlRocket fragment was redundant.
class RocketMiningConfig extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 8)) ++
  new WithNSmallCores(1) ++
  new chipyard.config.AbstractConfig)

class RocketMiningDualConfig extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 8)) ++
  new WithNSmallCores(2) ++
  new chipyard.config.AbstractConfig)
