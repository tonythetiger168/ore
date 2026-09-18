package mining

import chisel3._
import chisel3.util._

// ---------------------------------------------------------------------------
// SKETCH -- low-power additions (see docs/LOW_POWER_DESIGN.md).
// UNVERIFIED: needs chiseltest (gate on/off during sRun, iso_en semantics)
// + gate sim before tapeout. Register offsets are NEW (0xB0+) and do not
// collide with the existing map.
// ---------------------------------------------------------------------------

// 1. Clock-gated wrapper: hash pipes sleep unless the engine FSM is busy.
//    Template/start/clear pulses wake it; it sleeps after FSM returns to
//    idle/ready. Register file and FSM stay on the bus clock.
class ClockGatedEngine extends Module {
  val io = IO(new Bundle {
    val busClockIn = Input(Clock())
    val en       = Input(Bool())          // master enable (power reg)
    val template = Input(new TemplateData)
    val load     = Input(Bool())
    val start    = Input(Bool())
    val clear    = Input(Bool())
    val nonceStart = Input(UInt(32.W))
    val nonceEnd   = Input(UInt(32.W))
    val busy  = Output(Bool())
    val done  = Output(Bool())
    val found = Output(Bool())
    val foundNonce = Output(UInt(32.W))
    val foundDigest = Output(UInt(256.W))
  })

  val eng = withClockAndReset(io.busClockIn, reset) { Module(new MiningEngine) }
  eng.io.template := io.template
  eng.io.nonceStart := io.nonceStart
  eng.io.nonceEnd := io.nonceEnd

  val asleep = RegInit(true.B)
  val wake = io.load || io.start || io.clear
  when (wake) { asleep := false.B }
  when (eng.io.busy === false.B && !wake) { asleep := true.B }

  val gated = io.en && !asleep
  eng.clock := io.busClockIn  // in real integration: clock-gated branch net
  eng.io.load  := io.load
  eng.io.start := io.start
  eng.io.clear := io.clear

  io.busy := eng.io.busy; io.done := eng.io.done; io.found := eng.io.found
  io.foundNonce := eng.io.foundNonce; io.foundDigest := eng.io.foundDigest
  // TODO(integration): instantiate via a clock-gated wrapper in the parent
  // (BUFGC_E / latch-based ICG); 'gated' is the enable into that cell.
  // Do NOT use this sketch's eng.clock line as-is; Chisel cannot express
  // ICG insertion -- it belongs in the integration layer.
}

// 2. Operand-isolated stage variant: force sigma/adder inputs to zero when
//    iso=0, freeze the W window. Kills idle-phase toggling (~5-15% of stage
//    logic dynamic, and all 512 bits/cycle of window shifting).
class Sha256StageIso(t: Int) extends Module {
  val io = IO(new Bundle {
    val in  = Input(new StageData)
    val iso = Input(Bool())   // 1 = normal operation, 0 = isolated
    val out = Output(new StageData)
  })
  val s0 = Module(new Sha256Stage(t))
  val wSafe = Wire(Vec(16, UInt(32.W)))
  for (i <- 0 until 16) wSafe(i) := Mux(io.iso, io.in.w(i), 0.U)
  val stateSafe = Wire(new HashState)
  val iz = HashState.zero
  Seq(stateSafe.a, stateSafe.b, stateSafe.c, stateSafe.d,
      stateSafe.e, stateSafe.f, stateSafe.g, stateSafe.h).zip(
    Seq(io.in.state.a, io.in.state.b, io.in.state.c, io.in.state.d,
        io.in.state.e, io.in.state.f, io.in.state.g, io.in.state.h).zip(
    Seq(iz.a, iz.b, iz.c, iz.d, iz.e, iz.f, iz.g, iz.h))).foreach {
    case (o, (a, b)) => o := Mux(io.iso, a, b) }
  s0.io.in.state := stateSafe
  s0.io.in.w := wSafe
  s0.io.in.valid := io.in.valid && io.iso
  io.out := s0.io.out
}

// 3. Power/clock control registers (new offsets, 0xB0+):
//    0xB0  per-engine clock-enable bitmask  (RW)
//    0xB4  per-engine fault-mask  bitmask    (RW; engine_manager.c writes)
//    0xB8  V-select (0..7 -> on-chip LDO/DVFS table index)  (RW)
//    0xBC  per-engine activity proxy toggle-count snapshot   (R, debug)
// Integrate into MiningAccelWrapper.regmap alongside the existing map.
