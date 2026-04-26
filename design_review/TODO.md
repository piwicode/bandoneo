# Pre-fabrication PCB Verification

Items to check before sending Gerbers to JLCPCB. Schematic review is complete; everything below is layout-specific and cannot be verified from the netlist alone.

## Component Placement

### STM32WB5MMG (U1, main board)
- [ ] **Antenna keep-out** per WB5MMG datasheet Fig. 4: no copper within 2.5 mm top/bottom of the module antenna edge, 7.6 mm to the right of the antenna tip, and 1.3 mm below the antenna on the reverse side. No GND pour, no trace, no silk inside this zone.
- [ ] Module placed at a board **edge** with the antenna pointing outward; no metal (connectors, battery, mounting hardware) within 10 mm of the antenna in the horizontal plane.
- [ ] **ANT_IN / RF_OUT pads** tied to the module GND plane (short, direct). `ANT_NC` routed to an unconnected pad (do not tie to GND).
- [ ] Module decoupling: the caps integrated in the module are sufficient; no external bulk required per the module datasheet.

### BQ25895 (charger)
- [ ] **Star-point grounding** for PGND and AGND, joined under the IC pad on the charger side; do not cross the star with high-current loops.
- [ ] Short, wide **VBUS → 1 µF ceramic → PGND** loop (input decoupling) with the cap placed within 2-3 mm of the VBUS pin.
- [ ] **SW node** kept small and away from sensitive analog (TS divider, VREGN). BTST trace short and adjacent to SW.
- [ ] **Inductor** placed close to SW; return current path back to PGND under the inductor kept tight.
- [ ] **Sense resistor** (if external) in a Kelvin-connection.
- [ ] TS divider resistors (R11/R12/R13/R14/R15) placed near the TS pin with short traces.

### TPS2553 per-wing switches (U62 left, U64 right)
- [ ] Input cap on IN within a few mm of the pin.
- [ ] ILIM resistor (R38/R39 = 82 kΩ) short trace to ILIM pin and to GND.
- [ ] /FAULT pull-up short; trace routed to main MCU GPIO with no coupling to switching nodes.
- [ ] OUT trace sized for 1 A continuous (see trace sizing below).

### Wing SPI isolation buffers (U61 left, U63 right, SN74LVC125A-Q1)
- [ ] Placed **adjacent to the wing connector** on the motherboard; wing-side (Y) traces short, main-side (A) traces may be longer.
- [ ] **C2 (100 nF) / C44 (100 nF)** placed within 2 mm of each buffer VCC (pin 14), short return to GND.
- [ ] VCC pad tied to **gated rail** (`L_VCC` / `R_VCC`) — verify layer change doesn't accidentally short to main 3.3 V.
- [ ] All four OE# pins tied to GND via short trace.

### Wing-port ESD (SRV05-4: TV4/TV5 left, TV2/TV3 right)
- [ ] Placed **at the wing connector**, between the connector pins and any other component.
- [ ] **Pin 5** connected to the **gated wing rail** (`L_VCC` / `R_VCC`), not to main 3.3 V — verify on both wings.
- [ ] GND pin to a solid GND pour with a short, wide path.

### USB input ESD (SMBJ13A on VBUS)
- [ ] Placed as close as physically possible to the USB-C connector VBUS pin.
- [ ] Anode trace to GND **short and wide** (a TVS pinned by a long thin trace doesn't clamp fast enough).

### USB D+/D- ESD (SRV05)
- [ ] Placed between USB-C connector and any series element on D+/D-.
- [ ] **Pin 5 floating** (not connected to any net or plane) — verify on layout, not just schematic.

### Wing MCU decoupling (STM32F103C8T6, each wing)
- [ ] 4 × 100 nF: each within **a few mm** of its respective VDD pin (VBAT pin 1, VDD_1 pin 24, VDD_2 pin 36, VDD_3 pin 48). Do not share one cap across pins.
- [ ] 10 µF bulk on VDD_3 close to the pin, with short return.
- [ ] VDDA pair (1 µF + 10 nF) **adjacent to pins 8/9** with the ferrite bead between VDD and VDDA placed immediately upstream of VDDA.
- [ ] VSSA tied to GND through a single short path (not a via-only connection).

### Sensitive GPIO filter caps
- [ ] 3.3 pF 0201 caps on GPIOs used for analog/sensitive inputs, placed at the pin (not near the sensor). Confirm schematic notes match placement.

### CS1237 load-cell ADC
- [ ] Placed next to the load-cell connector.
- [ ] AINP/AINN traces routed as a **differential pair**, equal length, kept away from switching nodes (SW, BLE, SPI).
- [ ] REFOUT/REFIN tie short; bypass cap on REFOUT within 2 mm.

### Hall sensors (SC4015)
- [ ] Each sensor centered on its key footprint per Gateron Magnetic Jade Pro Mini geometry, **marked face toward the switch**.
- [ ] VDD decoupling (if per-sensor) placed at each sensor or per bank.
- [ ] Mux (74HC4052) placed near each bank to minimize analog routing distance.

## Trace Sizing

- [ ] **VBUS / USB power** path: ≥ 15 mil (0.38 mm) on 1 oz copper for 2 A headroom, wider through vias.
- [ ] **VSYS / battery / charger power path**: ≥ 25 mil (0.64 mm) for 3 A peak (BQ25895 can push this during high-current charge).
- [ ] **L_VCC / R_VCC** (wing rails): ≥ 20 mil (0.51 mm) for 1 A per wing, including via count sufficient for the same current.
- [ ] **3.3 V rail** (LDO output to main): ≥ 15 mil (0.38 mm).
- [ ] **GND pour**: solid on at least one layer, continuous under the WB5MMG module, BQ25895, and TPS2553 switches.
- [ ] **SPI signals** (SCK, MOSI, MISO, NSS): standard signal width (5-8 mil), length-match not required at 1-10 MHz but keep buffer→connector stubs minimal.
- [ ] **Analog traces** (Hall outputs, load-cell AINP/AINN, expression pedal): guarded from SPI and switching nodes by GND or distance.
- [ ] **Crystal / oscillator traces** (if any on the module side): shortest possible, guard-ringed.

## Clearance

- [ ] **USB VBUS → GND** at the connector: ≥ 0.5 mm (covers 12 V fast-charge negotiation plus TVS clamp transients).
- [ ] **Battery +/-** to other nets: ≥ 0.5 mm.
- [ ] **Wing connector** pin-to-pin: per JLCPCB default for the selected connector footprint (verify datasheet minimum is respected).
- [ ] **Antenna keep-out** (WB5MMG) — re-check after every layout change; this is the easiest thing to break.
- [ ] **High-current loops** (charger SW, input cap, inductor) kept out of the antenna keep-out and away from analog front-ends.
- [ ] **Silkscreen** does not sit on any pad or via.
- [ ] **Mounting holes** have appropriate keepout from traces; plated or non-plated per mechanical design.

## DRC / Pre-fab Sanity

- [ ] KiCad DRC passes with JLCPCB standard rules (6/6 mil trace/space, 0.3 mm vias).
- [ ] All nets from schematic present on PCB (no dangling pins).
- [ ] BOM + CPL files generated and checked against JLCPCB part numbers.
- [ ] Fabrication notes include stackup, copper weight (1 oz), finish, and any impedance control requirements.
- [ ] Panelization (if used) leaves adequate rail for JLCPCB pick-and-place.

---

# Post-manufacturing Board Verification

To run on the first assembled board before powering the full system.

## Bare-board Checks (before IC power-up)

- [ ] **Visual** under magnification: solder bridges, tombstones, missing parts vs BOM.
- [ ] **Continuity** on GND and the main power rails (VBUS, VSYS, 3.3 V, L_VCC, R_VCC) — no shorts to GND, no shorts rail-to-rail, with battery disconnected and USB disconnected.
- [ ] **Short test** on each decoupling cap location (should read open after the cap discharges).

## First Power-up (USB only, no battery)

- [ ] Apply 5 V via USB through a current-limited supply (set to 500 mA).
- [ ] Measure **VBUS**, **VSYS**, **3.3 V** with DMM — all within ±5 %.
- [ ] Current draw at idle (wings disabled): expect ~15-30 mA.
- [ ] **BQ25895 I²C** responds (read default register values). Confirm OTG / charge status matches input.
- [ ] **BOOT0/FN3** DFU path: press button, verify board enumerates as STM32 DFU on USB.

## Wing Power Gating and In-rush

- [ ] Scope on `L_VCC` (then `R_VCC`): assert `L_EN`, observe **TPS2553 soft-start ramp** — should be a smooth monotonic rise to 3.3 V over ~1-2 ms, no overshoot, no /FAULT trip.
- [ ] Current probe on wing input: peak in-rush < 150 mA typ (spec corner < 260 mA). Matches session estimate.
- [ ] **/FAULT** line stays high throughout normal power-on.
- [ ] Repeat with wing connected; verify the sensor array doesn't push current beyond the 700 mA TPS2553 limit.

## Back-feed Verification (CRITICAL — validates the whole isolation strategy)

- [ ] With wing powered, main actively driving SPI: measure **`L_VCC` current** (nominal operation baseline).
- [ ] **Deassert `L_EN` while main keeps driving SPI**. Scope and DMM on `L_VCC`:
  - Rail must collapse to < 100 mV within a few ms.
  - No current flowing into `L_VCC` from any source (measure with ammeter in series).
  - Wing MCU must remain fully unpowered (confirm VDD on wing = 0 V).
- [ ] Repeat for right wing.
- [ ] Scope the main→wing SPI lines during this transition: they should still be driven by the main MCU, but the buffer outputs must be high-Z on the wing side.

## MCU and BLE Bring-up

- [ ] STM32WB5MMG boots; firmware banner on USB-MIDI or debug UART.
- [ ] **BLE advertising** visible on a scanner; pair, confirm BLE-MIDI channel works.
- [ ] BLE range/sensitivity smoke test at 1 m, 5 m, 10 m — no severe dropouts near the antenna region (verifies antenna keep-out was respected).
- [ ] USB-MIDI enumeration works on Linux/macOS/Windows.

## Wing Communication

- [ ] With wing powered, main initiates SPI transfer — wing responds on MISO.
- [ ] READY line: verify wing drives high when ready, main reads via internal pulldown when wing is off.
- [ ] NRST pulse from main resets the wing and wing re-enumerates (READY low → high handshake).
- [ ] SPI throughput: confirm keying loop can complete full 33 (L) / 38 (R) sensor scan + loadcell read within 1 ms.

## Sensor Path

- [ ] Per-key Hall reading: press each key, confirm ADC reads ~2.05 V rest → ~0.84 V fully pressed (±10 %), no dead keys, no swapped keys.
- [ ] ADC sample time vs 74HC4052 Ron (~100 Ω at 3.3 V) + SC4015 output settling: check no blur/aliasing between adjacent mux channels (scope CH_OUT during a scan).
- [ ] Load-cell: zero offset with no force; push/pull produces symmetric reading around zero. Calibration repeatable across power cycles.

## Pedals

- [ ] **Sustain jack**: short Tip-Sleeve, verify MIDI CC 64 transitions on/off.
- [ ] **Expression jack**: plug in an EX-P or EV-5, sweep full travel — MCU reads ~0.66 V to ~2.5 V range. Confirm with both "M-Audio standard" and "Other" wiring.
- [ ] Sustain plugged into expression jack (Tip-Ring short): reads as full-deflection, still works as a sustain pedal.
- [ ] Presence-sense contact: unplugged = reported absent, plugged = reported present.

## Charging and Battery

- [ ] Connect battery (with NTC). Confirm BQ25895 reports battery present, temperature within range.
- [ ] Charge cycle: monitor charge current ramps to the configured 2.2 A / 1C target, tapers as cell fills.
- [ ] **NTC behavior**: cool battery with spray (~5 °C) — charger should slow/pause charge per BQ25895 TS thresholds. Warm gently — charger resumes.
- [ ] USB disconnect under load: system transitions to battery without brown-out on 3.3 V (scope to confirm < 50 mV dip).
- [ ] **Battery runtime** at nominal load (wings active, BLE streaming): ≥ 8 h target. Deep-discharge cutoff works.

## Charge-mode Power Savings

- [ ] USB plugged, firmware puts wing MCUs to STOP and gates both wing rails.
- [ ] Measure main current draw — target ~15 mA (MCU idle + charger Iq + charge LED).
- [ ] Confirm re-enabling wings (MCU commands `L_EN` / `R_EN` high) brings them back cleanly with no sensor calibration loss.

## Protection Smoke Tests (last, as these may stress the DUT)

- [ ] **Wing VCC short to GND** (intentional, through a fuse in series as a precaution): TPS2553 current-limits, /FAULT trips, main MCU detects and reports. Release; wing recovers.
- [ ] **USB VBUS overvoltage**: inject a transient ≥ 15 V via a controlled source — SMBJ13A clamps, no damage downstream. (Optional; destructive-risk test, do on a sacrificial board.)
- [ ] **ESD gun** on USB D+/D-, wing-port pins, pedal jacks: ±8 kV contact, no functional impact.

## Documentation / Sign-off

- [ ] All measurements logged in a bring-up report with scope screenshots.
- [ ] Any deviations from this checklist noted with justification.
- [ ] Update [hardware.md](hardware.md) with actual measured values (in-rush, idle current, battery runtime) once confirmed.
