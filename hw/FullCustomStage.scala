package mining

import chisel3._
import chisel3.util._

// ---------------------------------------------------------------------------
// SKETCH -- A1 full-custom-style round using a carry-save adder tree.
// Arithmetic gated on tests/csa_model.py (20k random vectors). UNVERIFIED
// as RTL: needs chiseltest (reuse MiningEngineTest -- same vectors) and
// gate-level energy numbers before it replaces Sha256Stage.
//
// T1 = h + bsig1(e) + ch(e,f,g) + K(t) + W(t):  5 terms.
//   level 1: csa(h,  bsig1,   ch)   -> s1, c1
//            csa(K,  W,       s1)   -> s2, c2
//   level 2: csa(s2, c1,      c2)   -> s3, c3
//   final  : T1 = s3 + c3            (one carry-propagate add)
// T2 = bsig0(a) + maj(a,b,c): 3 terms -> one CSA row, share the final CPA.
// FA/round ~128 vs ~192 naive RCA (tests/csa_model.py accounting).
// ---------------------------------------------------------------------------

class Csa32 extends Module {
  // one full-adder row: out_s = a^b^cin per bit; out_c = majority, shifted up
  val io = IO(new Bundle {
    val a, b, c = Input(UInt(32.W))
    val s       = Output(UInt(32.W))
    val cout    = Output(UInt(32.W))   // carry out, already shifted left 1
  })
  val p = io.a ^ io.b ^ io.c
  val g = (io.a & io.b) | (io.c & (io.a ^ io.b))
  io.s    := p
  io.cout := g(30, 0) ## 0.U(1.W)      // bit31 carry feeds the final CPA
}

class Sha256StageCsa(t: Int) extends Module {
  val io = IO(new Bundle { val in = Input(new StageData); val out = Output(new StageData) })

  def rotr(x: UInt, n: Int) = (x >> n) | (x << (32 - n))
  def bsig1(x: UInt) = rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25)
  def bsig0(x: UInt) = rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22)
  def ssig1(x: UInt) = rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10)
  def ssig0(x: UInt) = rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3)
  def ch (x: UInt, y: UInt, z: UInt) = (x & y) ^ (~x & z)
  def maj(x: UInt, y: UInt, z: UInt) = (x & y) ^ (x & z) ^ (y & z)

  val s = io.in.state
  val csa1 = Module(new Csa32); csa1.io.a := s.h; csa1.io.b := bsig1(s.e); csa1.io.c := ch(s.e, s.f, s.g)
  val csa2 = Module(new Csa32); csa2.io.a := Sha256.K(t); csa2.io.b := io.in.w(0); csa2.io.c := csa1.io.s
  val csa3 = Module(new Csa32); csa3.io.a := csa2.io.s; csa3.io.b := csa1.io.cout; csa3.io.cout := DontCare
  csa3.io.c := csa2.io.cout
  val T1 = csa3.io.s +& csa3.io.cout          // final carry-propagate add
  val T2z = Wire(UInt(33.W)); T2z := bsig0(s.a) +& maj(s.a, s.b, s.c)

  val ns = Wire(new HashState)
  ns.a := T1(31, 0) + T2z(31, 0)
  ns.b := s.a; ns.c := s.b; ns.d := s.c
  ns.e := s.d + T1(31, 0)
  ns.f := s.e; ns.g := s.f; ns.h := s.g

  val wNext = ssig1(io.in.w(14)) + io.in.w(9) + ssig0(io.in.w(1)) + io.in.w(0)
  io.out.state := ns
  for (i <- 0 until 15) io.out.w(i) := io.in.w(i + 1)
  io.out.w(15) := wNext
  io.out.valid := io.in.valid
}
