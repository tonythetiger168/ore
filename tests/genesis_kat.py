#!/usr/bin/env python3
"""Bitcoin genesis-block known-answer test data (tests/genesis_kat.json).

The genesis block is the perfect KAT: template, target, and solution nonce
(2083236893 = 0x7C2BAC1D) are all public, and its hash is famous.
Use in RTL sim: load template with sim target = all-ones, scan from nonce 0,
expect foundNonce=0 and digest == sim_vectors[0].outer_words.
Use on FPGA/silicon soak: real target + real_nonce (takes ~2.1G cycles @1/cyc).
"""
import json, os
if __name__ == "__main__":
    doc = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                      "genesis_kat.json")))
    print(doc["name"], "real_nonce =", doc["real_nonce"],
          "= 0x%X" % doc["real_nonce"])
