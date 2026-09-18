package mining

import chisel3._
import chiseltest._
import org.scalatest.flatspec.AnyFlatSpec

class Sha256PipeTest extends AnyFlatSpec with chiseltest.ChiselScalatestTester {
  behavior of "Sha256Pipe"

  def pokeInit(c: Sha256Pipe): Unit = {
    c.io.in.bits.hinit.a.poke(0x6a09e667L.U); c.io.in.bits.hinit.b.poke(0xbb67ae85L.U)
    c.io.in.bits.hinit.c.poke(0x3c6ef372L.U); c.io.in.bits.hinit.d.poke(0xa54ff53aL.U)
    c.io.in.bits.hinit.e.poke(0x510e527fL.U); c.io.in.bits.hinit.f.poke(0x9b05688cL.U)
    c.io.in.bits.hinit.g.poke(0x1f83d9abL.U); c.io.in.bits.hinit.h.poke(0x5be0cd19L.U)
  }

  it should "hash 'abc' (single padded block)" in {
    test(new Sha256Pipe) { c =>
      pokeInit(c)
      // "abc" || 0x80 || zeros(52) || be64(24)
      c.io.in.bits.block(0).poke(0x61626380L.U)
      for (i <- 1 until 15) c.io.in.bits.block(i).poke(0.U)
      c.io.in.bits.block(15).poke(24.U)
      c.io.in.valid.poke(true.B); c.clock.step()
      c.io.in.valid.poke(false.B)
      c.clock.step(70)
      c.io.out.valid.expect(true.B)
      // sha256("abc") = ba7816bf 8f01cfea 414140de 5dae2223
      //                 b00361a3 96177a9c b410ff61 f20015ad
      c.io.out.bits.digest(0).expect(0xba7816bfL.U)
      c.io.out.bits.digest(1).expect(0x8f01cfeaL.U)
      c.io.out.bits.digest(2).expect(0x414140deL.U)
      c.io.out.bits.digest(3).expect(0x5dae2223L.U)
      c.io.out.bits.digest(4).expect(0xb00361a3L.U)
      c.io.out.bits.digest(5).expect(0x96177a9cL.U)
      c.io.out.bits.digest(6).expect(0xb410ff61L.U)
      c.io.out.bits.digest(7).expect(0xf20015adL.U)
    }
  }

}
// Engine-level golden-vector harness: drive MiningEngine with the template
// from tests/vectors.json, pulse load -> start, and check foundNonce /
// foundDigest for each vector nonce (run under chiseltest with file IO).
