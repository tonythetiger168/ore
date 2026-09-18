# Clocking & CDC architecture

## Domains

| Domain          | Source            | Frequency        | Contents |
|-----------------|-------------------|------------------|----------|
| bus_clk         | SoC PLL           | e.g. 100-500 MHz | Rocket/TileLink, MMIO reg files, PLIC |
| eng_clk         | separate PLL/MMCM | 0.5-1.5 GHz      | hash pipes (64-stage x n), engine FSMs |
| bmc_clk         | ext. osc/BMC      | 100 kHz-24 MHz   | I2C sensors, pump/fan PWM (board MCU) |

eng_clk may be per-engine-gated (PowerRegs 0xB0) but NOT per-engine
frequency in v1 (one PLL output; gating only). VSEL (0xB8) scales the
shared rail.

## CDC points (must all be async-FIFO or pulse-sync)

1. bus->eng : template regs, start/load/clear pulses, nonceStart/End.
   Template regs are quasi-static; requirement is they be stable >= 1
   eng_clk before the load pulse (pulse is double-flop synced; template
   writes hold in bus domain until ack from eng domain FSM).
2. eng->bus : found/done/busy, foundNonce/foundDigest. Use Req/Ack
   handshake with data held in eng domain until bus samples (one-shot
   event bits + sticky capture regs -- ALREADY implemented in
   MiningEngine sticky regs).
3. bmc->bus : sensor values (ThermalRegs expects them pre-synchronized:
   BMC writes through a small async FIFO or holds values stable >= 2
   bus_clk with a data-valid toggle).
4. Reset: eng domain reset must be synchronized into eng_clk; bus-domain
   reset into bus_clk. Never fan out one reset unsynchronized.

## Rules

- No combinational paths across domains. All crossings through explicit
  synchronizers, reviewed at code review with a CDC checklist.
- eng_clk gating: clock-gate enable must be glitch-free (ICG); enable is
  generated in bus domain and synchronized to eng_clk before use.
- Simulation: run with `--x-initial unique` + randomized delays to catch
  missing synchronizers; gate-level sim with SDF for sign-off corners.
