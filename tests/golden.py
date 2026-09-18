#!/usr/bin/env python3
"""
Golden reference model for the mining accelerator.

Implements SHA-256 compression by hand so we can expose the *midstate*
(the chaining value after header Block 1), which hashlib cannot give us
directly. Also validates everything against hashlib.

Bitcoin header (80 bytes, little-endian fields on the wire):
  version(4) | prevhash(32) | merkle(32) | time(4) | bits(4) | nonce(4)

SHA-256 sees the header as two 512-bit blocks:
  Block 1 = bytes  0..63  (no nonce)
  Block 2 = bytes 64..79  | 0x80 | 39 zeros | 0x280   (nonce in W[3])
Outer hash input = 32-byte inner digest -> single block:
  W[0..7] = digest, W[8] = 0x80000000, W[9..14] = 0, W[15] = 256
"""
import json, struct, hashlib, os

MASK = 0xFFFFFFFF
K = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]
H0 = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]

def rotr(x, n): return ((x >> n) | (x << (32 - n))) & MASK

def compress(state, block_words):
    w = list(block_words) + [0] * 48
    for t in range(16, 64):
        s1 = rotr(w[t-2], 17) ^ rotr(w[t-2], 19) ^ (w[t-2] >> 10)
        s0 = rotr(w[t-15], 7) ^ rotr(w[t-15], 18) ^ (w[t-15] >> 3)
        w[t] = (s1 + w[t-7] + s0 + w[t-16]) & MASK
    a, b, c, d, e, f, g, h = state
    for t in range(64):
        S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)
        ch = (e & f) ^ (~e & g & MASK)
        t1 = (h + S1 + ch + K[t] + w[t]) & MASK
        S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)
        maj = (a & b) ^ (a & c) ^ (b & c)
        t2 = (S0 + maj) & MASK
        h, g, f, e, d, c, b, a = g, f, e, (d + t1) & MASK, c, b, a, (t1 + t2) & MASK
    return [(x + y) & MASK for x, y in zip(state, [a, b, c, d, e, f, g, h])]

def be_words(b):
    assert len(b) % 4 == 0
    return list(struct.unpack(">%dI" % (len(b) // 4), b))

def header_blocks(version, prevhash, merkle, time_, bits, nonce):
    """Return (block1_words[16], block2_words[16]) exactly as SHA-256 sees them."""
    hdr = (struct.pack("<i", version) + prevhash + merkle +
           struct.pack("<III", time_, bits, nonce))
    assert len(hdr) == 80
    b1 = be_words(hdr[0:64])
    b2 = be_words(hdr[64:80])                 # merkle tail | time | bits | nonce
    assert len(b2) == 4
    block2 = b2 + [0x80000000] + [0] * 10 + [640]
    return b1, block2

def midstate(block1_words):
    return compress(H0, block1_words)

def outer_block(inner_digest_words):
    return inner_digest_words[:8] + [0x80000000] + [0] * 6 + [256]

def bswap32(x):
    return struct.unpack(">I", struct.pack("<I", x & MASK))[0]

def mine_once(block1, block2_const, nonce):
    """Full double-SHA256 for one nonce, replicating the hardware data path.

    Endianness trap: header fields are little-endian on the wire, so the
    SHA-256 message word W[3] is the BYTE-REVERSAL of the integer nonce.
    (Real miners often skip the swap and walk the permuted space instead,
    since byte reversal is a bijection -- but then found nonces must be
    un-reversed before submission. We swap in the datapath and report the
    true nonce directly.)
    """
    b2 = list(block2_const); b2[3] = bswap32(nonce)
    inner = compress(midstate(block1), b2)
    outer = compress(H0, outer_block(inner))
    # Bitcoin compares the digest as a LITTLE-ENDIAN 256-bit integer:
    # word a (first 4 digest bytes) is the LEAST significant word.
    # (Genesis-block KAT caught this: int(digest,'big') FAILS the target
    # check, int(digest,'little') passes -- see CheckProofOfWork /
    # arith_uint256 in Bitcoin Core.)
    val = 0
    for i, w in enumerate(outer): val |= bswap32(w) << (32 * i)
    return val, outer

# --------------------------------------------------------------------------
# self-checks against hashlib
# --------------------------------------------------------------------------
def selftest():
    # 1) plain SHA-256 of "abc" through our compress
    msg = b"abc"
    blk = be_words(msg + b"\x80" + b"\x00" * 52 + struct.pack(">Q", 24))
    d = compress(H0, blk)
    expect = hashlib.sha256(msg).digest()
    got = b"".join(struct.pack(">I", w) for w in d)
    assert got == expect, "sha256 core mismatch"
    print("[ok] sha256 core matches hashlib")

    # 2) double-sha256 of an 80-byte header vs hardware-style path
    version, time_, bits, nonce = 1, 0x5EE5C900, 0x1A2B3C4D, 0xDEADBEEF
    prevhash = bytes.fromhex("00" * 32)
    merkle   = bytes.fromhex("11" * 32)
    hdr = (struct.pack("<i", version) + prevhash + merkle +
           struct.pack("<III", time_, bits, nonce))
    ref = hashlib.sha256(hashlib.sha256(hdr).digest()).digest()
    b1, b2c = header_blocks(version, prevhash, merkle, time_, bits, 0)
    val, outer = mine_once(b1, b2c, nonce)
    got = b"".join(struct.pack(">I", w) for w in outer)
    assert got == ref, "double-sha path mismatch"
    assert val == int.from_bytes(ref, "little"), "LE-integer check"
    print("[ok] midstate + inner + outer path matches hashlib double-SHA256")

    # 3) W[3] is the only nonce-dependent word of block 2
    _, b2a = header_blocks(version, prevhash, merkle, time_, bits, 0x11111111)
    _, b2b = header_blocks(version, prevhash, merkle, time_, bits, 0x22222222)
    diff = [i for i in range(16) if b2a[i] != b2b[i]]
    assert diff == [3], diff
    print("[ok] nonce only touches W[3] of block 2")
    return True

def gen_vectors(path):
    """Emit JSON vectors consumed by the Chisel testbench."""
    version, time_, bits = 2, 0x60000000, 0x170FFFFF
    prevhash = bytes.fromhex("22" * 32)
    merkle   = bytes.fromhex("33" * 32)
    b1, b2c = header_blocks(version, prevhash, merkle, time_, bits, 0)
    vectors = []
    for nonce in [0, 1, 0x12345678, 0xFFFFFFFF]:
        val, outer = mine_once(b1, b2c, nonce)
        vectors.append({
            "nonce": nonce,
            "midstate": ["%08x" % w for w in midstate(b1)],
            "outer_words": ["%08x" % w for w in outer],
            "outer_int": "%064x" % val,
        })
    doc = {
        "block1_words": ["%08x" % w for w in b1],
        "block2_const_words": ["%08x" % w for w in b2c],
        "state3": ["%08x" % w for w in state3_with(b1, b2c[:3])],
        "vectors": vectors,
    }
    with open(path, "w") as f:
        json.dump(doc, f, indent=2)
    print("[ok] wrote", path)
    return doc



# ---------------------------------------------------------------------------
# Round-level precompute: rounds 0..2 of inner Block 2 use only W[0..2], all
# constant -> state3 can be computed at template load. Per nonce: 125 rounds
# instead of 128 (~2%, NOT the 14% a naive W[16]/W[17] analysis suggests --
# round 3 already consumes W[3] = nonce).
# GOTCHA: the chaining add-back uses the ORIGINAL hinit (midstate), not the
# segment start -- the hardware must carry midstate to the final adder.
# ---------------------------------------------------------------------------
def compress_rounds(state, words, t_from, t_to):
    """Run rounds t_from..t_to-1 of the compression function."""
    a,b,c,d,e,f,g,h = state
    w = list(words)
    for t in range(t_from, t_to):
        if t >= 16:
            s1 = rotr(w[t-2],17)^rotr(w[t-2],19)^(w[t-2]>>10)
            s0 = rotr(w[t-15],7)^rotr(w[t-15],18)^(w[t-15]>>3)
            w.append((s1 + w[t-7] + s0 + w[t-16]) & MASK)
        S1 = rotr(e,6)^rotr(e,11)^rotr(e,25)
        ch = (e&f)^(~e&g&MASK)
        t1 = (h+S1+ch+K[t]+w[t]) & MASK
        S0 = rotr(a,2)^rotr(a,13)^rotr(a,22)
        maj = (a&b)^(a&c)^(b&c)
        t2 = (S0+maj) & MASK
        a,b,c,d,e,f,g,h = (t1+t2)&MASK, a, b, c, (d+t1)&MASK, e, f, g
    return [a,b,c,d,e,f,g,h]

def mine_once_state3(block1_words, b2_w012, nonce):
    """Mining path starting from precomputed state3. Must equal mine_once."""
    st = state3_with(block1_words, b2_w012)
    b2 = [0]*16
    b2[0], b2[1], b2[2] = b2_w012
    b2[3] = bswap32(nonce)
    b2[4] = 0x80000000
    b2[15] = 640
    inner_raw = compress_rounds(st, b2, 3, 64)
    # chaining add-back uses the ORIGINAL hinit (midstate), not the segment
    # start -- a real implementation must carry midstate to the final adder
    inner = [(x+y) & MASK for x, y in zip(midstate(block1_words), inner_raw)]
    outer = compress(H0, outer_block(inner))
    val = 0
    for i, w in enumerate(outer): val |= bswap32(w) << (32 * i)
    return val, outer

def state3_with(block1_words, b2_w012):
    ms = midstate(block1_words)
    w = [0]*16; w[0], w[1], w[2] = b2_w012
    return compress_rounds(ms, w, 0, 3)

def verify_precompute():
    version, time_, bits = 2, 0x60000000, 0x170FFFFF
    prevhash = bytes.fromhex("22" * 32)
    merkle   = bytes.fromhex("33" * 32)
    b1, b2c = header_blocks(version, prevhash, merkle, time_, bits, 0)
    for nonce in [0, 1, 0x12345678, 0xFFFFFFFF]:
        ref, _ = mine_once(b1, b2c, nonce)
        got, _ = mine_once_state3(b1, b2c[:3], nonce)
        assert got == ref, "state3 precompute mismatch at nonce %x" % nonce
    print("[ok] state3 precompute path matches full path (rounds 0..2 only:"
          " round 3 already consumes W[3]=nonce; ~2% gain, not 14%)")

if __name__ == "__main__":
    verify_precompute()
    selftest()
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "vectors.json")
    gen_vectors(out)
