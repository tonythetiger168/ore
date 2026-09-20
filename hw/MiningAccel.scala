package mining

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config.{Parameters, Field, Config}
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.regmapper._
import freechips.rocketchip.tilelink._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.interrupts._

// ---------------------------------------------------------------------------
// Per-template inputs (written by BOOM over MMIO, big-endian words)
// ---------------------------------------------------------------------------
class TemplateData extends Bundle {
  val block1 = Vec(16, UInt(32.W))  // header bytes  0..63 as big-endian words
  val b2w    = Vec(3, UInt(32.W))   // header bytes 64..75: merkle tail | time | bits
  val target = UInt(256.W)          // outer digest <= target  (a-word is MSB)
}

// ---------------------------------------------------------------------------
// One mining engine:
//   load : inner pipe computes midstate = compress(H0, Block1)  (once)
//   run  : streams nonces: (midstate, Block2(nonce)) -> inner -> pad -> outer
//          outer digest compared against target; hit -> found + sticky regs
// Latency per nonce: 128 cycles pipelined -> 1 nonce/cycle sustained.
// ---------------------------------------------------------------------------
class MiningEngine extends Module {
  val io = IO(new Bundle {
    val template    = Input(new TemplateData)
    val load        = Input(Bool())   // pulse: latch template, compute midstate
    val start       = Input(Bool())   // pulse: begin scanning [nonceStart, nonceEnd)
    val clear       = Input(Bool())   // pulse: clear found/done, abort to idle
    val nonceStart  = Input(UInt(32.W))
    val nonceEnd    = Input(UInt(32.W))
    val busy        = Output(Bool())
    val done        = Output(Bool())  // range exhausted without hit
    val found       = Output(Bool())
    val foundNonce  = Output(UInt(32.W))
    val foundDigest = Output(UInt(256.W))
  })

  val inner = Module(new Sha256Pipe)
  val outer = Module(new Sha256Pipe)

  val midstate     = Reg(new HashState)
  val b2w          = Reg(Vec(3, UInt(32.W)))
  val target       = Reg(UInt(256.W))
  val nonce        = RegInit(0.U(32.W))
  val foundReg     = RegInit(false.B)
  val foundNonceR  = RegInit(0.U(32.W))
  val foundDigestR = RegInit(0.U(256.W))
  val doneReg      = RegInit(false.B)

  val sIdle :: sLoad :: sReady :: sRun :: Nil = Enum(4)
  val state  = RegInit(sIdle)
  val issued = RegInit(false.B)   // midstate request already injected into pipe

  // ---- Block 2 message schedule words for a given nonce (only W[3] varies)
  // Endianness: header fields are little-endian on the wire, so W[3] must be
  // the byte-reversal of the integer nonce (see tests/golden.py).
  def bswap32(n: UInt): UInt = Cat(n(7, 0), n(15, 8), n(23, 16), n(31, 24))

  def block2Gen(n: UInt): Vec[UInt] = {
    val w = Wire(Vec(16, UInt(32.W)))
    w(0) := b2w(0); w(1) := b2w(1); w(2) := b2w(2); w(3) := bswap32(n)
    w(4) := 0x80000000L.U
    for (i <- 5 until 15) w(i) := 0.U(32.W)
    w(15) := 640.U(32.W)
    w
  }

  // ---- outer-hash block: pad a 32-byte digest into one 512-bit block
  def outerBlock(d: Vec[UInt]): Vec[UInt] = {
    val w = Wire(Vec(16, UInt(32.W)))
    for (i <- 0 until 8) w(i) := d(i)
    w(8) := 0x80000000L.U
    for (i <- 9 until 15) w(i) := 0.U(32.W)
    w(15) := 256.U(32.W)
    w
  }

  // ---- inner pipe input mux: template load vs. nonce streaming
  val loading = state === sLoad
  inner.io.in.bits.hinit := Mux(loading, Sha256.initState, midstate)
  inner.io.in.bits.block := Mux(loading, io.template.block1, block2Gen(nonce))
  inner.io.in.valid := (loading && !issued) || (state === sRun)

  // ---- outer pipe fed directly from inner pipe
  outer.io.in.bits.hinit := Sha256.initState
  outer.io.in.bits.block := outerBlock(inner.io.out.bits.digest)
  outer.io.in.valid := inner.io.out.valid

  // ---- pipeline-aligned tracking: which nonce / whether it came from sRun
  val tagValid = RegInit(VecInit(Seq.fill(128)(false.B)))
  val tagRun   = RegInit(VecInit(Seq.fill(128)(false.B)))
  val tagNonce = Reg(Vec(128, UInt(32.W)))
  tagValid(0) := inner.io.in.valid
  tagRun(0)   := state === sRun
  when (inner.io.in.valid) { tagNonce(0) := nonce }
  for (i <- 1 until 128) {
    tagValid(i) := tagValid(i - 1)
    tagRun(i)   := tagRun(i - 1)
    tagNonce(i) := tagNonce(i - 1)
  }

  // ---- target compare + sticky capture (mask off the load-phase garbage)
  when (outer.io.out.valid && tagValid(127)) {
    // Bitcoin compares the digest as a LITTLE-ENDIAN 256-bit integer
    // (arith_uint256 in Bitcoin Core): the whole 32-byte digest is
    // byte-reversed and read as a big-endian 256-bit number.
    val dBig = Cat(outer.io.out.bits.digest.reverse.map(bswap32(_)))
    when (tagRun(127) && dBig <= target) {
      foundReg     := true.B
      foundNonceR  := tagNonce(127)
      foundDigestR := dBig
    }
  }

  // ---- control FSM
  when (io.load) {
    b2w     := io.template.b2w
    target  := io.template.target
    state   := sLoad
    issued  := false.B
    doneReg := false.B
  }
  when (loading && inner.io.in.valid) { issued := true.B }
  when (loading && issued && inner.io.out.valid) {
    midstate := HashState.fromDigest(inner.io.out.bits.digest)
    state    := sReady
  }
  when (io.start && state === sReady) {
    state := sRun
    nonce := io.nonceStart
  }
  when (state === sRun && inner.io.in.valid) {
    val nxt = nonce + 1.U
    nonce := nxt
    when (nxt >= io.nonceEnd) { state := sReady; doneReg := true.B }
  }
  when (io.clear) {
    foundReg := false.B
    doneReg  := false.B
    when (state =/= sRun) { state := sIdle; issued := false.B }
  }

  io.busy        := state === sLoad || state === sRun
  io.done        := doneReg
  io.found       := foundReg
  io.foundNonce  := foundNonceR
  io.foundDigest := foundDigestR
}

// ---------------------------------------------------------------------------
// Parameters + TileLink MMIO wrapper (TLRegisterNode on the periphery bus)
// ---------------------------------------------------------------------------
case object MiningAccelKey extends Field[Option[MiningParams]](None)
case class MiningParams(base: BigInt = 0x10020000L, engines: Int = 8)

class MiningAccelWrapper(implicit p: Parameters) extends LazyModule {
  val params = p(MiningAccelKey).getOrElse(MiningParams())
  val device = new SimpleDevice("mining-accel", Seq("edu,mining-accel"))
  val node = TLRegisterNode(Seq(AddressSet(params.base, 0xFFF)),
                            device, "reg/control", beatBytes = 8)
  val intnode = IntSourceNode(IntSourcePortSimple(num = 1,
                            sources = Seq(IntSource(name = "irq", range = 1))))

  lazy val module = new Impl

  class Impl extends LazyModuleImp(this) {
    val engines = Seq.fill(params.engines)(Module(new MiningEngine))

    // template / control registers
    val block1     = RegInit(VecInit(Seq.fill(16)(0.U(32.W))))
    val b2w        = RegInit(VecInit(Seq.fill(3)(0.U(32.W))))
    val target     = RegInit(0.U(256.W))
    val nonceStart = RegInit(0.U(32.W))
    val nonceStride= RegInit(0x02000000L.U(32.W))  // per-engine range split
    val irqEn      = RegInit(false.B)
    val busy       = RegInit(false.B)
    val found      = RegInit(false.B)
    val doneAny    = RegInit(false.B)
    val foundNonce = RegInit(0.U(32.W))
    val foundDigest= RegInit(0.U(256.W))

    // one-shot pulses set by MMIO writes
    val startPulse = RegInit(false.B)
    val loadPulse  = RegInit(false.B)
    val clearPulse = RegInit(false.B)

    // engine wiring: shared template, staggered nonce windows
    engines.zipWithIndex.foreach { case (eng, i) =>
      eng.io.template    := DontCare
      eng.io.template.block1 := block1
      eng.io.template.b2w    := b2w
      eng.io.template.target := target
      eng.io.load        := loadPulse
      eng.io.start       := startPulse
      eng.io.clear       := clearPulse
      eng.io.nonceStart  := nonceStart + (i.U * nonceStride)
      eng.io.nonceEnd    := nonceStart + ((i + 1).U * nonceStride)
    }

    busy    := engines.map(_.io.busy).reduce(_ || _)
    doneAny := engines.map(_.io.done).reduce(_ || _)
    when (clearPulse) { found := false.B }
    engines.foreach { eng =>
      when (eng.io.found && !found) {
        found       := true.B
        foundNonce  := eng.io.foundNonce
        foundDigest := eng.io.foundDigest
      }
    }

    val irq = irqEn && found
    intnode.out(0)._1(0) := irq

    // ---- register map
    node.regmap(
      0x00 -> Seq(RegField(3, RegReadFn(() => 0.U),
        RegWriteFn((valid, data) => {
          when (valid) {
            startPulse := startPulse || data(0)
            loadPulse  := loadPulse  || data(1)
            clearPulse := clearPulse || data(2)
          }
          true.B
        }), RegFieldDesc("ctrl", "bit0=start bit1=load_template bit2=clear",
                         access = RegFieldAccessType.W))),
      0x04 -> Seq(RegField.r(2, Cat(doneAny, busy),
        RegFieldDesc("status", "bit0=busy bit1=done", volatile = true))),
      0x08 -> Seq(RegField(1, irqEn)),
      0x0C -> Seq(RegField(32, nonceStart)),
      0x10 -> block1.map(w  => RegField(32, w)),
      0x50 -> b2w.map(w    => RegField(32, w)),
      0x5C -> Seq(RegField(32, nonceStride)),
      0x60 -> Seq.tabulate(8)(i => RegField(32,
                  RegReadFn(() => target(i * 32 + 31, i * 32)),
                  RegWriteFn((valid, data) => {
                    when (valid) {
                      val nt = WireInit(target)
                      nt := (target & ~(BigInt("FFFFFFFF", 16).U(256.W) << (i * 32))) |
                            (data.asTypeOf(UInt(256.W)) << (i * 32))
                      target := nt
                    }
                    true.B
                  }))),
      0x80 -> Seq(RegField.r(32, foundNonce, volatile = true)),
      0x84 -> Seq.tabulate(8)(i => RegField.r(32,
                  foundDigest(i * 32 + 31, i * 32), volatile = true)),
      0xA4 -> Seq(RegField.r(1, found, volatile = true))
    )

    // pulse self-clear
    when (startPulse || loadPulse || clearPulse) {
      startPulse := false.B
      loadPulse  := false.B
      clearPulse := false.B
    }
  }
}

// ---------------------------------------------------------------------------
// Chipyard config. Modern attachment (SubsystemInjector was REMOVED from
// rocket-chip; verified against chipyard main @371ab92): a CanHavePeriphery
// trait, mixed into ChipyardSubsystem -- see
// generators/chipyard/src/main/scala/Subsystem.scala (one-line patch,
// scripted in scripts/integrate_into_chipyard.sh) and the GCD example.
// ---------------------------------------------------------------------------
class WithMiningAccel(params: MiningParams = MiningParams())
    extends Config((site, here, up) => { case MiningAccelKey => Some(params) })

trait CanHavePeripheryMiningAccel { this: BaseSubsystem =>
  private val pbus = locateTLBusWrapper(PBUS)   // GCD-example pattern
  val miningAccel = p(MiningAccelKey).map { _ =>
    val accel = LazyModule(new MiningAccelWrapper()(p))
    pbus.coupleTo("mining-accel") {
      accel.node := TLFragmenter(pbus.beatBytes, pbus.blockBytes) := TLBuffer() := _
    }
    plicOpt.foreach { plic => plic.intnode := accel.intnode }  // IntNexusNode
    accel
  }
}

class BoomMiningConfig extends Config(
  new WithMiningAccel(MiningParams(base = 0x10020000L, engines = 8)) ++
  new boom.v3.common.WithNSmallBooms(1) ++
  new chipyard.config.AbstractConfig)
