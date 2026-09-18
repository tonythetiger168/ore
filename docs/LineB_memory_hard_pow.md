# Line B: memory-hard PoW engine (design spec, DRAFT)

Goal: retargetable memory-hard PoW in SoC form. Reference algorithm:
Scrypt-class (ROMix), the most instructive memory-hard primitive.

## Why memory-hard changes the architecture

SHA-256d engines are compute-bound: E/hash set by adder capacitance.
Memory-hard PoW is BANDWIDTH-bound: Scrypt(N=2^20, r=8) touches ~1 GB
per hash. Energy per hash is then dominated by SRAM reads:
~1e9 reads x ~1-5 pJ/SRAM-read ~= 1-5 J/GH... i.e. memory-hard ASICs are
~100x less energy-efficient than SHA ASICs BY DESIGN -- that is the
security property (it erases the ASIC advantage over GPUs).

## Engine architecture (Scrypt reference)

Per lane: PBKDF2-HMAC-SHA256 (uses our existing SHA-256 pipe!) ->
ROMix(N): loop 1 writes V[0..N) scratchpad (128B x N = 128KB x N x r...;
N=2^15 -> 4MB per lane on-chip SRAM, or N=2^20 -> 1GB external LPDDR),
loop 2 random-reads V[j] (random index = the memory-hard part) ->
blockmix SMix (Salsa20/8) -> final PBKDF2.

## Hardware plan

- Reuse: SHA-256 pipeline (PBKDF2), MMIO map, stratum clients, DVFS,
  SMART masking -- the entire control plane carries over unchanged.
- New: Salsa20/8 round unit (cheap: XOR/rotate/add array), scratchpad
  controller (on-chip SRAM macros 4-16MB, or LPDDR4 controller for
  N>2^18), random-access arbiter (banked SRAM to feed 1+ read/cycle).
- Energy honesty: at memory-hard, LOW-POWER work shifts from the round
  logic to the memory system (banked SRAM access energy, LPDDR I/O) --
  LOW_POWER_DESIGN.md levers 1-3 still apply, plus SRAM banking and
  I/O minimization.
- Numbers (N=2^15, r=8, on-chip): ~4MB SRAM/lane x 8 lanes = 32MB --
  large but feasible at 28nm; at N=2^20 move to LPDDR4 x32 @ 8-16 Gb/s.

## Open questions

- Target algorithm list beyond Scrypt (EthashNG? RandomX is CPU-bound,
  hostile to ASIC -- out of scope).
- On-chip SRAM vs LPDDR: N ceiling, cost, energy. Decided at Line B
  architecture freeze (Phase 2, PRODUCTS.md).
