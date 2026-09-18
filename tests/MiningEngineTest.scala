package mining

import chisel3._
import chiseltest._
import org.scalatest.flatspec.AnyFlatSpec
import scala.io.Source
import spray.json._

class MiningEngineTest extends AnyFlatSpec with ChiselScalatestTester {
  behavior of "MiningEngine (golden vectors from tests/vectors.json)"

  it should "reproduce the golden double-SHA256 for each vector nonce" in {
    val jsonTxt = Source.fromFile("tests/vectors.json").mkString
    val doc = jsonTxt.parseJson.convertTo[Map[String, JsValue]]
    val b1 = doc("block1_words").convertTo[Seq[String]].map(s => BigInt(s, 16))
    val b2c = doc("block2_const_words").convertTo[Seq[String]].map(s => BigInt(s, 16))
    val vecs = doc("vectors").convertTo[Seq[Map[String, JsValue]]]

    test(new MiningEngine) { c =>
      // load template
      c.io.template.block1.zipWithIndex.foreach { case (w, i) => w.poke(b1(i).U) }
      c.io.template.b2w(0).poke(b2c(0).U)
      c.io.template.b2w(1).poke(b2c(1).U)
      c.io.template.b2w(2).poke(b2c(2).U)
      c.io.template.target.poke(BigInt(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 16).U)

      c.io.load.poke(true.B); c.clock.step(); c.io.load.poke(false.B)
      c.clock.step(80)  // midstate computed
      c.io.busy.expect(false.B)

      vecs.foreach { v =>
        val nonce = BigInt(v("nonce").convertTo[Int]).toLong & 0xFFFFFFFFL
        val outer = v("outer_words").convertTo[Seq[String]]
          .map(s => BigInt(s, 16))
        c.io.clear.poke(true.B); c.clock.step(); c.io.clear.poke(false.B)
        c.io.nonceStart.poke(nonce.U)
        c.io.nonceEnd.poke((nonce + 1).U)
        c.io.start.poke(true.B); c.clock.step(); c.io.start.poke(false.B)
        c.clock.step(300)  // > 128 pipeline latency + margin
        c.io.found.expect(true.B)
        c.io.foundNonce.expect(nonce.U)
        // foundDigest holds byte-reversed words as LE limbs:
        // slice i == bswap32(outer_words[i])
        def bswap32(x: BigInt): BigInt = {
          val b = x.toByteArray
          BigInt(1, b.reverse.padTo(4, 0.toByte).take(4))
        }
        outer.zipWithIndex.foreach { case (w, i) =>
          c.io.foundDigest(i * 32 + 31, i * 32).expect(bswap32(w).U)
        }
      }
    }
  }
}
