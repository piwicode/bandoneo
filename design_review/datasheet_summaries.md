# Datasheet Integration Summaries

Purpose: consolidate the integration-critical facts (supply range, pinout, required externals, protection limits, layout/application notes) for every part in [export/](export/). Each summary is scoped to what a design reviewer needs — not a reproduction of the datasheet.

Cross-reference: BOM files live in [export/](export/); the PDFs are symlinked in [datasheets/](datasheets/).

---

## Processors / Wireless

### STM32WB5MMG — BLE 5.0 + 802.15.4 SiP module (MainBoard)
[datasheets/stm32wb5mmgh6tr.pdf](datasheets/stm32wb5mmgh6tr.pdf)

- **Package**: LGA-86, 7.3 × 11 × 1.342 mm, 0.435 mm pitch.
- **Core**: STM32WB55VGY die (Cortex-M4 @ 64 MHz + Cortex-M0+ for radio), 1 MB flash, 256 KB SRAM.
- **Integrated**: 32 MHz HSE crystal, 32.768 kHz LSE crystal, SMPS passives, IPD matching, chip antenna — **no external RF/clock components required**.
- **Supply**: VDD 1.8–3.6 V; separate VDDA (analog) and VDDUSB (USB). VBAT for RTC backup.
- **Peripherals exposed**: 2× DMA (7ch), USART + LPUART, 2× SPI (up to 32 Mbit/s), 2× I²C, SAI, USB 2.0 FS (crystal-less, BCD/LPM), TSC (18 sensors), LCD 8×40, timers (1× adv 4ch, 2× GP 2ch, 1× 32-bit 4ch, 2× LP), 12-bit ADC.
- **Critical integration rules**:
  - **Antenna clearance**: keep 2.5 mm from board edge at antenna side, 7.6 mm clearance width, 1.3 mm from long edge (datasheet Fig. 4). No copper/ground right of the module.
  - **Ground plane**: solid, vias at ≤2 mm spacing; ANT_NC pad soldered to unconnected pad.
  - **ANT_IN and RF_OUT pads must tie to GND** (internal antenna is used).
  - **Sensitive GPIOs** PB10, PB11, PC5 — recommend a 3.3 pF capacitor in 0201 at pin and ground-bordered tracks.
  - **Reset**: internal pull-up present; only a 100 nF cap to GND on NRST needed.
  - **SMPS**: always-on at 4 MHz, passives already inside the module — nothing external to add.
  - **LSE**: must configure `RCC_BDCR_LSEDRV[1:0] = 11` (high drive); HSETUNE preloaded, do not change.
  - **LCD/USB/SAI signals** overlap with GPIO — must be checked at pin-mux time.
- **Enclosure effects**: metal case must stay ≥46 mm (a-dim) from the antenna for negligible impact; any contact of plastic case with antenna detunes it.
- **Two-layer PCB supported** using only the outer-ring pads; four-layer required if inner pads are used.

### STM32WB55RG — underlying MCU for the module (reference only)
[datasheets/stm32wb55rg-datasheet.pdf](datasheets/stm32wb55rg-datasheet.pdf)

Used as reference for WB5MMG internals, **not placed on the BOM**. Key facts that transfer:
- Cortex-M4 (64 MHz, FPU, DSP, MPU) + Cortex-M0+ for radio.
- VDD 1.71–3.6 V, integrated SMPS with bypass fallback below VBOR.
- 12-bit ADC 4.26 Msps (up to 16-bit with hardware oversampling).
- BLE 5.4 / 802.15.4 / Thread 1.3 / Zigbee 3.0 stacks; 2 Mbps support; +6 dBm TX.
- 72 fast I/Os, 70 of them 5 V-tolerant (on the full-size package — count differs on WB5MMG which exposes ~68).
- **Review use**: consult for register-level behavior, supply current tables, ADC accuracy — the WB5MMG datasheet does not duplicate these.

### STM32F103C8 — wing MCU (WingLeft / WingRight)
[datasheets/stm32f103x8.pdf](datasheets/stm32f103x8.pdf)

- **Package on BOM**: LQFP48 (STM32F103C8T6).
- **Core**: Cortex-M3 @ 72 MHz, 64 KB flash, 20 KB SRAM.
- **Supply**: VDD 2.0–3.6 V; VDDA/VSSA dedicated; VBAT 1.8–3.6 V for RTC + backup registers.
- **Peripherals on C8 (48-pin)**: 2× I²C, 2× SPI, 3× USART, 1× USB FS, 1× CAN (USB and CAN share pins — mutually exclusive), 2× 12-bit ADC (10 channels, 0–3.6 V), 7-ch DMA, 3× GP 16-bit timers + 1× advanced PWM timer, 2 watchdogs.
- **GPIOs**: 37 I/O on LQFP48, almost all 5 V-tolerant. 16 EXTI lines.
- **Clocks**: 4–16 MHz HSE (8 MHz typical for USB), 32.768 kHz LSE, 8 MHz HSI, 40 kHz LSI. **USB requires HSE + PLL → 48 MHz USBCLK**.
- **ADC**: 1 µs conversion requires APB2 at 14/28/56 MHz. Max conversion clock 14 MHz. VDDA ≥ 2.4 V when ADC is used.
- **Boot modes**: Flash / System memory (USART1 bootloader) / SRAM, selected via BOOT0/BOOT1.
- **Typical externals**: one 100 nF per VDD pair + 4.7 µF bulk on VDDA; 100 nF on NRST; 8 MHz crystal with 2× 18–22 pF; 32.768 kHz crystal if RTC needed.
- **Debug**: SWD (2-wire) or JTAG (5-wire).
- **Review note**: current design uses BOOT0 re-purposed as FN3 (see [hardware.md](hardware.md)) — confirm BOOT1 is latched at reset and that FN3 routing does not prevent entering system bootloader for DFU recovery.

---

## Power

### BQ25895 — single-cell Li-Ion charger + boost (MainBoard)
[datasheets/bq25895.pdf](datasheets/bq25895.pdf)

- **Package**: WQFN-24, 4×4 mm with exposed PowerPAD (must be soldered, star-connected to PGND).
- **Function**: 5-A switch-mode buck charger from 3.9–14 V input; boost (OTG) 4.55–5.55 V @ 3.1 A; integrated BATFET and power-path (NVDC).
- **Pinout (must-route nets)**:
  - VBUS (1) ← USB/adapter. **1 µF ceramic from VBUS to PGND, right at the pin.**
  - SW (19, 20) ← power inductor. **2.2 µH on current BOM (APH0630T2R2M).**
  - BTST (21) ← 0.047 µF bootstrap cap to SW.
  - REGN (22) ← 4.7 µF (10 V rating) ceramic to analog GND. REGN also biases TS.
  - PMID (23) ← boost output; ≥40 µF for 2.4 A OTG, ≥60 µF for 3.1 A OTG.
  - SYS (15, 16) ← 20 µF close.
  - BAT (13, 14) ← 10 µF close.
  - PGND (17, 18) connects single-point to AGND near IC.
  - TS (11) ← NTC divider from REGN (103AT-2 recommended). **Required — charge suspends if TS out of range.**
  - CE (9) active-low charge enable — must be driven high or low, not floating.
  - QON (12) has internal pull-up; pulse low to exit ship mode / reset system.
  - STAT, INT, SCL, SDA, DSEL — open-drain, need 10 kΩ pull-ups to the host logic rail (1.8–3.3 V). SDA/SCL at I²C 400 kHz max.
- **Protection limits**: absmax VBUS −2 / +22 V; VACOV rising 14–14.6 V; battery OVP at 104 % of VREG (2 % hysteresis); system OCP 9 A; thermal shutdown 160 °C.
- **Register defaults of note**: REG01 default changed from 0x06 → 0x05 in Rev C (affects boost/charge at reset).
- **Thermal**: RθJA = 31.8 °C/W (good), RθJC(bot) = 2.0 °C/W — exposed pad is the primary path. Use multiple thermal vias.
- **Layout highlights**: single-point PGND-to-AGND join at IC; tight loop VBUS→1µF→PGND; BTST/SW trace short; REGN cap right at pin.
- **Watchdog**: 40 s default; disable via WATCHDOG bits for systems not servicing I²C continuously.

### APH0630T2R2M — power inductor for BQ25895
[datasheets/APH0630T2R2M.pdf](datasheets/APH0630T2R2M.pdf)

- **Size**: 7 × 6.6 × 2.8 mm SMD.
- **L = 2.2 µH**, **Isat ≈ 10 A**, **Irms ≈ 9.5 A**, **DCR = 15 mΩ**.
- Matches BQ25895 recommended 1–2.2 µH range at 1.5 MHz and the 5 A charge / 3.1 A boost ratings with margin.
- **Review check**: routed as short, wide copper between SW (pins 19–20) and BAT; keep away from sensitive analog traces.

### TLV755P — 500 mA LDO (3.3 V rail)
[datasheets/tlv755p.pdf](datasheets/tlv755p.pdf)

- **Package on BOM**: SOT-23-5 (DBV) variant — pin 1 IN, 2 GND, 3 EN, 4 NC, 5 OUT.
- **Supply**: VIN 1.45–5.5 V; VOUT fixed (0.6–5 V options); 500 mA; IQ 25 µA typ.
- **Externals**: 1 µF on IN, 1 µF on OUT (nominal; effective must be >0.47 µF after derating). Both ceramic, placed close.
- **Enable**: EN active-high (VHI ≥ 1 V). If always-on, tie EN to IN. Internal 120 Ω pulldown on output during shutdown for clean re-start.
- **UVLO**: VIN rising threshold ~1.3 V.
- **Protection**: foldback current limit (~720 mA), thermal shutdown at 165 °C, active output discharge.
- **Dropout at 3.3 V / 500 mA**: 150 mV typ / 215 mV max @ 85 °C → **VIN ≥ 3.55 V needed to maintain 3.3 V at full load**. From a single Li-Ion (3.0–4.2 V), output may droop during deep discharge.
- **PSRR**: 46 dB @ 100 kHz.
- **Review check**: verify which rail feeds TLV755P input — if it is VSYS (BQ25895), VSYS(MIN)=3.5 V will keep it in regulation; if direct from battery, regulation fails below ~3.55 V.

### TPS2553DBV — current-limited USB power switch
[datasheets/TPS2553DBVR.pdf](datasheets/TPS2553DBVR.pdf)

- **Package**: SOT-23-6 (DBV). Pin 1 IN, 2 GND, 3 EN (active-high on TPS2553), 4 /FAULT, 5 ILIM, 6 OUT.
- **Supply**: 2.5–6.5 V. Up to 1.5 A load. 85 mΩ rDS(on) typ.
- **Current limit**: programmed by R_ILIM, 15–232 kΩ range. IOS ≈ K_ILIM / R_ILIM; ±6 % accuracy around 1.7 A.
- **Behavior**: TPS2553 (no "-1") is **constant-current** on fault (auto-recover) — correct choice for USB ports. -1 variant **latches off**.
- **Externals**: 0.1 µF minimum on IN (10 µF typical in reference), 120 µF on OUT per USB spec (shared across ports OK), 10 kΩ pull-up on /FAULT.
- **Reverse-voltage protection**: trips when VOUT > VIN by 135 mV for 4 ms — useful if downstream can back-drive.
- **Protection**: 15 kV IEC 61000-4-2 air / 8 kV contact ESD at connector **with external capacitance** present.
- **Review check**: R_ILIM value in schematic must correspond to the configured limit (e.g., 20 kΩ → ~1.3 A). Verify DBV vs. DRV package on BOM matches footprint.

---

## Protection / ESD

### SMAJ13A — unidirectional TVS (⚠️ BOM mismatch)
[datasheets/SMAJ13A.pdf](datasheets/SMAJ13A.pdf)

- **Package**: SMA (DO-214AC). **400 W** peak pulse power (10/1000 µs).
- **VRWM = 13 V**, VBR(min) = 14.4 V, **VC = 21.5 V @ IPP = 18.6 A**.
- ⚠️ **BOM inconsistency**: the current BOM lists **SMBJ13A** (SMB package, 600 W) but the datasheet on disk is **SMAJ13A** (SMA package, 400 W). Either update the BOM or swap the datasheet. **Action required before fabrication.**
- **Use in this design**: VBUS clamp (USB 5 V rail) / adapter input protection.
- **Polarity**: unidirectional — cathode toward the protected line, anode to GND.

### H5VND5BA — bidirectional low-capacitance ESD diode
[datasheets/H5VND5BA.pdf](datasheets/H5VND5BA.pdf)

- **Package**: SOD-523 (0402-sized footprint).
- **Bidirectional**, VRWM = ±5 V, VBR = 6–8 V, VC = 8 V @ 1 A / 15 V @ 8 A.
- **CJ ≤ 20 pF typ** — usable on USB 2.0 FS, DIN-MIDI, I²C, audio. **Not suited to USB 3 / HDMI (capacitance too high)**.
- **Use**: placed on I/O lines that see user contact (5-pin DIN MIDI, keys, external connectors).
- **IEC 61000-4-2**: rated ±8 kV contact, ±15 kV air.

### SRV05-4 — 4-channel ESD diode array
[datasheets/SRV05-4.pdf](datasheets/SRV05-4.pdf)

- **Package**: SOT-23-6.
- 4 I/O channels + VCC + GND, using steering diodes to VCC/GND rails and an internal TVS.
- **VWM = 5 V**, VC = 12 V @ 1 A, **CJ ≈ 3.5 pF typ per line** — suitable for USB 2.0 D+/D−.
- **Use**: USB data lines and similar 4-lane bundles.
- **Layout**: place immediately at the connector, route protected lines through the device (not stubbed).

### BAT54 — Schottky diode (steering / protection)
[datasheets/BAT54.pdf](datasheets/BAT54.pdf)

- **Package**: SOT-23. Variants: BAT54 (series), BAT54A (common-anode), BAT54C (common-cathode), BAT54S (series-pair).
- **Ratings**: VR 30 V, IF 200 mA avg (600 mA surge), **VF = 320 mV @ 1 mA**, **trr = 5 ns**, **CT = 10 pF**.
- **Use**: low-forward-drop signal steering (e.g., power-OR'ing of USB 5 V vs. battery rails, isolation on boot pins, reset ORing).
- **Review check**: verify which BAT54 variant the BOM calls out — the three topologies have different footprints.

---

## Sensing / ADC / Mux

### SC4011SO-N — linear Hall-effect sensor (key position, per-key)
[datasheets/SC4011SO-N-TR.pdf](datasheets/SC4011SO-N-TR.pdf)

- **Package**: SOT-23 (3-pin: VCC, GND, OUT).
- **Supply**: 2.5–18 V. **At 3.3 V: sensitivity ≈ 1.6 mV/Gauss**, VOUT(Q) = VCC/2 (quiescent), rail-to-rail-ish within ~100 mV.
- **Bandwidth**: DC to ~30 kHz. Response time a few µs.
- **Externals**: 0.1 µF from VCC to GND at the sensor; **optional 1 nF on OUT** as a low-pass (noise filtering before the mux/ADC).
- **Output impedance**: low; drives the 74HC4052 input directly. With 80 Ω Ron mux and the ADC sampling cap, settling is dominated by the ADC sample time, not the sensor.
- **Integration note**: 142 sensors × 3.3 V × typ quiescent current → budget the sensor supply current draw (check SC4011 ICC vs. VCC curve). Consider whether all 142 are always-powered or gated in banks.

### CS1237 — 24-bit Σ-Δ ADC (auxiliary analog, if used)
[datasheets/DS_CS1237_V1.1.pdf](datasheets/DS_CS1237_V1.1.pdf)

- **Package**: SOP-8. Pinout: 1 REFIN, 2 GND, 3 AINN, 4 AINP, 5 SCLK, 6 DRDY/DOUT, 7 VDD, 8 REFOUT.
- **Supply**: VDD 3.0–3.6 V (for logic) or 4.5–5.5 V (ratiometric). Internal 5.2 MHz osc, no crystal.
- **Interface**: 2-wire (SCLK, DRDY/DOUT) — DRDY pulses when new sample is ready. Not standard SPI; bit-banged on most MCUs.
- **PGA**: ×1 / ×2 / ×64 / ×128. **Data rates: 10 / 40 / 640 / 1280 Hz** — note 1280 Hz is fastest; not suitable if you need per-key 1 kHz polling across many channels.
- **Reference**: internal REFOUT drives REFIN (tie together for default), or external ref for better absolute accuracy.
- **Externals**: 100 nF on VDD, 100 nF on REFIN.
- **Review context**: the primary key-sensing path is STM32 ADC + 74HC4052 mux. CS1237 — if present — is likely for a single high-precision channel (battery gauge, pressure, bellows?). Confirm what it actually digitizes in [hardware.md](hardware.md).

### 74HC4052PW (Nexperia, ,118) — analog mux (key scanning)
[datasheets/74HC4052.pdf](datasheets/74HC4052.pdf)

- **Package**: TSSOP-16 (PW suffix), Nexperia ,118 reel.
- **Function**: dual 4-channel analog multiplexer/demux. Two select inputs (S0, S1), one active-low enable (Ē).
- **Supply**: VCC 2–10 V (with VEE 0 V for unipolar), or VCC/VEE split for bipolar signal range.
- **Ron at 3.3 V**: ~80 Ω typ (100 Ω max over temp). Not a precision mux, but adequate for driving an MCU ADC sample-and-hold with sufficient settling time (which the STM32 ADC sampling time register controls).
- **Leakage**: a few nA at room temp; grows to ~1 µA at 85 °C — negligible for 3.3 V full-scale Hall signals.
- **Architecture note (for review)**: 142 keys → typical topology is ~18 muxes × 8 channels or similar tree. Verify total number of mux stages and that the ADC acquisition time is set for the worst-case series Ron (multiple muxes cascaded would stack Ron, which matters for settling against the ADC sample cap).
- **Externals**: 100 nF bypass on VCC per package.
- **Enable Ē**: when disabled, all channels open — useful for bank-switching power.

---

## Summary of Protection Coverage

| Threat | Defense | Notes |
|---|---|---|
| USB VBUS overvoltage / surge | SMAJ13A (⚠ BOM says SMBJ13A) | Verify package & part number match |
| USB D+/D− ESD | SRV05-4 | 3.5 pF, USB 2.0 FS OK |
| External user-touched I/O (MIDI DIN, keys) | H5VND5BA | Bidirectional, 20 pF max — OK for MIDI & audio |
| Charger overvoltage (VBUS > 14.6 V) | BQ25895 internal VACOV | Plus SMAJ13A as coarse clamp |
| Battery OVP/OCP | BQ25895 internal | 104 % VREG, 9 A system |
| 3.3 V rail short | TLV755P foldback + thermal | Regulated down to ~1 V out |
| USB sourcing short (OTG / 5 V port) | TPS2553 current-limit + /FAULT | Constant-current mode, not latch |

## Open Items for Design Review

1. **SMAJ13A vs. SMBJ13A**: BOM / datasheet mismatch on TVS — choose one and align.
2. **TLV755P input rail**: confirm it is VSYS from BQ25895 (stays ≥3.5 V) rather than direct VBAT.
3. **CS1237 role**: document in [hardware.md](hardware.md) what signal it digitizes and whether its 1280 Hz max rate is compatible with that role.
4. **74HC4052 architecture**: document the number of mux stages and ADC sample-time configuration; Ron-vs-sample-cap settling needs to be verified for worst-case topology.
5. **WB5MMG layout**: verify 2.5/7.6/1.3 mm antenna clearance, sensitive-GPIO capacitors on PB10/PB11/PC5, and ANT_IN/RF_OUT/ANT_NC pad handling on the MainBoard layout.
6. **STM32F103 DFU path**: with BOOT0 repurposed as FN3, document the recovery procedure for entering system-memory bootloader.
7. **BAT54 variant**: confirm which of BAT54/A/C/S is placed (footprints differ).
