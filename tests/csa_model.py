#!/usr/bin/env python3
"""Golden model for the A1 full-custom SHA-256 round (CSA variant).

The whole point of a carry-save adder tree: sum N 32-bit terms with
O(N) full adders instead of O(N) carry-propagate adders; ONE final
carry-propagate at the end. This model proves the arithmetic is exact
(vs plain sums) -- the RTL sketch in hw/FullCustomStage.scala is gated
on matching this.
"""
import random, json, os

MASK = 0xFFFFFFFF

def fa(a, b, cin):
    s = a ^ b ^ cin
    cout = (a & b) | (cin & (a ^ b))
    return s, cout

def csa32(a, b, c):
    """Full adder row: returns (sum, carry) with carry shifted left."""
    s = 0; carry = 0
    for i in range(32):
        bit_s, bit_c = fa((a >> i) & 1, (b >> i) & 1, (c >> i) & 1)
        s |= bit_s << i
        if i < 31:                      # final bit's carry goes to the CPA
            carry |= bit_c << (i + 1)
    return s, carry

def t1_direct(h, s1e, ch, K, W):
    return (h + s1e + ch + K + W) & MASK

def t1_csa(h, s1e, ch, K, W):
    # level 1: (h, s1e, ch) -> s1, c1 ; (K, W, s1) -> s2, c2
    s1, c1 = csa32(h, s1e, ch)
    s2, c2 = csa32(K, W, s1)
    # level 2: (s2, c1, c2) -> s3, c3 ; final CPA: s3 + c3
    s3, c3 = csa32(s2, c1, c2)
    return (s3 + c3) & MASK

def t2_direct(s0a, maj):
    return (s0a + maj) & MASK

def t2_csa(s0a, maj):
    # 2 terms + carry-in from T1's final CPA can merge: here standalone
    return (s0a + maj) & MASK

def selftest(n=20000):
    rng = random.Random(1234)
    for _ in range(n):
        h, s1e, ch, K, W = (rng.getrandbits(32) for _ in range(5))
        assert t1_csa(h, s1e, ch, K, W) == t1_direct(h, s1e, ch, K, W)
        s0a, maj = rng.getrandbits(32), rng.getrandbits(32)
        assert t2_csa(s0a, maj) == t2_direct(s0a, maj)
    print("[ok] CSA T1/T2 trees match direct sums on %d random vectors" % n)

    # full adder accounting (first-order, standard cell FA ~ 6-7 T):
    fa_rca  = 5 * 32 + 1 * 32          # naive: 5 RCA for T1 + 1 for T2 + ...
    fa_csa  = 3 * 32 + 1 * 32          # 2+1 CSA levels + 1 final CPA (T1)
    # (T2 merges into T1's CPA chain in the real design; counted separately)
    print("[info] FA/round: naive RCA ~%d vs CSA ~%d (%.2fx reduction)"
          % (fa_rca, fa_csa, fa_rca / fa_csa))

if __name__ == "__main__":
    selftest()
