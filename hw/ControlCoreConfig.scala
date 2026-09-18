package mining

import org.chipsalliance.cde.config.Config

// Minimal control core for the mining SoC.
// The hash engines dominate area/power; BOOM here only runs Linux + stratum
// client + IRQ handling, so trim everything non-essential.
//
// Compose AFTER WithNSmallBooms (it maps over the tiles that config creates):
//   class BoomMiningConfig extends Config(
//     new WithMiningAccel(...) ++ new WithMiningAttach ++
//     new boom.common.WithNSmallBooms(1) ++ new WithTinyControlBoom ++
//     new chipyard.config.AbstractConfig)
//
// NOTE: field names drift between BOOM/Chipyard releases -- check
// generators/boom/src/main/scala/common/config.scala in your checkout.
class WithTinyControlBoom extends Config((site, here, up) => {
  case boom.common.BoomTilesKey =>
    up(boom.common.BoomTilesKey, site).map { b =>
      b.copy(
        core = b.core.copy(
          fpu         = None,   // no floating point: ~15-20% of core area saved
          fetchWidth  = 4,
          decodeWidth = 2,
          useVM       = true    // keep the MMU: we run Linux
        ),
        icache = b.icache.map(_.copy(nSets = 64, nWays = 2)),  // 16 KiB
        dcache = b.dcache.map(_.copy(nSets = 64, nWays = 2))   // 16 KiB
      )
    }
})
