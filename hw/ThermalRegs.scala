package mining

import chisel3._
import chisel3.util._

// ---------------------------------------------------------------------------
// Thermal sensor MMIO block (offsets 0xC0+ on the mining register window).
// See docs/LIQUID_COOLING.md. Values arrive from the board BMC (I2C/SMBus)
// already synchronized to the bus clock; this block is a register window +
// sticky leak + alarm aggregation + IRQ.
//
// 0xC0 T_IN     x16 signed (0.1 C/LSB)
// 0xC4 T_OUT    x16
// 0xC8 DEWPOINT x16
// 0xCC TJ_EST   x16 (max of zone estimates)
// 0xD0 FLOW     bit0 flow_ok, [7:4] flow code
// 0xD4 LEAK     bit0 sticky (write-1-clear)
// 0xD8 ALARM    RW1C: bit0 warn, bit1 shutdown
// 0xDC IRQ_EN   thermal IRQ enable
// ---------------------------------------------------------------------------

class ThermalSensors extends Bundle {
  val tIn      = SInt(16.W)
  val tOut     = SInt(16.W)
  val dewpoint = SInt(16.W)
  val tjEst    = SInt(16.W)
  val flowOk   = Bool()
  val leak     = Bool()
}

class RegBusIO extends Bundle {
  // generic word-aligned register slave (adapt to TL or WB in integration)
  val addr  = Input(UInt(8.W))
  val wdata = Input(UInt(32.W))
  val wen   = Input(Bool())
  val rdata = Output(UInt(32.W))
}

class ThermalRegs extends Module {
  val io = IO(new Bundle {
    val bus    = new RegBusIO
    val sensor = Input(new ThermalSensors)
    val irq    = Output(Bool())
  })

  val leakSticky = RegInit(false.B)
  val alarm      = RegInit(0.U(2.W))
  val irqEn      = RegInit(false.B)

  when (io.sensor.leak) { leakSticky := true.B }
  when (io.bus.wen && io.bus.addr === 0xD4.U) { leakSticky := leakSticky & ~io.bus.wdata(0) }
  when (io.bus.wen && io.bus.addr === 0xD8.U) { alarm := alarm & ~io.bus.wdata(1, 0) }

  // auto-alarm: combinational conditions set sticky bits (write-clear only)
  when (io.sensor.leak || !io.sensor.flowOk) { alarm := alarm | 2.U }
  when (io.bus.wen && io.bus.addr === 0xDC.U) { irqEn := io.bus.wdata(0) }

  io.irq := irqEn && alarm =/= 0.U

  io.bus.rdata := 0.U
  switch (io.bus.addr) {
    is (0xC0.U) { io.bus.rdata := io.sensor.tIn.asUInt }
    is (0xC4.U) { io.bus.rdata := io.sensor.tOut.asUInt }
    is (0xC8.U) { io.bus.rdata := io.sensor.dewpoint.asUInt }
    is (0xCC.U) { io.bus.rdata := io.sensor.tjEst.asUInt }
    is (0xD0.U) { io.bus.rdata := Cat(0.U(24.W), 0.U(3.W), io.sensor.flowOk) }
    is (0xD4.U) { io.bus.rdata := Cat(0.U(31.W), leakSticky) }
    is (0xD8.U) { io.bus.rdata := Cat(0.U(30.W), alarm) }
    is (0xDC.U) { io.bus.rdata := Cat(0.U(31.W), irqEn) }
  }
}
