package mining.caravel

import chisel3._
import chisel3.util._
import mining._

// ---------------------------------------------------------------------------
// SKETCH -- Caravel (Wishbone classic) wrapper for the mining engines.
// NOT yet covered by chiseltest: needs a WB-driver variant of
// MiningEngineTest before tapeout (our rule: no unverified logic on silicon).
//
// Strategy: the engines themselves have no TileLink dependency -- only the
// register block was TL. Here the same register map (identical offsets) sits
// behind a Wishbone slave, so ALL golden vectors carry over unchanged.
// Caravel's management SoC (or an external MCU on the WB bus) plays the role
// the Rocket core played in the full SoC.
// ---------------------------------------------------------------------------

class WishboneIO extends Bundle {
  val cyc   = Input(Bool())
  val stb   = Input(Bool())
  val we    = Input(Bool())
  val adr   = Input(UInt(10.W))   // WORD address (offset/4), 0x000..0x2A9
  val datI  = Input(UInt(32.W))
  val datO  = Output(UInt(32.W))
  val ack   = Output(Bool())
}

class CaravelMiningTop(nEngines: Int = 4) extends Module {
  val io = IO(new Bundle {
    val wb  = new WishboneIO
    val irq = Output(Bool())
  })

  val engines = Seq.fill(nEngines)(Module(new MiningEngine))

  // ---- register file (offsets match hw/MiningAccel.scala, word units)
  val block1 = RegInit(VecInit(Seq.fill(16)(0.U(32.W))))
  val b2w    = RegInit(VecInit(Seq.fill(3)(0.U(32.W))))
  val target = RegInit(0.U(256.W))
  val nonceStart  = RegInit(0.U(32.W))
  val nonceStride = RegInit(0x08000000L.U(32.W))   // 4 engines x 512M
  val irqEn  = RegInit(false.B)
  val found  = RegInit(false.B)
  val foundNonce = RegInit(0.U(32.W))
  val foundDigest = RegInit(0.U(256.W))
  val startP = RegInit(false.B); val loadP = RegInit(false.B); val clearP = RegInit(false.B)
  val busy = engines.map(_.io.busy).reduce(_ || _)

  engines.zipWithIndex.foreach { case (e, i) =>
    e.io.template.block1 := block1
    e.io.template.b2w    := b2w
    e.io.template.target := target
    e.io.load  := loadP
    e.io.start := startP
    e.io.clear := clearP
    e.io.nonceStart := nonceStart + (i.U * nonceStride)
    e.io.nonceEnd   := nonceStart + ((i + 1).U * nonceStride)
  }
  when (clearP) { found := false.B }
  engines.foreach { e => when (e.io.found && !found) {
    found := true.B; foundNonce := e.io.foundNonce; foundDigest := e.io.foundDigest } }

  // ---- Wishbone classic: single-cycle ack, word-addressed
  val rd = WireDefault(0.U(32.W))
  val doWrite = io.wb.cyc && io.wb.stb && io.wb.we && !RegNext(io.wb.ack)
  val doRead  = io.wb.cyc && io.wb.stb && !io.wb.we && !RegNext(io.wb.ack)
  io.wb.ack := io.wb.cyc && io.wb.stb && !RegNext(io.wb.ack)

  when (doWrite) {
    switch (io.wb.adr) {
      is (0x00.U) { startP := io.wb.datI(0); loadP := io.wb.datI(1); clearP := io.wb.datI(2) }
      is (0x02.U) { irqEn := io.wb.datI(0) }
      is (0x03.U) { nonceStart := io.wb.datI }
      is (0x17.U) { nonceStride := io.wb.datI }
    }
    for (i <- 0 until 16) when (io.wb.adr === (0x04 + i).U) { block1(i) := io.wb.datI }
    for (i <- 0 until 3)  when (io.wb.adr === (0x14 + i).U) { b2w(i) := io.wb.datI }
    for (i <- 0 until 8)  when (io.wb.adr === (0x18 + i).U) {
      target := (target & ~(BigInt("FFFFFFFF", 16).U(256.W) << (i * 32))) |
               (io.wb.datI.asTypeOf(UInt(256.W)) << (i * 32)) }
  }
  when (doRead) {
    switch (io.wb.adr) {
      is (0x01.U) { rd := Cat(engines.map(_.io.done).reduce(_ || _), busy) }
      is (0x20.U) { rd := foundNonce }
      is (0x29.U) { rd := found }
    }
    for (i <- 0 until 8) when (io.wb.adr === (0x21 + i).U) { rd := foundDigest(i * 32 + 31, i * 32) }
    for (i <- 0 until 8) when (io.wb.adr === (0x18 + i).U) { rd := target(i * 32 + 31, i * 32) }
  }
  io.wb.datO := rd
  io.irq := irqEn && found

  when (startP || loadP || clearP) { startP := false.B; loadP := false.B; clearP := false.B }
}
