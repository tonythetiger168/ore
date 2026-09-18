package mining

import chisel3._
import chisel3.util._

// ---------------------------------------------------------------------------
// Power/clock control block (offsets 0xB0+). Pairs with
// docs/LOW_POWER_DESIGN.md and sw/dvfs_daemon.c / engine_manager.c.
//
// 0xB0 CLK_EN    per-engine clock-enable bitmask        (RW)
// 0xB4 FAULT_MSK per-engine fault-mask bitmask          (RW; masks found/done
//              aggregation for a dead engine, SMART-style skip)
// 0xB8 VSEL      DVFS voltage-select index 0..7         (RW; drives on-chip
//              LDO/DVFS rail controller in integration)
// 0xBC ACTIVITY  per-engine toggle-count snapshot (debug)(R)
// Integration: parent creates per-engine gated clocks
//   engClk(i) := busClk & clkEn(i) & !faultMsk(i)
// using proper ICG cells (integration layer, not Chisel).
// ---------------------------------------------------------------------------
class PowerRegs(nEngines: Int = 8) extends Module {
  val io = IO(new Bundle {
    val bus     = new RegBusIO
    val activity = Input(Vec(nEngines, UInt(32.W)))  // from toggle counters
    val clkEn   = Output(UInt(nEngines.W))
    val faultMsk= Output(UInt(nEngines.W))
    val vsel    = Output(UInt(3.W))
  })

  val clkEnR  = RegInit(((1 << nEngines) - 1).U(nEngines.W))
  val faultR  = RegInit(0.U(nEngines.W))
  val vselR   = RegInit(3.U(3.W))   // mid-range default

  when (io.bus.wen) {
    switch (io.bus.addr) {
      is (0xB0.U) { clkEnR := io.bus.wdata(nEngines - 1, 0) }
      is (0xB4.U) { faultR := io.bus.wdata(nEngines - 1, 0) }
      is (0xB8.U) { vselR  := io.bus.wdata(2, 0) }
    }
  }

  io.bus.rdata := 0.U
  switch (io.bus.addr) {
    is (0xB0.U) { io.bus.rdata := Cat(0.U((32 - nEngines).W), clkEnR) }
    is (0xB4.U) { io.bus.rdata := Cat(0.U((32 - nEngines).W), faultR) }
    is (0xB8.U) { io.bus.rdata := Cat(0.U(29.W), vselR) }
    is (0xBC.U) {
      // read one engine's counter per cycle: addr bits [7:4] select engine
      io.bus.rdata := io.activity(io.bus.addr(6, 4))
    }
  }
  io.clkEn := clkEnR; io.faultMsk := faultR; io.vsel := vselR
}
