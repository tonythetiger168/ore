# Stratum V2 mapping for the mining SoC

Status: DESIGN DOC + UNVERIFIED skeleton. Before any production use, run
the official SV2 interoperability test vectors (stratum-mining/sv2-spec)
against `stratum_v2_client.c`.

## Why SV2 (A4 in BUSINESS.md)

- Encrypted + authenticated transport (Noise): kills share-hijack and
  Antbleed-style tampering on the wire. SV1 is plaintext JSON-RPC.
- Binary framing: ~10-100x less parse CPU on the control core -- matters
  when the control core is a small Rocket.
- Template/prevhash separation enables smarter template caching.
- Aligns us with Block Proto's open-architecture playbook.

## Transport & framing

- TCP, then Noise handshake (commonly NK: pool knows miner's static key;
  some deployments use NN). Encrypted thereafter.
- Frame: `u24le message_length | u12le+4 msg_type | payload` (little-endian
  fields throughout SV2 -- note the OPPOSITE of our SHA-256 BE world).

## Message mapping to our stack

| SV2 message          | Direction  | Our action |
|----------------------|-----------|------------|
| SetupConnection{protocol=MiningProtocol, min/max_version=2, flags} | -> pool | on connect |
| SetupConnection.Success{used_version, flags} | <- | enable channels |
| NewTemplate{template_id, coinbase_tx_version, coinbase_prefix, coinbase_tx_input_sequence, coinbase_tx_outputs, coinbase_tx_locktime, merkle_path, standard_job=true} | <- | build coinbase = coinbase_prefix + our extranonce; cb_hash=dsha(coinbase); fold merkle_path -> merkle root; STORE as pending template |
| SetNewPrevHash{template_id, prev_hash, header_timestamp, nbits, target} | <- | assemble header (version from template/flags); **reload engine template** (prevhash lives in Block1 -> midstate must be recomputed) |
| SubmitShares{channel_id, sequence_number, job_id, nonce, ntime, version} | -> | on engine hit: nonce from R_NONCE_FOUND; ntime = header ntime |
| SubmitShares.Success / .Error | <- | telemetry + share accounting |

Channel notes: one Standard channel per (template,prevhash) pair is the
simplest correct subset. Group/Extended channels and SetCustomMiningJob
are deliberately out of scope for the skeleton.

## Endianness contract (adds to our existing rules)

- SV2 wire fields: LITTLE-endian (incl. ntime, nbits, target, nonce).
- Header assembly for the engine stays as before: header bytes are the
  wire-format header; template regs are big-endian words (unchanged).
- So the ONLY new conversion: SV2 LE uint32 fields -> header LE bytes is a
  straight copy; do NOT byte-swap SV2 fields the way stratum v1 required.
