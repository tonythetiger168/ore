package mining

import org.chipsalliance.cde.config.Config

// Tier 2 (docs/AI_ACCELERATION.md): add the Gemmini NPU alongside the
// mining engines. OPTION gated at Phase 2 (A-16 freeze).
// CHECK: Gemmini's default MMIO window may collide with the mining block
// at 0x10020000 -- rebase one (set gemmini params or move MiningParams.base).
class RocketMiningAiConfig extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 32)) ++
  new gemmini.DefaultGemminiConfig ++
  new WithNSmallCores(1) ++
  new WithTinyControlRocket ++
  new chipyard.config.AbstractConfig)
