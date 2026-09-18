# Liquid-cooling support design (Line C / heat-reuse)

Scope: chip sensors -> module cold plate -> rack CDU -> facility or
heat-reuse loop. Single-phase direct-to-chip (D2C) is the v1 choice;
two-phase and immersion are compared in section 6. Dev kits (Line A)
stay air-cooled; this design serves Line C modules (G3-gated).

## 0. Honest starting point

Our v10-class chip is SMALL thermally: 64 engines x 128 rounds x 0.45pJ x
1GHz ~= 3.7W. No single chip needs water. Liquid enters when we scale to
MODULES: e.g. 128 chips x 3W = ~400W per hash module, 4-8 modules per
2U unit -> 1.6-3.2kW per unit. Design targets THAT, and targets heat
REUSE (the only Line C market per BUSINESS.md): capture energy as
55-60C hot water instead of throwing it into facility air.

## 1. Thermal budget (worked example, per hash module)

Assume: P = 400W over N=128 chips -> 3.1W/chip, Tj_max = 80C (reliability;
near-Vt leakage also rewards low Tj), coolant inlet 45C (heat-reuse mode).

    Tj = T_in + P_chip x (theta_jc + theta_TIM + theta_plate)
       = 45 + 3.1 x (0.10 + 0.05 + 0.05) = 45.6 C        [huge margin]

Module-level (cold plate as one surface):
    theta_plate_total = (Tj - T_in)/P_module
                      = (70 - 45)/400 = 0.0625 K/W       design target

Flow sizing:  m_dot = P / (cp x dT)
    service mode dT=10K:  m = 400/4186/10 = 9.6 g/s ~ 0.58 L/min
    heat-reuse mode dT=35K (20->55C):  m = 0.16 L/min  (tiny; use
    low-flow + mixing valve to keep plate dT uniform)

Chip count scaling table (same 0.06 K/W module):
    32 chips:  100W module,  Tj-T_in ~ 9K
    128 chips: 400W module,  Tj-T_in ~ 25K (comfortable)
    256 chips: 800W module,  Tj-T_in ~ 50K -> need 0.03 K/W or lower Tin

## 2. Architecture

    [chips on cold plate] --G1/4--> [module manifold] --UQD-->
    [rack manifold] --> [CDU: pump + HX + reservoir + controls] -->
    (a) facility water loop, or (b) reuse loop: mixing tank ->
    radiant floor / DHW pre-heat -> dry cooler as trim.

- Cold plate: Cu microchannel, skived or machined, serial-mini-parallel
  channels under the chip array; 0.05-0.06 K/W/module class (commercially
  standard). Design for 0.5-1.5 L/min and 30-60 kPa drop.
- TIM: thin-BTU phase-change sheet (e.g. PTM class), controlled mounting
  pressure 20-40 psi via ILM backplate; specify pump-out life >= 5yr at
  module dT.
- Quick disconnects: UQD/UQDB-class, dripless; one per module. Spare
  ports for filling loop.
- Materials: Cu plate + Cu manifold + EPDM hoses; DO NOT mix Al radiator
  with Cu loop unless inhibited + isolated (galvanic). Radiator/dry
  cooler in Al requires isolated loop or DI water + inhibitor package.
- Coolant: DI water + biocide + corrosion inhibitor (or PG25 where
  freeze risk exists). Conductivity sensor doubles as leak/coolant-loss
  early warning.

## 3. Sensors & electronics (feed the control firmware)

On-chip: reuse foundry diode per engine zone (4 zones/chip) -> I2C/SMBus
via board MCU (BMC), or direct to SoC ADC if available. Board/module:
T_in, T_out (1k NTC or digital), flow (paddle/pulse), leak (conductivity
point sensor under manifold + drip tray), pump tach + PWM control, CDU
level switch. All exposed over the existing MMIO map (0xC0+ sketch in
hw/ThermalRegs.scala) so Linux firmware + dvfs_daemon own the loop.

## 4. Control policy (implemented + host-tested in sw/dvfs_daemon.c)

thermal_policy() verdicts (pure function, 5 scenarios tested):
    leak / flow-loss        -> alarm 2, initiate shutdown sequence
    T_in < dewpoint + 3K    -> derate 1 step + alarm 1 (condensation
                               guard; warm-water operation makes this
                               rare -- a selling point vs sub-ambient)
    Tj_est > Tj_max         -> derate 2 steps + alarm 1
    reuse_mode headroom     -> +1 step (harvest heat when coolant can
                               take it: "heat-follow mining")

Shutdown sequence on alarm 2: engines finish current window -> clk gate
engines (LowPowerAdditions 0xB0) -> keep BMC + sensors alive -> host
decides restart after fault clears. Dewpoint from onboard RH/T sensor.

## 5. FMEA (top failure modes)

| Mode            | Detection              | Response                        |
|-----------------|------------------------|---------------------------------|
| Coolant leak    | point sensor + cond.   | alarm2, shutdown, solenoid iso. |
| Pump failure    | tach/flow loss         | alarm2, shutdown                |
| Flow restriction| dT plate rise at const P| derate, flag service           |
| Fouling/scaling | dP creep over months   | maintenance counter, alert      |
| Condensation    | T_in vs dewpoint       | derate + alarm1 (sec. 4)        |
| Coolant loss    | CDU level switch       | alarm2, shutdown                |
| Sensor fault    | out-of-range readings  | plausibility check, fail-safe to derate |

## 6. Two-phase / immersion: why not v1

| Option            | Efficiency | Complexity | Reuse fit | Verdict |
|-------------------|-----------|------------|-----------|---------|
| Single-phase D2C  | good      | low        | excellent | v1      |
| Two-phase (R1233zd-ish) | best | med-high  | good      | v2 eval |
| Immersion (tanks) | good      | high       | poor      | reject for reuse; fine for datacenter density |

## 7. Standards & verification checklist

- OCP Advanced Cooling Solutions cold-plate spec alignment (ORS footprint)
- ASHRAE W27/W32/W40 inlet classes; Line C targets W32-continuous
- UQD dripless disconnects; RoHS materials; pressure-test every module
  (1.5x working pressure, 30 min) at production
- Verification: CFD on A-16 package before tapeout (P2); calorimetric
  acceptance per production module (measure dT x flow = P, compare to
  electrical P, tolerance 5%)

## 8. Line C product implications (PRODUCTS.md cross-ref)

- Module: 128-chip cold plate, 400W, UQD in/out, leak tray, BMC board
- Unit: 4 modules/2U + CDU, 1.6kW; reuse loop: mixing tank, target
  outlet 55-60C at <= 0.7 L/min total
- Seasonal modes: space-heating season (55C), DHW pre-heat (60C),
  summer trim via dry cooler
