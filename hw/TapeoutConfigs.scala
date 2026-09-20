package mining

import org.chipsalliance.cde.config.Config
import freechips.rocketchip.rocket._

// Tapeout-scale configs: more engines to amortize the fixed power tax of
// the control complex. See tapeout/energy_model.py "ENGINE-COUNT SWEEP":
// 8 engines pays a 2.5x J/TH penalty, 64 engines ~1.19x, 128 ~1.09x.
// Verification note: keep sims/CI on 8 engines (linear sim cost); use
// these for synthesis/tapeout runs. Wrapper is fully parameterized
// (MiningParams.engines), so this is a config-only change.
class RocketMiningTapeout28Config extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 32)) ++
  new WithNSmallCores(1) ++
  new chipyard.config.AbstractConfig)

class RocketMiningTapeout16Config extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 64)) ++
  new WithNSmallCores(1) ++
  new chipyard.config.AbstractConfig)
