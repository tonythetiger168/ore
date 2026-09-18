# Mining SoC with RISC-V BOOM + SHA-256d accelerator

Reference implementation for the blog-post series "Build a mining chip with
BOOM": 1x small BOOM core runs Linux + stratum client; 8 SHA-256d mining
engines do the actual hashing at 1 nonce/cycle/engine.

## Repo layout

    hw/sha256.scala         64-stage fully-pipelined SHA-256 compression core
    hw/MiningAccel.scala    mining engine FSM + TileLink MMIO wrapper + Chipyard config
    hw/ControlCoreConfig.scala  trimmed-BOOM fragment (no FPU, small caches)
    hw/RocketConfig.scala   Rocket-based configs: RocketMiningConfig (primary,
                            tapeout-oriented), RocketMiningDualConfig, and the
                            WithTinyControlRocket trim fragment
    sw/miner_baremetal.c    OS-less bring-up smoke test (HTIF exit code)
    sw/miner_driver.c       Linux userspace driver (mmap register layer)
    sw/stratum_client.c     COMPLETE stratum client (subscribe/authorize/notify/
                            submit, merkle fold, share target) -- verified
    tests/golden.py         golden reference model (pure-python SHA-256 w/ midstate)
    tests/vectors.json      generated golden vectors
    tests/Sha256UnitTest.scala   chiseltest unit test ("abc" known answer)
    tests/MiningEngineTest.scala engine test against vectors.json

## Hardware data path (Bitcoin, double-SHA256)

    Block1 (bytes 0..63, no nonce)
      midstate = compress(H0, Block1)              <- once per template,
                                                      computed BY the inner pipe
    Block2 (bytes 64..79 + padding, nonce in W[3])
      inner  = compress(midstate, Block2(nonce))   <- 64 rounds, streamed
      outer  = compress(H0, pad(inner))            <- 64 rounds, fully constant pad
      hit    = outer <= target                     <- a-word = MSB

Per-engine throughput: 1 nonce/cycle sustained (128-cycle latency, pipelined).
8 engines @ 1 GHz = 8 Ghash/s per chip.

## Endianness contract (the #1 source of mining-hardware bugs)

- All MMIO words are BIG-ENDIAN interpretations of header bytes as SHA-256
  sees them (software byte-swaps each 32-bit field when writing).
- The integer nonce is byte-reversed into W[3] inside block2Gen (Bitcoin
  header fields are little-endian on the wire). foundNonce is reported as the
  true header nonce (pre-reversal value), ready for stratum submit.
- Bitcoin compares the double-SHA digest as a LITTLE-ENDIAN 256-bit
  integer (Bitcoin Core arith_uint256): byte-reverse the whole 32-byte
  digest and read it as a big-endian number. Hardware:
  Cat(digest.reverse.map(bswap32)) <= target, with the target written by
  software in display order (MSW @0x60). The genesis-block KAT caught
  this TWICE (first BE words, then BE bytes within words) -- ALWAYS
  validate against a known historical block before tapeout.

## Register map (base 0x10020000)

    0x00  W   ctrl        bit0=start  bit1=load_template  bit2=clear
    0x04  R   status      bit0=busy   bit1=done
    0x08  RW  irq_en      -> PLIC source 0
    0x0C  RW  nonce_start
    0x10  RW  block1_w[0..15]   (0x10 + 4*i)
    0x50  RW  b2_w[0..2]        (merkle tail | time | bits)
    0x5C  RW  nonce_stride      (per-engine split, default 0x02000000)
    0x60  RW  target[255:0]     (8 words, word @0x60 = MSW)
    0x80  R   nonce_found
    0x84  R   digest_found[255:0]
    0xA4  R   found (sticky; clear with ctrl.bit2)

## Software sequence

    1. write block1_w, b2_w, target
    2. ctrl = 0x2  (load_template; engines compute midstate, ~70 cycles)
    3. poll status.busy == 0
    4. write nonce_start; ctrl = 0x1 (start)
    5. poll found / wait IRQ; read nonce_found, digest_found
    6. ctrl = 0x4 to clear, go to 4 with next window or 1 for new template

## Which control core? Rocket vs BOOM

Rocket is the recommended control core for this SoC (in-order, ~5x smaller
than SmallBoom, trivial timing closure at any node, fast simulation); BOOM
remains useful for bring-up and as a learning vehicle. Both configs share
the identical accelerator + software (the control core only sees TileLink).

## Building inside Chipyard

    # from chipyard root; copy or symlink this repo into generators/mining
    cd sims/verilator
    make CONFIG=RocketMiningConfig        # Rocket control core (tapeout)
    make CONFIG=BoomMiningConfig          # BOOM control core (bring-up/dev)
    # smoke test (genesis template, target=all-ones, expect nonce 0):
    make CONFIG=RocketMiningConfig run-binary BINARY=miner_baremetal.riscv
    # bare-metal smoke test:
    make CONFIG=BoomMiningConfig run-binary BINARY=miner-baremetal.riscv
    # FireSim:
    cd sims/firesim && make config-recipe CONFIG=BoomMiningConfig ...

    # Scala unit tests (need chiseltest + spray-json on the classpath):
    cd generators/mining && sbt "testOnly mining.Sha256PipeTest mining.MiningEngineTest"

    # golden vectors:
    python3 tests/golden.py

    # stratum client logic test (no hardware needed; cross-check vs hashlib):
    gcc -O1 -DSTRATUM_TEST sw/stratum_client.c -o /tmp/st
    printf 'SUBSCRIBE_RESULT {"id":1,"result":[[["mining.set_difficulty","d"],["mining.notify","n"]],"c0ffee00",4]}\n' \
      'NOTIFY {"id":null,"method":"mining.notify","params":["bf41","0000000000000000000000000000000000000000000000000000000000000000","aabbccddee","ffeeddccbbaa",["1111111111111111111111111111111111111111111111111111111111111111","2222222222222222222222222222222222222222222222222222222222222222"],"20000000","1d00ffff","66f2b8a0",false]}\n' \
      'DIFF 1024\nABC\n' | /tmp/st
    # expected: MERKLE 68cad05d..., ABC 4f8b42c2... (double-sha256 of "abc")

## Known simplifications (production TODOs)

1. No backpressure between pipes: consumer must accept 1 result/cycle.
   Add skid buffers if the outer pipe ever stalls (multi-bank target compare,
   throttling on thermal events).
2. Nonce window split is static (stride). Production: per-engine dynamic
   work queues or rollover chaining.
3. Target compare is exact-256-bit; real pools hand you a 256-bit target
   already expanded from 'bits', so this is fine -- just never compare
   against the *encoded* compact form.
4. Power/clock gating of idle engines is left as an exercise (ASIC tapeout
   item: per-engine clock gates + nonce-range done interrupts).
5. The RegWriteFn target update path clears-and-sets one word; make sure
   software writes all 8 words before load_template.
6. SubsystemInjector / pbus.coupleTo API names shift between Chipyard
   releases -- check against your chipyard version's custom-device docs.

## Bring-up checklist

- [ ] Sha256PipeTest green ("abc" known answer)
- [ ] golden.py selftest green (matches hashlib incl. midstate path)
- [ ] MiningEngineTest green on vectors.json (midstate + 4 nonces)
- [ ] Verilator full-chip: BOOM writes template, engine finds a nonce for a
      historical block header with a known solution nonce
- [ ] FireSim: Linux boots, miner_driver mmap OK, stratum connect OK,
      share submits accepted by a test pool (regtest/testnet)
- [ ] FPGA (VCU118-class): 100-200 MHz, error-rate soak 24h
- [ ] 130nm shuttle (SkyWater/Efabless): PLL, IO, DFT, scan chain, power OK


## Lessons: 8 bugs caught by verification (all fixed)

1.  golden/RTL: nonce must be byte-reversed into W[3] (header fields are LE).
2.  golden/RTL: Bitcoin compares the digest as a LITTLE-ENDIAN 256-bit int
    (word order reversed).
3.  golden/RTL: ... AND byte order within each word is reversed too
    (genesis KAT caught both layers separately).
4.  stratum JSON parser: key/value separator ':' was never consumed.
5.  stratum JSON parser: object sibling chain overwrote key->value links.
6.  stratum hex2bin: read h[1] past the NUL terminator for even-length
    strings (UB; passed by luck on some layouts, failed on others).
7.  stratum sha256_fin: padding bytes went through sha256_upd, corrupting
    the bit counter before the length field was written.
8.  stratum sha256_fin: SHA-256's 64-bit length field is BIG-ENDIAN
9.  design-doc (not code): rounds 0..17 of Block 2 were claimed
    nonce-independent -- actually only rounds 0..2 are; the chaining
    add-back must use the original hinit, not the segment start. Caught by
    extending the golden model before writing any RTL.
    (C wrote it LE; empty message masked this because 0 == 0 in either
    order, and the "known-answer" blk test only passed because the manual
    vector happened to encode the length big-endian).

Moral: every one of these survived code review. Only golden-vector
cross-checks (hashlib for crypto, python-vs-C for the client) killed them.
Never tape out logic that has not matched an independent golden model.


## Rocket / BOOM configs

    make CONFIG=RocketMiningConfig        # Rocket control core (tapeout)
    make CONFIG=RocketMiningDualConfig    # 2x Rocket: miner + monitor
    make CONFIG=BoomMiningConfig          # BOOM control core (bring-up/dev)

## state3 precompute (optional ~2% optimization)

tests/golden.py now emits a `state3` vector: inner rounds 0..2 use only
constant W[0..2], so they can run at template load. Corrections vs earlier
notes: (a) round 3 already consumes W[3]=nonce -- only 3 rounds are
precomputable, NOT 18 (the W[16]/W[17]-are-constant observation does not
help rounds 3..15); (b) the chaining add-back still uses the ORIGINAL hinit
(midstate) -- carry it to the final adder, do not add state3.


## Improvement-plan status (started 2026-09)

- [x] A0 benchmarks defined (BENCHMARKS.md, named targets: BZM2 / SMART / Proto)
- [x] A1 feasibility model: CSA+DVFS @28nm ~18-32 J/TH (docs/a1_model.json)
- [ ] A1 RTL: `useCSA` parameter in hw/sha256.scala, same golden vectors
- [ ] A2 16nm open-PDK shuttle scouting
- [ ] A3 engine_manager.c integration; RTL TODO: per-engine ENABLE bit
      (masking currently needs empty-window surgery)
- [ ] A4 sv2_client.c skeleton per docs/stratum_v2.md
- Kill criterion: B1 > 40 J/TH @28nm gate-level -> drop product Line C.
