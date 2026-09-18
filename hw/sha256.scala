package mining

import chisel3._
import chisel3.util._

// ---------------------------------------------------------------------------
// SHA-256 constants
// ---------------------------------------------------------------------------
object Sha256 {
  val K = Seq(
    0x428a2f98L, 0x71374491L, 0xb5c0fbcfL, 0xe9b5dba5L,
    0x3956c25bL, 0x59f111f1L, 0x923f82a4L, 0xab1c5ed5L,
    0xd807aa98L, 0x12835b01L, 0x243185beL, 0x550c7dc3L,
    0x72be5d74L, 0x80deb1feL, 0x9bdc06a7L, 0xc19bf174L,
    0xe49b69c1L, 0xefbe4786L, 0x0fc19dc6L, 0x240ca1ccL,
    0x2de92c6fL, 0x4a7484aaL, 0x5cb0a9dcL, 0x76f988daL,
    0x983e5152L, 0xa831c66dL, 0xb00327c8L, 0xbf597fc7L,
    0xc6e00bf3L, 0xd5a79147L, 0x06ca6351L, 0x14292967L,
    0x27b70a85L, 0x2e1b2138L, 0x4d2c6dfcL, 0x53380d13L,
    0x650a7354L, 0x766a0abbL, 0x81c2c92eL, 0x92722c85L,
    0xa2bfe8a1L, 0xa81a664bL, 0xc24b8b70L, 0xc76c51a3L,
    0xd192e819L, 0xd6990624L, 0xf40e3585L, 0x106aa070L,
    0x19a4c116L, 0x1e376c08L, 0x2748774cL, 0x34b0bcb5L,
    0x391c0cb3L, 0x4ed8aa4aL, 0x5b9cca4fL, 0x682e6ff3L,
    0x748f82eeL, 0x78a5636fL, 0x84c87814L, 0x8cc70208L,
    0x90befffaL, 0xa4506cebL, 0xbef9a3f7L, 0xc67178f2L
  ).map(_.U(32.W))

  // Standard SHA-256 initial hash value H0
  val H0 = Seq(
    0x6a09e667L, 0xbb67ae85L, 0x3c6ef372L, 0xa54ff53aL,
    0x510e527fL, 0x9b05688cL, 0x1f83d9abL, 0x5be0cd19L
  ).map(_.U(32.W))

  def initState: HashState = {
    val s = Wire(new HashState)
    s.a := H0(0); s.b := H0(1); s.c := H0(2); s.d := H0(3)
    s.e := H0(4); s.f := H0(5); s.g := H0(6); s.h := H0(7)
    s
  }
}

// ---------------------------------------------------------------------------
// Working state registers a..h
// ---------------------------------------------------------------------------
class HashState extends Bundle {
  val a = UInt(32.W)
  val b = UInt(32.W)
  val c = UInt(32.W)
  val d = UInt(32.W)
  val e = UInt(32.W)
  val f = UInt(32.W)
  val g = UInt(32.W)
  val h = UInt(32.W)
}

object HashState {
  // digest words a..h (a = most significant) back into a chaining state
  def fromDigest(d: Vec[UInt]): HashState = {
    val s = Wire(new HashState)
    s.a := d(0); s.b := d(1); s.c := d(2); s.d := d(3)
    s.e := d(4); s.f := d(5); s.g := d(6); s.h := d(7)
    s
  }
  def zero: HashState = {
    val s = Wire(new HashState)
    s.a := 0.U; s.b := 0.U; s.c := 0.U; s.d := 0.U
    s.e := 0.U; s.f := 0.U; s.g := 0.U; s.h := 0.U
    s
  }
}

// Carried between pipeline stages.
// Window invariant at the INPUT of stage t: w(i) = W[t + i]
class StageData extends Bundle {
  val state = new HashState
  val w     = Vec(16, UInt(32.W))
  val valid = Bool()
}

// ---------------------------------------------------------------------------
// One fully-unrolled SHA-256 round. K(t) is a compile-time constant.
// ---------------------------------------------------------------------------
class Sha256Stage(t: Int) extends Module {
  val io = IO(new Bundle {
    val in  = Input(new StageData)
    val out = Output(new StageData)
  })

  def rotr(x: UInt, n: Int) = (x >> n) | (x << (32 - n))
  def bsig1(x: UInt) = rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25)
  def bsig0(x: UInt) = rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22)
  def ssig1(x: UInt) = rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10)
  def ssig0(x: UInt) = rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3)
  def ch (x: UInt, y: UInt, z: UInt)   = (x & y) ^ (~x & z)
  def maj(x: UInt, y: UInt, z: UInt)   = (x & y) ^ (x & z) ^ (y & z)

  val s  = io.in.state
  val wT = io.in.w(0)                       // W[t]
  val k  = Sha256.K(t)                      // constant

  val T1 = s.h + bsig1(s.e) + ch(s.e, s.f, s.g) + k + wT
  val T2 = bsig0(s.a) + maj(s.a, s.b, s.c)

  val ns = Wire(new HashState)
  ns.a := T1 + T2
  ns.b := s.a
  ns.c := s.b
  ns.d := s.c
  ns.e := s.d + T1
  ns.f := s.e
  ns.g := s.f
  ns.h := s.g

  // W[t+16] = ssig1(W[t+14]) + W[t+9] + ssig0(W[t+1]) + W[t]
  val wNext = ssig1(io.in.w(14)) + io.in.w(9) + ssig0(io.in.w(1)) + io.in.w(0)

  io.out.state := ns
  for (i <- 0 until 15) io.out.w(i) := io.in.w(i + 1)
  io.out.w(15) := wNext
  io.out.valid := io.in.valid
}

// ---------------------------------------------------------------------------
// 64-stage fully pipelined SHA-256 compression core.
//
// SIMPLIFICATION (teaching version): no backpressure. io.out is Valid-only and
// the consumer MUST accept one result per cycle. A production engine array
// needs skid buffers between pipes; see README "Known simplifications".
//
// Throughput: 1 block/cycle after 64-cycle fill.
// ---------------------------------------------------------------------------
class PipeIn extends Bundle {
  val hinit = new HashState        // IV, or midstate for Bitcoin inner hash
  val block = Vec(16, UInt(32.W))  // 512-bit message block, big-endian words
}

class PipeOut extends Bundle {
  val digest = Vec(8, UInt(32.W))  // a..h, digest word a is the MSB
}

class Sha256Pipe extends Module {
  val io = IO(new Bundle {
    val in  = Flipped(Decoupled(new PipeIn))
    val out = Valid(new PipeOut)
  })

  val stages = Seq.tabulate(64)(t => Module(new Sha256Stage(t)))

  val inReg   = Reg(new PipeIn)
  val inValid = RegInit(false.B)

  io.in.ready := true.B
  when (io.in.fire) {
    inReg     := io.in.bits
    inValid   := true.B
  } .otherwise {
    inValid   := false.B
  }

  stages(0).io.in.state := inReg.hinit
  stages(0).io.in.w     := inReg.block
  stages(0).io.in.valid := inValid
  for (t <- 1 until 64) stages(t).io.in := stages(t - 1).io.out

  // delay hinit by 64 cycles so we can add it back (chaining value)
  val initSR = Reg(Vec(64, new HashState))
  when (inValid) { initSR(0) := inReg.hinit }
  for (i <- 1 until 64) initSR(i) := initSR(i - 1)

  val validSR = RegInit(VecInit(Seq.fill(64)(false.B)))
  validSR(0) := inValid
  for (i <- 1 until 64) validSR(i) := validSR(i - 1)

  val last   = stages(63).io.out
  val sf     = Seq(last.state.a, last.state.b, last.state.c, last.state.d,
                   last.state.e, last.state.f, last.state.g, last.state.h)
  val inf    = Seq(initSR(63).a, initSR(63).b, initSR(63).c, initSR(63).d,
                   initSR(63).e, initSR(63).f, initSR(63).g, initSR(63).h)
  val digest = Wire(Vec(8, UInt(32.W)))
  for (i <- 0 until 8) digest(i) := sf(i) + inf(i)

  io.out.valid := validSR(63)
  io.out.bits.digest := digest
}
