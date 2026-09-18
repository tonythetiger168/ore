package mining

import chisel3._
import chisel3.util._

// SKETCH -- thermal sensor MMIO block (0xC0+), pairs with
// docs/LIQUID_COOLING.md and sw/dvfs_daemon.c::thermal_policy().
// Values come from a board BMC over I2C/SMBus in practice; this block is
// a register window + interrupt aggregator. UNVERIFIED (chiseltest gate).
//
// 0xC0  T_IN      coolant inlet   x16 fixed-point (0.1C/LSB, 2's comp)
// 0xC4  T_OUT     coolant outlet  x16
// 0xC8  DEWPOINT  ambient dewpoint x16
// 0xCC  TJ_EST    max zone junction estimate x16
// 0xD0  FLOW_OK   bit0: flow present; bits[7:4]: flow code
// 0xD4  LEAK      bit0: leak sensor sticky (write 1 clear)
// 0xD8  ALARM     RW1C: bit0 warn, bit1 shutdown
// 0xDC  IRQ_EN    thermal IRQ enable
class ThermalRegs extends Module {
  val io = IO(new Bundle {
    val bus    = new RegMapIO           // attach to TL/WB window at 0xC0
    val sensor = Input(new ThermalSensors) // from BMC/I2C domain (sync'd)
    val irq    = Output(Bool())
  })
  // structural sketch: registers + sticky leak + alarm aggregation
  ...
}
