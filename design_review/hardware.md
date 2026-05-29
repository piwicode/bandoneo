
## ESD and VBUS Protection

The instrument is bus-powered from USB VBUS at the standard 5 V (USB 2.0) — no PD voltage negotiation is performed, no battery, no buck/charger on the input. The connector type is a separate mechanical decision; the electrical front end below works with any USB 2.0 receptacle. This simplifies the protection considerably:

- **VBUS TVS:** an `SMF5.0A` (SOD-123FL, unidirectional, 200 W peak) clamps transient surges to ≈ 9.2 V, well below the downstream LDO's absolute-max input. Reverse standoff 5 V matches the only operating voltage VBUS will see; a misbehaving source pushing higher voltage is clamped, not propagated to the LDO.
- **No polyfuse / eFuse on VBUS:** deliberately not fitted. The USB 2.0 host port is already required by spec to limit current to 500 mA; the SMF5.0A TVS clamps transient surges; and the TLV755P has internal foldback + thermal shutdown. A polyfuse would be an JLCPCB extended part with no matching basic-part substitute. The combination of host OCP + TVS + LDO self-protection is judged sufficient for v1.
- **D+ / D− ESD:** SRV05-4 (TV8) with pin 5 (V+) tied to **VBUS (5 V)**. Since no PD negotiation is performed, VBUS is always standard USB 2.0 5 V — the only voltage this pin will ever see. Tying V+ to VBUS is the correct usage: it activates both the upper and lower steering diodes, giving full bidirectional clamping on D+/D−. ESD charge steered upward disperses into the host's source impedance; the SMF5.0A is there to clamp any transient that reaches VBUS. Floating pin 5 would disable the upper diodes and halve clamping effectiveness.

## Power Consumption

Estimated from the BOMs in [export/](export/) at the 3.3 V rail, with typical datasheet values at room temperature. Hall sensor count: **33 (WingLeft) + 38 (WingRight) = 71** key-position sensors, plus **2 on the motherboard** for push/pull sensing — **73 total, all gated**.

**Hall sensor power gating.** Each board carries one AO3401A P-channel MOSFET (Q1) whose drain feeds a switched `VDDH` rail. The MCU drives the gate, enabling `VDDH` only during each scan window:

- **Wings**: 33 / 38 SC4015 sensors per board, scanned via 4 / 5 × 74HC4052 muxes at 1 kHz. Window ≈ ~40 channels × ~6 µs settle+convert = **~240 µs ON / 1 000 µs period ≈ 25 % duty cycle**.
- **Motherboard**: 2 SC4015 sensors for push/pull, gated by the same Q1 topology but sampled at the push/pull rate (a few hundred Hz at most — mechanical bandwidth). Duty cycle is dominated by the wings' loop, so the motherboard's 2 sensors run at **≤ 25 %** in the worst case where main schedules them inside the same window.

The SC4015 datasheet T<sub>PO</sub> ≤ 0.8 µs makes the start-up overhead negligible against the sweep window.

| Block | Part | Qty | I typ (mA) | Duty | Subtotal avg (mA) |
|---|---|---:|---:|---:|---:|
| Main MCU @ 170 MHz | STM32G474CBT6 | 1 | ~30 | 100 % | 30 |
| Wing MCU @ 170 MHz | STM32G474CBT6 | 2 | ~30 | 100 % | 60 |
| Key Hall (gated via wing AO3401A) | SC4015 | 71 | 2.5 | 25 % | ~44 |
| Push/pull Hall (gated via main AO3401A) | SC4015 | 2 | 2.5 | ≤ 25 % | ≤ 1.3 |
| Analog mux (static) | 74HC4052 | 9 | <0.01 | 100 % | ~0 |
| LEDs (POW + ~3 status) | — | ~4 | 3 | — | ~10 |
| LDO I<sub>Q</sub> + leakage | TLV755P + misc | — | — | — | ~1 |
| **Total average (playing, USB-MIDI streaming)** | | | | | **~146** |

At ~146 mA × 3.3 V ≈ **0.48 W** delivered, average. Drawn from 5 V VBUS through a linear regulator (3.3 / 5 = 66 % efficiency), the input side sees ≈ **146 mA at 5 V / 0.73 W average** — comfortably inside the USB 2.0 default 500 mA / 2.5 W budget, with plenty of margin for a low-power 100 mA / 500 mW port if ever needed.

**Peak vs. average.** During the ~240 µs scan window the gated-Hall current adds ~183 mA (73 × 2.5 mA) on top of the always-on ~101 mA (MCUs + LEDs + LDO Iq), so instantaneous draw spikes to **~284 mA**. The 4.7 µF bulk cap (C54) and the LDO's transient response cover the step. Outside the window the rail is back to ~101 mA. Both wings scan independently (separate MCUs), but the LDO sees the sum — worst case both sweeps overlap; the motherboard's push/pull window is small enough to be scheduled inside the same envelope.

**LDO thermal — comfortable after gating.** TLV755P (SOT-23-5) dissipates **(5 V − 3.3 V) × 0.146 A ≈ 250 mW** averaged. θ<sub>JA</sub> ≈ 195 °C/W gives ~49 °C rise — junction at ~74 °C in 25 °C ambient, ~89 °C in 40 °C ambient. Well below the 125 °C abs-max. Peak-current excursions (~284 mA × 1.7 V ≈ 483 mW) for 240 µs every 1 ms barely budge the junction temperature thanks to the package's thermal mass. The earlier review item — buck regulator or larger LDO package — is no longer required; the SOT-23-5 LDO is the right pick for the gated load profile.

If gating ever has to be disabled (e.g., during bring-up before firmware controls the AO3401A gates), the LDO falls back to ~300 mA / 510 mW worst-case, which is at the edge of SOT-23-5's thermal envelope — keep that in mind for first-power-on without firmware. Default the gate to OFF at reset (P-FET gate pulled high by R40 to VCC) so the hall sensors are de-energised until firmware enables them.

## Wing Power and Bus Interface

Each wing is powered directly from the motherboard's 3.3 V rail through pins 1 and 12 of its 2×6 1.27 mm SMD connector. There is **no per-wing power switch (no TPS2553), no gated wing rail, and no back-feed buffer (no SN74LVC125A)**. With the battery removed, there is no operating mode that powers the motherboard while leaving a wing off — when VBUS is present the whole instrument is powered, when VBUS is gone everything is off — so the back-feed path that the isolation buffer was protecting against no longer exists in any normal operating state.

**Wing reset and presence handling.**

- **NRST** is driven open-drain by the motherboard MCU through a BAT54C steering diode to 3.3 V (U53 main reset, U54 wing reset). The wing MCU's internal NRST pull-up provides idle-high. The open-drain topology means the motherboard can never source current into a wing that is somehow held in reset by another agent (e.g., a programmer connected to a wing).
- **READY** is a passive input on the motherboard with the internal pull-down enabled. The wing drives the line actively high once its firmware has initialised. If a wing is absent, unprogrammed, or still booting, the pull-down resolves the line to "not ready" and the motherboard simply doesn't poll that wing — no external resistor needed.

**Wing-port ESD.** The SRV05-4 arrays at the wing connectors (TV1/TV2 left, TV3/TV4 right) tie pin 5 to the motherboard's 3.3 V rail. Since this is the same rail that powers the wings, ESD steering currents stay in a single power domain.

## Wing Bus Connector

The motherboard↔wing bus uses an **S062100026 — 2×6 (12-pin), 1.27 mm pitch, SMD vertical header** (CN1 = right wing, CN2 = left wing). This is a compact, low-profile SMD part; the mating cable assembly must use the matching 1.27 mm pitch crimp housing.

The 12-pin layout carries 9 signals, 2 VCC, and 3 GND returns — see [architecture.md](architecture.md) for the full pin table. Compared to the earlier 10-pin 2.54 mm design:

- **BOOT0 added** (pin 11) — enables the motherboard MCU to force a wing into system bootloader mode without a physical button. See §"Remote Wing Programming via BOOT0" below.
- **Second VCC pin** (pin 12) — shares power delivery across both connector rows, reducing pin current.
- **1.27 mm vs. 2.54 mm pitch** — smaller footprint, but no longer Dupont-compatible; a 1.27 mm crimp cable or a flex-cable assembly is required. The trade-off is board area vs. cable-swap ergonomics.

## PCB Manufacturing Strategy: JLCPCB Economic, 2-Layer

All three boards (motherboard, left wing, right wing) are manufactured via **JLCPCB Standard (Economic) service**: 2 copper layers, no buried/blind vias, 1 oz copper weight, standard soldermask and silkscreen.

**2-layer constraint — no dedicated VCC plane:**

- **Layer 1:** Signal + GND plane (split GND where necessary)
- **Layer 2:** Signal + power traces (5 V VBUS and 3.3 V routed as traces, not planes; the wings receive 3.3 V directly through their connectors — no separate per-wing rail)

**Implications for power distribution:**

1. **3.3 V rail:** Distributed via wide traces (24–32 mil minimum) rather than a continuous plane. The TLV755P LDO output and the wing-connector VCC pins require direct copper widths to minimize resistance and transient voltage sag.

2. **No distributed plane capacitance:** Decoupling capacitors become critical — they are the only capacitive reservoir for transient current. Each power rail segment (motherboard main, left wing, right wing) gets dedicated bulk (10 µF) and bypass (100 nF per IC) capacitance placed within 5 mm of the load.

3. **GND plane preserved:** The single GND plane on Layer 1 serves as the common return path for all signals and power. This is the primary defense against noise coupling to analog sensor lines (Hall outputs, ADC inputs). Sensitive traces route on Layer 2 with GND return traces of equal width run parallel and adjacent.

4. **Thermal management:** Average dissipation ~250 mW in the TLV755P (SOT-23-5), within its thermal budget at the gated load profile — see "Power Consumption". Thermal vias under the LDO tie directly to the GND plane for passive cooling.

**Trade-off:** Economic 2-layer design saves cost (~20 % vs. 4-layer) at the expense of denser PCB layout, stricter decoupling discipline, and slightly tighter voltage margins. The ~146 mA average current at 3.3 V (and ~284 mA peak during Hall scan windows) is well within the LDO's 500 mA rating.

## Passive Form Factor

Default passive footprint for both resistors and capacitors is **0603**, picked for hand-rework ergonomics on this 2-layer prototype-friendly design. **0402** is reserved exclusively for the **MCU load capacitors** that sit directly between the package's 0.5 mm-pitch pads — the 100 nF per-VDD bypass (C8, C9, C10) and the VDDA / VREF+ filter pair (C7/C8/C55/C57/C58 depending on board). The smaller pad geometry minimises the decoupling loop inductance that dominates supply impedance at the M4F's switching harmonics; nothing else on the board needs this. NRST, debounce, ESD, and signal-filter caps all stay 0603. Bulk reservoir caps (4.7 µF) stay in **0805** because the dielectric volume forces a larger package anyway.

## LED Brightness Scheme

Two current-limit tiers, by visual purpose:

- **MainBoard FN status LEDs (R29/R35/R36 = 330 Ω → ~4 mA)** — sit *behind* their respective function buttons (SW2/SW3/SW4 = FN1/FN2/FN0) and glow through the translucent button cap. The higher current overcomes the diffuser loss so the LED is legible at-a-glance during play. SW1 (global NRST) has no LED.
- **All other LEDs (1 kΩ → ~1.3 mA)** — POW on all three boards, READY on each wing, and the wing's FN LEDs (which are not used as runtime indicators in current firmware). These are open-air status LEDs at modest 0805 brightness; 1 kΩ is enough to be obviously-on under office lighting without being distracting on a dark stage.

Net LED current contribution to the rail: ~10 mA total worst-case across the system, well inside the 500 mA LDO budget.

## Function and Boot Buttons

### MainBoard

Four SMD tactile switches (SW1–SW4):

| Designator | Function | GPIO | Logic | Pull | LED |
|---|---|---|---|---|---|
| SW1 | Global NRST | — (NRST via BAT54C) | Pulls all 3 MCU NRST lines low when pressed | None | No |
| SW2 | FN1 | PB6 | **Low when pressed** (ties GPIO to GND) | MCU internal pull-up | Yes (330 Ω, behind cap) |
| SW3 | FN2 | PB4 | **Low when pressed** (ties GPIO to GND) | MCU internal pull-up | Yes (330 Ω, behind cap) |
| SW4 | BOOT0 / FN0 | PB8 / BOOT0 | **High when pressed** (ties PB8 to 3.3 V) | **External 10 kΩ pull-down (R1) to GND** | Yes — LED_FN0, R9 (330 Ω), driven by **PB9**, behind SW4 cap |

**Global NRST (SW1).** Pressing SW1 drives the open-drain NRST node through the BAT54C network, pulling all three MCU NRST pins (motherboard + both wings) low simultaneously — a full system reset without USB disconnect. SW1 has no GPIO and no LED.

**BOOT0 / FN0 (SW4).** SW4 is dual-purpose: held at power-on (or during NRST via SW1), BOOT0 = 1 forces the motherboard MCU into system bootloader (USB DFU). In normal operation PB8/BOOT0 is sampled only at reset; firmware may thereafter drive PB8 as GPIO (FN0). The external 10 kΩ pull-down (R1) holds BOOT0 low when SW4 is open, providing a defined idle state — unlike FN1/FN2 which rely on the MCU's internal pull-ups. Active-high logic: press = FN0 asserted / DFU condition.

**FN1, FN2 (SW2, SW3).** Active-low, rely on MCU internal pull-ups. No external resistors.

### Wing Boards

Each wing board carries one dedicated **BOOT0 pushbutton** wired the same way as SW4 on the motherboard: the switch ties BOOT0 directly to 3.3 V, with no external pull resistor (the wing's BOOT0 idle state is managed by the motherboard's R_BOOT0 / L_BOOT0 GPIO driving the line low through the bus connector). There are no FN buttons on the wing boards; all user-facing controls are on the motherboard. With remote BOOT0 available via the wing bus connector (see §"Remote Wing Programming via BOOT0"), the physical wing BOOT0 button is a fallback for bench use only.

## MCU Decoupling (STM32G474CBT6, LQFP48)

All three boards run the **same STM32G474CBT6** in LQFP48 with the same decoupling pattern, per the G474 datasheet "Power supply scheme" and AN4488. The LQFP48 package has:

- **3 × VDD pins** (24, 36, 48) — one 100 nF X7R per pin within a few mm of the package (C8, C9, C10, 0402). **VBAT** (pin 1) is tied to VCC since no backup battery is fitted; it requires no dedicated bypass cap — the G474 datasheet Figure 16 does not call for one, and the VDD decoupling covers it.
- **1 × 4.7 µF bulk** ceramic on VDD (C54, 0805). Provides the local reservoir absent a 3.3 V plane on the 2-layer stackup. An additional 1 µF (C55) sits on the same rail.
- **VDDA filter** (pin 21): a ferrite bead between VDD and VDDA (L1, GZ1608D601TF, 600 Ω @ 100 MHz, 200 mA — comfortable for VDDA at ~10 mA when the ADC runs), plus 1 µF (C49) + 10 nF (C48) on VDDA referenced to VSSA. The ferrite is justified on every board: the motherboard reads the pedal pots, and the wings sample 32–40 Hall channels through the on-chip ADC.
- **NRST cap**: 100 nF (C39) close to the NRST pin, per AN4488.

**No HSE crystal.** G474's USB-FS uses HSI48 with the Clock Recovery System (CRS), locked to USB SOF — so no external crystal is needed for USB enumeration, and no 32.768 kHz LSE either (no RTC requirement). PF0/PF1, freed up from OSC_IN/OSC_OUT, are repurposed as GPIO (right-wing SPI2 NSS/SCK on the motherboard; bank-mux control on the wings).

## Key Sensing: Magnet / Hall-sensor Pairing

Each key uses a **Gateron Magnetic Jade Pro mini** switch (KS-33D, total travel 3.5 ± 0.2 mm) paired with an **SC4015SO-N** linear Hall sensor mounted on the opposite side of the 1.5 mm PCB (SMD, marked face toward the switch).

**Polarity.** Gateron specifies the magnet N-pole facing down toward the PCB. The SC4015**-N** variant is activated by N-pole approach to its marked face, so the geometry is correct: as the key is pressed, V<sub>OUT</sub> moves from the quiescent 2.2 V *down* toward 0.8 V.

**Field at the sensor pad** (Gateron-spec, numbers already through the PCB):

| Key state | Field @ sensor | V<sub>OUT</sub> typ (2.0 mV/Gs) |
|---|---|---|
| Rest (0 mm) | 75 ± 10 Gs | ≈ 2.05 V |
| Full press (3.5 mm, 500 gf) | 680 ± 70 Gs | ≈ 0.84 V |

Gateron's figures assume their reference PCB (≈ 1.6 mm). Our 1.5 mm stack puts the sensor 0.1 mm closer to the magnet — field slightly higher, inside the ±10 % tolerance.

**Usable ADC swing**: ~1.2 V across 3.5 mm, i.e. ~1500 LSB on a 12-bit / 3.3 V ADC (~430 LSB/mm). In the worst-case combination (field at +10 %, sensor sensitivity at its +10 % limit), output clamps at the 0.8 V floor during the last ~0.1–0.2 mm of travel — no effect on note detection or velocity since the velocity-relevant portion of travel is mapped well before bottom-out.

## 3.3 V Rail (TLV755P)

TLV755P input (pin 1, IN) is fed from **VBUS** through the USB front-end (TVS clamp, optional polyfuse/eFuse). VBUS is the only upstream source — no battery, no buck, no charger — so the LDO sees the standard USB 2.0 5 V ± tolerance (4.40 V min at full 500 mA draw with worst-case cable IR drop, 5.25 V max).

- **Dropout headroom:** the TLV755P needs ≥ 3.55 V for 3.3 V / 500 mA per datasheet. Comfortable even at the worst-case 4.40 V VBUS minimum, with margin remaining after the TVS clamp's negligible leakage drop.
- **Hot-plug behaviour:** when the cable is connected, VBUS ramps over ~10 ms; the LDO follows; both wings come up on the same 3.3 V rail simultaneously. Unplugging cuts power instantly — no orderly shutdown is performed (there is nothing to save: no battery state, and no log persistence beyond what the firmware writes synchronously to flash).
- **Input decoupling:** 10 µF bulk + 100 nF bypass within a few mm of the LDO input pin, with the TVS clamp anchored between them and the USB connector.

See the "Power Consumption" section above for the thermal analysis — at the gated load profile the SOT-23-5 package is adequate.

## Push/Pull Sensor (spring blade + hall sensors)

The instrument replaces the acoustic bellows with a **flexure-style spring blade** whose deflection is read by two SC4015 linear hall sensors on the motherboard (U57, U58). As the player pushes or pulls, the blade flexes one way or the other; a small magnet on the moving end translates that deflection into a bipolar field at the sensor faces. The two sensors are read into ADC channels PB1 and PB12 — one biased to detect push, the other pull — and the firmware differences them to recover the signed effort with rest-position cancellation.

**Why this over a loadcell.** The first prototype used a loadcell + CS1237 24-bit Σ-Δ ADC. The electrical path was clean, but the mechanical feel was wrong: the loadcell's near-zero compliance means the player gets no proprioceptive feedback as they ramp effort — it plays like pressing on a wall. A spring blade gives a small, defined travel proportional to effort, which is what bandoneonists already expect from the bellows. The hall-based readout also fits the existing per-key sensor BOM (same SC4015 part, same supply, same ADC pipeline), so no new parts class is introduced.

**Why two sensors.** A single hall on a unipolar magnet only resolves one direction. With two sensors mounted on either side of the blade's neutral position (or one above / one below, with opposing N-pole orientations), push and pull each push exactly one sensor below quiescent and leave the other at rest. Subtracting the two readings gives a signed value through zero, with the resting offset cancelled — robust against temperature drift in either sensor.

**Power.** Both sensors share the motherboard's gated `VDDH` rail (see "Power Consumption") via Q1 (AO3401A) — sampled in step with the rest of the hall network, default-OFF at reset.

The deferred loadcell + CS1237 path is no longer on the BOM. If a future variant ever wants the bridge approach (e.g., a force-curve mode for non-bandoneonists), CS1237 is still a credible choice — see archived notes.

## Pedals

Two TRS 6.35 mm jacks on the motherboard, with mechanical presence sensing (switched contact that reports whether a plug is inserted):

- **Sustain port** — expects a TS pedal shorting Tip–Sleeve. Digital input with pull-up; presence sense gates the MIDI sustain event.
- **Expression port** — accepts M-Audio EX-P / Roland EV-5 and compatible TRS pots. The MCU measures **R(Tip-Ring)** via a **3.9 kΩ pull-up** (R7, basic-part E24 value — nearest standard to 4 kΩ, < 3 % deviation, negligible effect on the voltage range). Voltage varies from ~0.67 V (min) to ~2.5 V (max) — 56 % of the ADC range, drawing ≤ 0.85 mA. The same measurement also accepts a TS sustain pedal plugged into this jack (Tip–Ring short = full deflection), so users who only have a sustain pedal are not stuck.

### Expression pedal reference measurements

M-Audio EX-P and Roland EV-5 have the wiper on the TRS tip.

| Knob | R(Ring-Sleeve) | R(Tip-Sleeve) heel / tip | R(Tip-Ring) heel / tip |
|---|---|---|---|
| max | 65 kΩ | 53.64 / 64 kΩ | 12.46 / 1 kΩ |
| min | 11.47 kΩ | 1 / 12.46 kΩ | 12.46 / 1 kΩ |

Setting M-Audio mode to "Other" swaps tip and ring.

## USB Connector Choice (USB-B)

The instrument uses a **USB Type-B** receptacle (USB1, right-angle SMD). The choice is deliberate:

- **Mechanical robustness**: USB-B connectors have a large, deep socket with four substantial through-hole anchor pins. The mating plug latches positively and requires a deliberate pull to remove — no accidental disconnects during performance.
- **Cable retention**: the square-socket geometry prevents the cable from wiggling free when the instrument is moved on stage or rested on a stand.
- **Standard MIDI device convention**: USB-B is the established connector for hardware synthesisers, keyboards, and audio interfaces. Experienced musicians already own USB-A-to-B cables.

USB-C would offer a smaller footprint and reversibility, but its connector body is less robust to lateral stress and the cable can be unplugged with a light tug — an unacceptable failure mode mid-performance. No PD negotiation is performed (VBUS is taken as-is at 5 V), so the CC pull-down resistors that USB-C requires add complexity for no benefit here.

## Programming Port (STDC14 Pogo Pads)

Each board (motherboard and both wings) exposes **14 pogo-pin landing pads** laid out per the **STDC14** pinout (ST's modern compact debug standard). There is **no connector body** on the BOM — the pads are bare copper, touched by a **TC2070-IDC-050** spring-loaded pogo jig pressed against the board when programming or debugging.

**Why no connector.** A through-hole or SMD STDC14 header would consume board area touched only at the factory and during occasional rework. The pogo-pad approach leaves the footprint as a solder-free land pattern; the jig is held by hand or a fixture for the few seconds needed to flash firmware.

**Mating jig**: Tag-Connect TC2070-IDC-050 (0.050" / 1.27 mm pitch, 14-pin, with legs). The legs anchor the jig to the board during the programming cycle. This jig is **not on the BOM** — it is a workshop tool, one per programmer setup.

STDC14 carries: SWD (SWDIO/SWCLK), SWO trace, NRST, UART VCP (TX/RX), target VCC reference, and GND returns — everything needed for flashing, halting, real-time tracing, and serial console.

## Remote Wing Programming via BOOT0

The wing bus connector (CN1/CN2) carries a **BOOT0** line (pin 11) driven by the motherboard MCU:

- CN1-11 (`R_BOOT0`) → motherboard GPIO **PB13**
- CN2-11 (`L_BOOT0`) → motherboard GPIO **PA3**

The motherboard also controls wing **NRST** (CN1/CN2 pin 4, open-drain via BAT54C). Together these two lines allow the motherboard to force either wing into the STM32G474 system bootloader:

1. Assert BOOT0 high (motherboard drives PB13/PA3 → 3.3 V).
2. Assert NRST low, then release it.
3. Wing MCU boots with BOOT0 = 1 → enters system bootloader (supports USART, SPI, I2C protocols).
4. Motherboard communicates with the wing bootloader over the existing SPI bus wires (MOSI/MISO/SCK/NSS), which the STM32G474 bootloader can expose as an SPI slave interface.
5. Motherboard receives new firmware from the USB host and relays it to the wing — a full OTA-style wing update without touching the wing board.

This eliminates the need to physically access the wing's BOOT0 button or STDC14 pads for firmware updates in the field. The STDC14 pads remain available for initial factory programming and debug.

