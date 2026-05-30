# Datasheet Integration Summaries

Purpose: consolidate the integration-critical facts (supply range, pinout, required externals, protection limits, layout/application notes) for every part in [export/](export/). Each summary is scoped to what a design reviewer needs — not a reproduction of the datasheet.

Cross-reference: BOM files live in [export/](export/); the PDFs are symlinked in [datasheets/](datasheets/).

---

## Processors

### STM32G474CBT6 — shared MCU (MainBoard, WingLeft, WingRight)

- **Package on BOM**: LQFP48 (STM32G474CBT6) — same part on all three boards.
- **Core**: Cortex-M4F (FPU + DSP + MPU) @ 170 MHz, 128 KB flash, 128 KB SRAM.
- **Supply**: VDD 1.71–3.6 V; VDDA/VSSA dedicated; VBAT 1.55–3.6 V for RTC + backup registers (not used here — VBAT tied to VDD).
- **Peripherals on CB (48-pin)**: 3× I²C, 3× SPI, 5× UART/USART (including LPUART), 1× USB 2.0 FS device (crystal-less via HSI48 + CRS), 1× FDCAN, 2× 12-bit ADCs (up to 4.27 Msps with hardware oversampling, 0–3.3 V), 4-ch DMA + DMAMUX, 1× advanced PWM timer + multiple GP timers, 2 watchdogs.
- **GPIOs**: 38 I/O on LQFP48, all 5 V-tolerant. EXTI on every line.
- **Clocks**: HSI16 internal, HSI48 internal for USB SOF lock via CRS, optional HSE 4–48 MHz (not populated), optional LSE 32.768 kHz (not populated). **No external crystal needed** — USB enumerates against HSI48 trimmed by CRS, system clock derived from HSI16 × PLL up to 170 MHz.
- **ADC**: 12-bit, 4.27 Msps. With hardware oversampling settable up to 16-bit effective resolution. VDDA ≥ 2.4 V when ADC is used.
- **Boot modes**: Flash / System memory (USART/USB DFU bootloader) / SRAM, selected via BOOT0 pin (no BOOT1 on G4).
- **Typical externals**: one 100 nF per VDD pin + 4.7 µF bulk; 1 µF + 10 nF on VDDA referenced to VSSA, with a ferrite bead between VDD and VDDA; 100 nF on NRST.
- **Debug**: SWD + SWO (2 + 1 wires); built-in USB DFU available for factory programming if BOOT0 is asserted at reset.
- **Why G4 instead of F1 or G431.** Same SKU runs on all three boards (no toolchain split), USB without a crystal, plenty of pin/peripheral budget for two independent SPI buses on the motherboard. G4 is *extended* on JLCPCB (small per-unit surcharge over basic-part G431), accepted in exchange for the single-SKU simplification.
- **Boot / DFU**: SW4 (BOOT0 / FN0 button) ties PB8/BOOT0 to 3.3 V when pressed; external 10 kΩ pull-down (R1) holds BOOT0 low at idle. Hold SW4 while pressing SW1 (global NRST) to enter DFU. SW1 is the global reset button (pulls all 3 MCU NRST lines via BAT54C). See [hardware.md](hardware.md) §"Function and Boot Buttons".

---

## Power

### TLV755P — 500 mA LDO (3.3 V rail)
[datasheets/tlv755p.pdf](datasheets/tlv755p.pdf)

- **Package on BOM**: SOT-23-5 (DBV) variant — pin 1 IN, 2 GND, 3 EN, 4 NC, 5 OUT.
- **Supply**: VIN 1.45–5.5 V; VOUT fixed (0.6–5 V options); 500 mA; IQ 25 µA typ.
- **Externals**: 1 µF on IN, 1 µF on OUT (nominal; effective must be >0.47 µF after derating). Both ceramic, placed close.
- **Enable**: EN active-high (VHI ≥ 1 V). If always-on, tie EN to IN. Internal 120 Ω pulldown on output during shutdown for clean re-start.
- **UVLO**: VIN rising threshold ~1.3 V.
- **Protection**: foldback current limit (~720 mA), thermal shutdown at 165 °C, active output discharge.
- **Dropout at 3.3 V / 500 mA**: 150 mV typ / 215 mV max @ 85 °C → **VIN ≥ 3.55 V needed to maintain 3.3 V at full load**. Comfortable at VBUS 4.40 V worst-case minimum.
- **PSRR**: 46 dB @ 100 kHz.
- **Input rail in this design**: **VBUS** through the USB TVS/polyfuse front-end. No battery, no charger, no VSYS intermediate rail.

---

## Protection / ESD

### SMF5.0A — unidirectional TVS (VBUS clamp)
[datasheets/SMF5.0A](datasheets/)

- **Package**: SOD-123FL. **200 W** peak pulse power (10/1000 µs), Ipp = 21.7 A.
- **VRWM = 5 V**, VBR(min) = 7 V @ IT = 1 mA, **VC = 9.2 V @ IPP = 21.7 A**.
- IR ≤ 400 µA @ VRWM; operating range −55 … +150 °C.
- **Use in this design**: VBUS clamp (USB 5 V rail). VC = 9.2 V is well below the TLV755P abs-max input (5.5 V continuous — the TVS clamps transients, not steady-state). Unidirectional, cathode to VBUS, anode to GND.
- **Layout**: short, wide trace to GND; place immediately after the USB connector.

### H5VND5BA — bidirectional low-capacitance ESD diode
[datasheets/H5VND5BA.pdf](datasheets/H5VND5BA.pdf)

- **Package**: SOD-523 (0402-sized footprint).
- **Bidirectional**, VRWM = ±5 V, VBR = 6–8 V, VC = 8 V @ 1 A / 15 V @ 8 A.
- **CJ ≤ 20 pF typ** — usable on slow digital I/O, I²C, audio-range analog. Not suited to USB 2.0 FS or faster (capacitance too high for signal-integrity on high-speed data lines).
- **Use in this design**: one per function-button line (D1–D4, four switches SW1–SW4). Placing the diode on the GPIO side of the switch simplifies the PCB trace routing — no dedicated ESD ground spur to each button is needed, and each GPIO pin is protected against hand-discharge events. The 20 pF capacitance is inconsequential on these low-frequency digital inputs.
- **Programming port protection**: the STDC14 pogo pads are protected by a dedicated **SRV05-4** array (not H5VND5BA). The SRV05-4 CJ ≈ 3.5 pF per line is well under the 20 pF budget at SWD/SWO speeds (≤ 10 MHz). PCB trace capacitance on a short 2-layer stub (< 20 mm, 0.2 mm wide over 1 mm GND plane) adds ≈ 1–2 pF — total per-line capacitance comfortably below 10 pF. Ground copper adjacent to the pogo traces contributes negligibly at these frequencies.
- **STDC14 pin 11 (GNDDetect) → MCU GPIO PA15**: per UM2448 Rev 9 Table 6 note 6 the STLINK-V3SET firmware ties this pin to GND so the target can detect the tool. PA15 is configured as input with internal pull-up: LOW when STLINK is mated, HIGH otherwise. See `hardware.md` §"Programming Port".
- **IEC 61000-4-2**: rated ±8 kV contact, ±15 kV air.

### SRV05-4 — 4-channel ESD diode array
[datasheets/SRV05-4.pdf](datasheets/SRV05-4.pdf)

- **Package**: SOT-23-6.
- 4 I/O channels + VCC + GND, using steering diodes to VCC/GND rails and an internal TVS.
- **VWM = 5 V**, VC = 12 V @ 1 A, **CJ ≈ 3.5 pF typ per line** — suitable for USB 2.0 D+/D−.
- **Use in this design**: (1) USB D+/D− on the MainBoard (TV8) — pin 5 (V+) tied to **VBUS (5 V)**; no PD negotiation so VBUS is always 5 V, upper steering diodes are active, ESD charge steers to VBUS (clamped by SMF5.0A) or GND; (2) two pedal TRS jacks (Tip + Ring × 2 = 4 lines) — pin 5 tied to 3.3 V; (3) programming port STDC14 pogo pads (SWD, SWO, NRST, UART lines).
- **Layout**: place immediately at the connector, route protected lines through the device (not stubbed).

### BAT54 — Schottky diode (steering / protection)
[datasheets/BAT54.pdf](datasheets/BAT54.pdf)

- **Package**: SOT-23. Variants: BAT54 (series), BAT54A (common-anode), BAT54C (common-cathode), BAT54S (series-pair).
- **Ratings**: VR 30 V, IF 200 mA avg (600 mA surge), **VF = 320 mV @ 1 mA**, **trr = 5 ns**, **CT = 10 pF**.
- **Use**: low-forward-drop signal steering (e.g., open-drain reset ORing on the wing NRST lines, boot-pin isolation).
- **Review check**: verify which BAT54 variant the BOM calls out — the three topologies have different footprints.

---

## Sensing / ADC / Mux

### SC4015SO-N — linear Hall-effect sensor (key position, per-key)
[datasheets/SC4015_EN.pdf](datasheets/SC4015_EN.pdf)

- **Package**: SOT23-3L (pin 1 VDD, 2 GND, 3 OUT).
- **Supply**: 2.2–5.5 V (abs-max 25 V). **ICC typ 2.5 mA / max 6 mA @ 5 V, 25 °C.**
- **Response**: T<sub>RESP</sub> = 1 µs, T<sub>PO</sub> ≤ 0.8 µs (fast enough to settle inside one ADC acquisition slot).
- **At 3.3 V**: **sensitivity 2.0 mV/Gs typ** (min 1.8 / max 2.2), V<sub>OUT(Q)</sub> ≈ 2.2 V (**unipolar** — quiescent sits near the top rail). Linear output range 0.8 V → 2.2 V; the useful single-sided swing on the active polarity is **~1.4 V**.
- **Active polarity** (`-N` variant): output drops below V<sub>OUT(Q)</sub> when an **N-pole** approaches the marked face. With the sensor mounted on the opposite side of the PCB, marked face toward the switch, this matches Gateron magnetic switches (N-pole faces PCB) — see the magnet/sensor pairing analysis in [hardware.md](hardware.md).
- **Externals**: 0.1 µF from VDD to GND at the sensor; **optional 1 nF on OUT** for noise filtering ahead of the mux/ADC.
- **Output impedance**: low; drives the 74HC4052 input directly. With 80 Ω Ron mux and the ADC sampling cap, settling is dominated by the ADC sample time, not the sensor.
- **Quantity**: 71 total (33 WingLeft + 38 WingRight).

### Gateron Magnetic Jade Pro mini (KS-33D) — keyboard switch / magnet
[datasheets/GATERONLowProfileMagneticJadeProSwitchSPEC.pdf](datasheets/GATERONLowProfileMagneticJadeProSwitchSPEC.pdf)

- **Mechanical**: low-profile keyboard switch, total travel **3.5 ± 0.2 mm**, initial force 40 ± 10 gf, total-travel force < 110 gf, life 100 M cycles.
- **Magnet orientation**: N-pole faces down toward the PCB (drawing page 7).
- **Magnetic flux at the Hall sensor pad** (Gateron-spec, sensor on the opposite side of the PCB):
  - Free position (0 mm): **75 ± 10 Gs**
  - Full travel @ 500 gf: **680 ± 70 Gs**
- **PCB stack assumption**: numbers above presume Gateron's reference PCB (~1.6 mm). At our 1.5 mm stack the sensor sits 0.1 mm closer → field marginally stronger, inside the spec ±10 % tolerance.
- **Pairing with SC4015-N at 3.3 V** (typical): rest V<sub>OUT</sub> ≈ 2.05 V, full-press V<sub>OUT</sub> ≈ 0.84 V → ~1.21 V active swing across 3.5 mm. Worst-case (high field × high sensitivity) clamps at the 0.8 V floor in the last ~0.1–0.2 mm of travel — harmless for velocity capture.

### CS1237 — 24-bit Σ-Δ ADC (evaluated, not placed)
[datasheets/DS_CS1237_V1.1.pdf](datasheets/DS_CS1237_V1.1.pdf)

- **Status**: evaluated for a loadcell-based push/pull readout, **not on the current BOM**. The loadcell approach was rejected on feel (near-zero compliance removes proprioceptive feedback the player needs); the design now uses a spring-blade + 2× SC4015 hall sensors on the motherboard. See [hardware.md](hardware.md) §"Push/Pull Sensor".
- **Package**: SOP-8. Pinout: 1 REFIN, 2 GND, 3 AINN, 4 AINP, 5 SCLK, 6 DRDY/DOUT, 7 VDD, 8 REFOUT.
- **Supply**: VDD 3.0–3.6 V (logic) or 4.5–5.5 V (ratiometric). Internal 5.2 MHz oscillator, no crystal.
- **Interface**: 2-wire (SCLK, DRDY/DOUT) — DRDY pulses when new sample is ready. Not standard SPI; would be bit-banged from the main MCU if used.
- **PGA**: ×1 / ×2 / ×64 / ×128. **Data rates: 10 / 40 / 640 / 1280 Hz**.
- **Reference**: internal REFOUT tied to REFIN (default), ratiometric with the bridge excitation.
- **Externals**: 100 nF on VDD, 100 nF on REFIN.
- **Kept as reference**: if a future variant ever wants a force-curve push/pull mode (e.g., a non-bandoneonist preset), CS1237 remains a credible single-IC choice for a 4-wire Wheatstone bridge.

### 74HC4052PW (Nexperia, ,118) — analog mux (key scanning)
[datasheets/74HC4052.pdf](datasheets/74HC4052.pdf)

- **Package**: TSSOP-16 (PW suffix), Nexperia ,118 reel.
- **Function**: dual 4-channel analog multiplexer/demux. Two select inputs (S0, S1), one active-low enable (Ē).
- **Supply**: VCC 2–10 V (with VEE 0 V for unipolar), or VCC/VEE split for bipolar signal range.
- **Ron at 3.3 V**: ~80 Ω typ (100 Ω max over temp). Not a precision mux, but adequate for driving an MCU ADC sample-and-hold with sufficient settling time (configured via the STM32 ADC sampling-time register).
- **Leakage**: a few nA at room temp; ~1 µA at 85 °C — negligible against 3.3 V full-scale Hall signals.
- **Topology in this design**: single-stage flat mux array — **WingLeft: 4 packages × 8 channels = 32 sensors**; **WingRight: 5 packages × 8 channels = 40 channels** (38 sensors, 2 spare). No cascading, so only one Ron appears in series with the ADC sample cap.
- **Externals**: 100 nF bypass on VCC per package.
- **Enable Ē**: when disabled, all channels open — available for bank-switching power if duty-cycling is later added.

---

## Summary of Protection Coverage

| Threat | Defense | Notes |
|---|---|---|
| USB VBUS overvoltage / surge | SMF5.0A (D5) | 200 W, VC ≈ 9.2 V — well below TLV755P input abs-max |
| USB D+/D− ESD | SRV05-4 (TV8, pin 5 → VBUS) | 3.5 pF, USB 2.0 FS OK; full bidirectional clamp |
| Pedal TRS jack ESD (Tip + Ring × 2 jacks) | SRV05-4 (pedal instance) | Slow analog lines; 4 channels = 2 jacks |
| Wing-bus ESD at the IDC connector | SRV05-4 × 2 per wing (SPI×4 + NRST + READY + BOOT0 = 7 signals, 8 channels total) | Pin 5 tied to 3.3 V (same rail as the wings) |
| Function buttons SW1–SW4 (GPIO lines) | H5VND5BA (D1–D4) | Bidirectional, 20 pF — fine for slow digital |
| Programming port (STDC14 pogo pads) | SRV05-4 | 3.5 pF/line — OK for SWD/SWO ≤ 10 MHz |
| 3.3 V rail short | TLV755P foldback + thermal | Regulated down to ~1 V out |
| VBUS sourcing short on the host side (optional) | Polyfuse / eFuse on VBUS (suggested) | ~500 mA hold / 1 A trip; not currently placed |

## Open Items for Design Review

1. **ADC sample-time configuration**: confirm the STM32G474 ADC sampling time is set long enough to settle through one 74HC4052 Ron (~80 Ω) plus SC4015 output impedance into the ADC sample-and-hold cap — spec sheet and scope trace welcome.
2. **BAT54 variant**: confirm which of BAT54 / A / C / S is placed — the three topologies share a SOT-23 package but differ in footprint connectivity. (BOM says BAT54C — common cathode — verify.)
3. **Pedal-jack SRV05-4 wiring**: confirm pin-5 handling on the pedal-jack SRV05-4 package (tied to 3.3 V vs. floating as on the USB instance), since pedal Tip/Ring lines carry slow analog — steering to rail is acceptable here, but the choice should be documented.
