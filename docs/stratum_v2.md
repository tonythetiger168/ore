# A4: Stratum V2 -- design note (benchmark B3, protocol future-proofing)

Stratum V2 (Stratum Reference Implementation, opensource@braiins):
binary framing + Noise-NK encrypted transport + channel-based job
distribution. Motivation vs our V1 text client: bandwidth, latency,
man-in-the-middle resistance, and it is the protocol Block Proto ships.

## Framing (what to implement in sw/sv2_client.c)

    [len: u24 LE][msg_type: u8][flags: u16][payload...]
    msg types: 0x00 SetupConnection, 0x01 SetupConnectionSuccess,
               0x02 SetupConnectionError, 0x10 NewMiningJob,
               0x11 NewTemplate (group), 0x12 SetNewPrevHash,
               0x13 SubmitShares, 0x14 SubmitSharesSuccess/Error,
               0x20 SetTarget, 0x21 Reconnect, ...

## Transport
Noise protocol framework: NK handshake (pool static key known), AES-GCM
after handshake. Key material must come from provisioned pool config,
not compiled-in -- TODO: secure key storage on the SoC (efuse or TPM
via SPI).

## Mapping to our engine template
NewMiningJob carries the same template fields our MMIO already eats:
prevhash, merkle path (coinbase parts), nbits, ntime, target. The
MINING work is unchanged -- SV2 is a software-side swap of
stratum_client.c's wire codec, reusing build_job() unchanged.

## Plan
1. docs: this file. 2. sv2 framing + handshake skeleton. 3. interop test
against a public SV2 pool (testnet). Target D+180.
