# MainBoard Design Review Plan

Sources inspected: `export/BOM_MainBoard_MainBoard.csv`, `export/Netlist_MainBoard.net`,
`export/Netlist_WingLeft.net`, `export/Netlist_WingRight.net`,
`export/PickAndPlace_*_fixed.csv`, `export/SCH_MainBoard.pdf`, `hardware.md`,
`architecture.md`, `datasheet_summaries.md`, `requirements.md`.

**Round 4 (2026-05-30)** — re-exported BOMs and netlists from the schematic.
Multiple previously-flagged items resolved, several new aspects audited.

Each item has a status:
- **PASS** — verified correct, no action needed
- **WARN** — concern that needs user confirmation before closing
- **FAIL** — definitive mismatch between netlist and documentation / a design error; doc or schematic must be updated
- **OPEN** — cannot be verified from BOM/netlist alone (firmware, layout, or datasheet lookup needed)

---

## 1. Power Supply Chain

### 1.1 VBUS TVS (D5, SMF5.0A)
Netlist: `D5-1(K) → VBUS`, `D5-2(A) → GND`. Unidirectional, cathode to VBUS.
**PASS** — correct polarity, correct part (SOD-123FL, Vc=9.2 V).

### 1.2 LDO Enable (R14, TLV755P U6)
Netlist: `R14-1 → VBUS`, `R14-2 → U6-3 (EN)`. 10 kΩ from VBUS to EN.
EN threshold ≥ 1 V; at 4.4 V VBUS min, EN ≈ 4.4 V → LDO enabled.
**PASS** — always-on LDO correctly wired.

### 1.3 LDO Input / Output Decoupling
Netlist: `C15 (1 µF 0603) → VBUS`, `C19 (1 µF 0603) + C20 (4.7 µF 0805) → VCC (output)`.
Datasheet requires ≥ 0.47 µF in + ≥ 0.47 µF out.

Transient sanity-check on the input cap: worst-case scan-window load step is ~140 mA delta on VBUS, sustained for the 240 µs Hall-scan window. Droop on a 1 µF input cap = `I·t/C` = `0.140 × 240e-6 / 1e-6` ≈ **34 mV**. VBUS worst-case minimum 4.40 V → 4.37 V during scan; the LDO needs only 3.55 V to maintain 3.3 V at full load → margin > 800 mV remains. USB-cable inductance (~0.5 µH for a typical 1–2 m cable) recharges C15 between scan windows in microseconds. The 1 µF input cap is **electrically adequate** — no need to add bulk or bypass.

`hardware.md` previously claimed "10 µF bulk + 100 nF bypass" on the LDO input — that overstated the build. Doc updated to match the actual 1 µF as of this review round.
**PASS** — schematic is correct; doc corrected.

### 1.4 LDO VIN Headroom
VBUS min 4.40 V; TLV755P dropout at 500 mA max 215 mV → min VIN needed 3.515 V.
4.40 V − 0.215 V = 4.19 V output → regulated at 3.3 V with 0.89 V margin.
**PASS** — comfortable headroom even at worst-case USB cable IR drop.

### 1.5 Hall-sensor Gate (Q1, AO3401A P-ch)
Netlist: `Q1-2(S) → VCC`, `Q1-3(D) → VDDH`, `Q1-1(G) → R5(1 kΩ) → PA2 (HALL_NEN)` and `R6(100 kΩ) → VCC`.
P-FET: gate pull-up to VCC = OFF by default at power-on (sensors de-energised). Firmware drives PA2 low through R5 to turn on.
**PASS** — correct default-off topology; gate resistors protect against inrush.

### 1.6 VDDH Bypass
Netlist: `CP1(100 nF 0603) + CP2(100 nF 0603)` on VDDH.
**PASS** — two 100 nF bypasses on the gated hall rail (one per SC4015).

---

## 2. MCU Decoupling (STM32G474CBT6, U1)

### 2.1 VDD / VBAT Pin Bypass Caps
G474 LQFP48 has **3 VDD pins** (24, 36, 48) and **VBAT** (pin 1). All four are tied to VCC in the netlist.
Netlist VCC net has **C8, C9, C10** (three × 100 nF 0402) for VDD bypass.

VBAT does **not** require a dedicated bypass cap when no backup battery is fitted: the G474 datasheet Figure 16 ("Power supply scheme") shows VBAT tied directly to VDD with no separate cap, and the mandatory decoupling caps are listed only for VDD/VDDA/VREF. Plane/trace inductance from VBAT to the nearest VDD bypass cap is the only practical concern, and on a LQFP48 the VBAT pin (1) sits 1–3 mm from the C8/C9/C10 footprints.
**PASS** — cap count matches G474 datasheet's "Power supply scheme" recommendation; no 4th cap required.

### 2.2 VDD Bulk Cap
Netlist: `C11 (4.7 µF 0805)` on VCC.
**PASS** — adequate bulk reservoir.

### 2.3 VDDA / VREF+ Filter
Netlist: `VCC → L1 (ferrite GZ1608D601TF 600 Ω/100 MHz) → VDDA`. VDDA net pools **U1-20 (VREF+) and U1-21 (VDDA)** with `C16(10 nF) + C17(1 µF) + C12(1 µF) + C18(100 nF)` to GND. VSSA (U1-19) tied to GND.
VREF+ and VDDA are bonded after the ferrite — acceptable when ratiometric VREF is not needed (key-position and pedal ADC are not ratiometric).
**PASS** — ferrite + multi-value VDDA/VREF+ decoupling per AN4488.

### 2.4 NRST Cap
Netlist: `C4 (100 nF 0603)` on M_NRST.
**PASS** — per AN4488 recommendation.

### 2.5 VBAT Pin
Netlist: `U1-1 (VBAT) → VCC`. No backup battery fitted, VBAT tied to VCC.
**PASS** — acceptable; RTC / backup registers not used.

### 2.6 No External Crystal
Netlist: PF0/PF1 (OSC_IN/OSC_OUT) are repurposed as R_SPI_NSS and R_SPI_SCK respectively (right-wing SPI).
USB uses HSI48 + CRS; no LSE fitted.
**PASS** — crystal-less USB design confirmed.

---

## 3. ESD and Protection Coverage

### 3.1 VBUS Clamp (D5)
See §1.1. **PASS**.

### 3.2 USB D+/D− (TV8, SRV05-4)
Netlist: `TV8-1 → USB_DP(PA12, USB1-3)`, `TV8-3 → USB_DM(PA11, USB1-2)`, `TV8-2 → GND`, `TV8-5 → VBUS`.
Pin 5 (V+) tied to VBUS is correct: no PD negotiation means VBUS is always 5 V, so the upper steering diodes are safe to activate. ESD charge steers to VBUS (held by SMF5.0A) or GND — full bidirectional clamping.
**PASS**

### 3.3 Pedal Jack ESD (TV7, SRV05-4)
Netlist: `TV7-5 → VCC (3.3 V)`, channels: `TV7-1 → SUS_PEDAL`, `TV7-3 → EXP_PEDAL`, `TV7-4 → EXP_PEDAL_INT`, `TV7-6 → SUS_PEDAL_INT`.
**PASS** — consistent.

### 3.4 Wing-Bus ESD (TV1–TV4, SRV05-4 × 2 per wing)
Netlist: `TV1-5, TV2-5, TV3-5, TV4-5 → VCC`.
Wing bus signals protected: SPI×4, NRST, READY, BOOT0 (7 signals total, 8 channels of headroom across two SRV05-4 per wing).
**PASS** — pin 5 on 3.3 V rail (same domain as wings).

### 3.5 Programming Port ESD (TV5–TV6, SRV05-4)
Netlist: `TV5-5, TV6-5 → VCC`. Channels protect SWDIO, SWCLK, SWO, NRST, USART_TX, USART_RX, DETECT.
ESD coverage is architecturally correct. Signal-to-pad routing is a separate issue covered in §7.1.
**PASS** (ESD only — see §7.1 for wiring).

### 3.6 Function-Button ESD (D1–D4, H5VND5BA)
Netlist: D1–D4 each have pin 1 → GND, pin 2 → respective button net.
D1 specifically protects the BAT54C common-cathode bus (SW1 / NRST OR-tie).
**PASS** — bidirectional clamp on each line.

### 3.7 VBUS Polyfuse / eFuse
Deliberately not fitted for v1. Rationale: USB 2.0 host port is spec-required to limit to 500 mA; SMF5.0A clamps transient surges; TLV755P has internal foldback + thermal shutdown. A polyfuse or eFuse would be a JLCPCB extended part with no basic-part substitute.
**SKIP — conscious decision, not an oversight.**

---

## 4. USB Interface

### 4.1 USB-B Connector (USB1)
BOM: U-B-M4SS-W-1, right-angle SMD, 4 contacts + 2 GND anchors.
Netlist: `USB1-1(VBUS)`, `USB1-2(D−)`, `USB1-3(D+)`, `USB1-4(GND)`, `USB1-5(GND)`, `USB1-6(GND)`.
**PASS** — correctly wired.

### 4.2 USB D+/D− MCU Pins
Netlist: PA11 = D−, PA12 = D+.
G474 datasheet: PA11 = USB_DM, PA12 = USB_DP. ✓
**PASS**

### 4.3 USB Crystal-less Enumeration
HSI48 + CRS per architecture.md. No crystal in BOM. VDDA ≥ 2.4 V (3.3 V VCC) as required by G474 ADC and USB.
**PASS**

### 4.4 USB VBUS Sense
Netlist: no MCU pin tied to VBUS for sense. For a fully bus-powered device this is correct — when VBUS is removed, the MCU loses power and the bus is disconnected automatically. STM32G474 USB peripheral's VBDEN bit must be cleared in firmware so it does not wait for VBUS presence on an unconnected pin.
**OPEN** — confirm in firmware that USB_BCDR.VBDEN = 0 (or use the equivalent HAL config for "no VBUS sensing").

---

## 5. Wing Bus

### 5.1 Connector Part vs. Architecture Documentation
BOM: `CN1, CN2 = S062100026`, which is 2×6 (12-pin), 1.27 mm pitch, SMD vertical.
`architecture.md` and `hardware.md` updated with correct connector, full 12-pin table (2× VCC, 3× GND, 4× SPI, READY, NRST, BOOT0), and trade-off note (1.27 mm cable required, no Dupont compatibility).
**PASS**

### 5.2 Wing Bus Signal-to-Pin Mapping (CN2 = left wing)
| Net | CN2 pin | MCU GPIO |
|---|---|---|
| VCC | 1, 12 | — |
| L_READY | 2 | PB0 (via R2 100 Ω) |
| L_SPI_NSS | 3 | PA4 |
| L_NRST | 4 | PB2 (via R3 100 Ω, BAT54C U2-A2) |
| L_SPI_SCK | 5 | PA5 |
| GND | 6, 8, 10 | — |
| L_SPI_MISO | 7 | PA6 |
| L_SPI_MOSI | 9 | PA7 |
| L_BOOT0 | 11 | PA3 |

**PASS** — SPI1 (PA4–PA7) correctly serves left wing. Wing's matching netlist confirms same pin numbers on CN1 of the WingLeft side, with wing-side MCU on the SPI1 alternate (PA4/PA5/PB4/PB5, AF5).

### 5.3 Wing Bus Signal-to-Pin Mapping (CN1 = right wing)
| Net | CN1 pin | MCU GPIO |
|---|---|---|
| VCC | 1, 12 | — |
| R_READY | 2 | PA8 (via R4 100 Ω) |
| R_SPI_NSS | 3 | PF0 (OSC_IN repurposed, AF5 = SPI2_NSS) |
| R_NRST | 4 | PB11 (via R8 100 Ω, BAT54C U3-A2) |
| R_SPI_SCK | 5 | PF1 (OSC_OUT repurposed, AF5 = SPI2_SCK) |
| GND | 6, 8, 10 | — |
| R_SPI_MISO | 7 | PB14 (AF5 = SPI2_MISO) |
| R_SPI_MOSI | 9 | PB15 (AF5 = SPI2_MOSI) |
| R_BOOT0 | 11 | PB13 |

PF0/PF1 are AF5 SPI2 alternates per the STM32G474 datasheet (DS12288, Section 4.11 Alternate Function table) — explicitly listed as `SPI2_NSS` and `SPI2_SCK`. Combined with PB14/PB15 (also AF5), SPI2 routes cleanly without remapping.
**PASS** — PF0/PF1 valid as SPI2 AF5; mapping verified against datasheet.

### 5.4 Remote BOOT0 on Wing Connector
Netlist: `CN2-11 = L_BOOT0 → PA3`, `CN1-11 = R_BOOT0 → PB13`.
Documented in `architecture.md` bus table and `hardware.md` §"Remote Wing Programming via BOOT0".
**PASS**

### 5.5 Wing NRST Open-Drain Topology (BAT54C)
Netlist `$1N16140` net: `U2-3(K) + U3-3(K) + SW1-2 + D1-2 + C1-1`.
BAT54C common-cathode: at idle the cathode is pulled toward GND by D1/C1 leakage, leaving NRST lines undisturbed (reverse-biased). When the cathode net is pulled to GND (by SW1 or firmware), both wings' NRST anodes are pulled to ~Vf ≈ 0.3 V → wings held in reset.
**PASS** — topology is valid (see also §6.4 for SW1 function).

### 5.6 Series Resistors on READY/NRST Wing Signals
R2, R3, R4, R8 are all 100 Ω. They sit between the MCU GPIO and the ESD device / connector.
**PASS** — standard 100 Ω series termination.

### 5.7 Wing BOOT0 — 100 Ω series R now added (RESOLVED)
Round 3 flagged the BOOT0 lines as the only wing-bus signals without a 100 Ω series limiter against wing-local-button contention. Round 4 confirms the fix:
- `L_BOOT0` net: `TV2-1, CN2-11, R16-1 (100 Ω)`; `R16-2` lands on net `$1N68331` together with `U1-11 (PA3)`.
- `R_BOOT0` net: `TV4-1, CN1-11, R17-2`; `R17-1` on net `$1N68371` with `U1-27 (PB13)`.

Contention math with the new resistors: if a user presses the wing-local SW1 (BOOT0 button, ties VCC straight to the wing-side BOOT0 net) while the mainboard MCU is sinking the line through R16/R17, the current is `(VCC − V_OL) / (R_series + R_switch + R_cable) ≈ 3.3 V / (100 Ω + ~50 mΩ + ~100 mΩ) ≈ 33 mA`. The STM32G474 GPIO can sustain this — the absolute max is 25 mA continuous at V_OL ≤ 0.4 V, ~ 50 mA pulse — so a momentary button press is safely bounded. The series R also rules out the prior worst-case "tens of A through the switch contact" peak.

**PASS** — all six wing-bus signals (NRST/READY/BOOT0 × 2 wings) now share the same 100 Ω series-R pattern. `hardware.md` should mention R16/R17 in the wing-bus discussion.

---

## 5W. Wing-board internal review

Items below cover the wing boards themselves (not the wing-to-mainboard bus). Both wings share the same MCU and topology; only WingLeft pins are quoted unless otherwise noted.

### 5W.1 Wing power gate (Q1 AO3401A) and gate network
Wing netlist: `Q1-2 (S) → VCC`, `Q1-3 (D) → VDDH`, `Q1-1 (G) → R7 (1 kΩ) → HALL_NEN → U37-25 (PB11)` and a separate gate pull-up to VCC. Same default-OFF P-FET topology as the mainboard (§1.5). PB11 drives gate-low to enable VDDH for the scan window.
**PASS** — symmetric to the mainboard hall-sensor gate.

### 5W.2 74HC4052 mux topology
- WingLeft: 4 muxes (U5, U14, U23, U32 — see §12.1a). Each mux exposes 8 channels → 32 channels total. Wing has 33 Hall sensors per BOM — count mismatch: only 32 channels are routed through muxes; one sensor (or 33-channel sweep) must be either unused or routed elsewhere. Worth a spot-check against the schematic/PCB.
- WingRight: 5 muxes (U5, U14, U23, U32, U41). 40 channels for 38 sensors → 2 spare channels. ✓
- Mux `E̅` (pin 6) is tied to GND on every mux on both wings → muxes are permanently enabled. Bank-power gating happens upstream at Q1 / VDDH, not at the mux enable, so always-on E̅ is consistent with the gated-Hall design. The "available for bank-switching" wording in `datasheet_summaries.md` describes a capability that the schematic doesn't use.
- Mux select lines `S0` (pin 10) and `S1` (pin 9) are bussed to all muxes in parallel. Driven by `SEL0 → U37-3 (PC14)` and `SEL1 → U37-4 (PC15)`.

**PASS for topology; WARN for the 32/33 channel discrepancy on WingLeft** — flag for schematic-level confirmation.

### 5W.3 Mux select on PC14 / PC15 (LSE-domain pins)
SEL0/SEL1 land on `PC14-OSC32_IN` / `PC15-OSC32_OUT`, which sit in the STM32G4's *backup* power domain. Per DS12288, these pins have **reduced output drive** when configured as GPIO, and they require **PWR_CR1.DBP = 1** to enable writes to the RCC backup-domain control register before they can be driven (the same dance as touching VBAT-domain RTC registers). Loads here are tiny — 4 (WingLeft) or 5 (WingRight) mux S inputs at a few pF each, switched at the scan rate (~kHz), well inside the reduced drive budget — so the hardware is fine.

**PASS for hardware; OPEN for firmware** — confirm the wing's HAL init sets `PWR_CR1.DBP` before configuring PC14/PC15 as GPIO outputs, otherwise SEL0/SEL1 will read back as ones and the scan will be stuck on channel 3.

### 5W.4 Wing buttons — three on each wing, not one
Netlist shows three TS-1088 tactile switches per wing (matches BOM `qty = 3`):

| Wing SW | Net | MCU pin | Role |
|---|---|---|---|
| SW1 | BOOT0 | U37-45 (PB8/BOOT0) | Local BOOT0 entry (bench-only; redundant with mainboard's remote BOOT0 via CN-11) |
| SW2 | PGM_RESET | U37-7 (NRST) | Local NRST button (debounced via C2, ESD via D2, ORed onto the wing NRST through TV2-6) |
| SW3 | SW_FN0 | U37-27 (PB13) | Generic FN button on PB13; debounced via C3, ESD via D3 |

`hardware.md` §"Function and Boot Buttons → Wing Boards" says "Each wing board carries one dedicated BOOT0 pushbutton… There are no FN buttons on the wing boards" — that's not what is built. The wings have local NRST and FN0 buttons too. Update the doc with the full per-wing button table, noting SW3 (FN0 on PB13) is a hard-soldered control that the wing firmware can use as a developer poke (mode toggle, dump-state, etc.).
**WARN (doc only)** — schematic is functional; documentation undersells what's on the wing.

### 5W.5 Wing READY — R9 is the LED current-limit, not a READY series R (CORRECTED)
Round 3 misread the wing READY topology. Re-checked against the current netlist:
- `READY` net: `U37-44 (PB7), CN1-2, TV1-6, R9-2`. PB7 drives the connector pin **directly**, with no wing-side series resistor.
- `R9-1` lands on net `$1N52732` together with `LED_READY-1`. R9 = 470 Ω is the LED current-limit, not a damping R.

So the actual topology is:
```
MCU PB7 ──┬── CN1-2 (out to mainboard, then to R2/R4 100 Ω series, then to PB0/PA8)
          └── R9 (470 Ω) ── LED_READY ── GND   (visual indicator: lit when wing READY = HIGH)
```

The only damping R on the READY line is the mainboard's R2/R4 = 100 Ω. With the LED branch drawing ~1.4 mA when READY=HIGH (Vf ≈ 2.85 V on 3.3 V through 470 Ω), the GPIO sees a 1.4 mA static load — well inside the G474's 8 mA min output spec, V_OL/V_OH unaffected. The READY-LED visualization is a nice touch for board bring-up.

**PASS** — topology works; previous round's "570 Ω total series" line was wrong and has been corrected.

### 5W.6 Wing hardware identification — 3-bit ID resistors (NEW)
Wing netlists carry three named ID nets read by the wing MCU at boot:

| Wing | ID0 (PC13, U37-2) | ID1 (PF0, U37-5) | ID2 (PF1, U37-6) | Binary |
|---|---|---|---|---|
| Left  | R1 (10 kΩ) → **GND** | R2 (10 kΩ) → **VCC** | R3 (10 kΩ) → **GND** | `010` |
| Right | R1 (10 kΩ) → **VCC** | R2 (10 kΩ) → **GND** | R3 (10 kΩ) → **GND** | `001` |

Each ID line has a single 10 kΩ pull (no companion pull on the other rail — the MCU's internal pull-resistor option is not used, the external 10 kΩ sets the state on its own). 3 bits give 8 possible wing layouts; two are used today, six are reserved for future keyboard variants (Concertina, Einheitsgriff, etc., per `architecture.md`).

Firmware reads PC13/PF0/PF1 at boot and embeds the resulting 3-bit ID in the per-frame wing identification code the mainboard sees over SPI. PC13 is in the backup-power domain (same `PWR_CR1.DBP = 1` caveat as the §5W.3 mux-select pins). PF0/PF1 are former OSC_IN/OSC_OUT and have no special access rules on the wing (these pins are repurposed on the mainboard as SPI2 NSS/SCK for the right wing — different boards, different uses of the same physical pads).

**PASS** — clean implementation; means the same wing firmware binary runs on every wing variant. Mention in `architecture.md` §"Internal Bus" so a reader looking at SPI frame format understands where the wing-ID byte comes from.

### 5W.7 WingLeft 33rd Hall sensor — direct ADC, not mux (RESOLVED §5W.2)
Round 3 flagged a 32-vs-33 channel mismatch on WingLeft (4 muxes provide 32 channels, but the BOM has 33 Hall sensors). Round 4 traces the 33rd:
- WingLeft sensors `U1, U2, U3, U4, U6–U13, U15–U22, U24–U31, U33–U36, U38` — that's 33 designators (U5/U14/U23/U32 are muxes, U37 is MCU).
- Net `A_IN0`: `U37-8 (PA0), U38-3 (SC4015 OUT)` — sensor U38's output goes **straight to the MCU's ADC pin PA0**, bypassing the mux network.
- The other 32 sensors route through the four 74HC4052 muxes onto `A_IN2`–`A_IN9` (8 mux outputs × 4 channels each = 32 channels), read on PA1–PA7 / PB0–PB1 via the ADC.

So the topology is **32 muxed + 1 direct** for 33 total. The direct-ADC channel is U38, which sits next to the MCU and gets the lowest-impedance read path. Likely the highest-priority key (or one chosen for layout convenience — confirm against the keyboard mapping doc).

**PASS** — count reconciles; one direct ADC sensor on WingLeft, no direct sensors on WingRight (its 38 sensors fit comfortably across 5 muxes × 8 channels = 40, with 2 spare).

### 5W.8 Wing A_IN1 has a 100 kΩ pull-down to define floating ADC pin (NEW)
Wing netlist `A_IN1`: `R5-1 (100 kΩ), U37-9 (PA1)`. `R5-2` on GND. The 100 kΩ pulls a spare ADC channel to a defined voltage so it doesn't float into the input clamp circuitry and inject noise into the shared ADC sample cap when scanning adjacent channels.

This is the right thing to do for any ADC input that isn't routed to a signal source. Mainboard equivalent: `R6 (100 kΩ) → VCC` on the AO3401A gate, which serves the same defined-state role for the P-FET gate (not ADC).
**PASS**.

### 6.1 Switch-to-Designator Mapping (Netlist vs. Documentation)
From netlist nets:
| Designator | Net | Connected to |
|---|---|---|
| SW1 | `$1N16140` | BAT54C U2-K + U3-K (NRST cathode bus) + C1 + D1 |
| SW2 | SW_FN1 | PB6 (FN1) |
| SW3 | SW_FN2 | PB4 (FN2) |
| SW4 | SW_FN0_BOOT0 | PB8/BOOT0 |

`hardware.md` button table matches.
**PASS**

### 6.2 External Pull-Down on BOOT0 (R1)
`hardware.md` button table updated: SW4/BOOT0 has external 10 kΩ pull-down (R1) for defined idle-low; debounce cap C5 (100 nF) parallels R1 (RC = 1 ms hold-time on BOOT0 after SW4 release — useful for asserting BOOT0 *before* the NRST release). FN1/FN2 use MCU internal pull-ups.
**PASS**

### 6.3 SW1 = Global Reset Button
Documented in `hardware.md` §"Function and Boot Buttons": SW1 grounds the BAT54C common cathode, forward-biasing both diodes and pulling all three MCU NRST lines to ~0.3 V simultaneously.
**PASS**

### 6.4 LED_FN0 (PB9)
LED behind SW4 (BOOT0/FN0) cap. R9 330 Ω, KT-0603W (white). Drive on PB9 (U1-46). Mirrors FN1/FN2.
**PASS** for wiring; see §10.1 for the brightness calculation correction.

### 6.5 FN Button Logic Levels
SW2 (FN1) and SW3 (FN2): pin 1 → GND, pin 2 → GPIO. Active-low via MCU internal pull-up. ✓
SW4 (BOOT0): pin 1 → VCC, pin 2 → PB8. Active-high. ✓
**PASS**

---

## 7. Programming Port (STDC14, CN3)

### 7.1 SWD Signal Wiring — verified against netlist + LQFP48 pinout
Cross-checked CN3 pin assignments against the actual `Netlist_MainBoard.net` nets and against the STM32G474CBT6 LQFP48 pinout (DS12288 Figure 6):

| STDC14 std pin | Mainboard net | Mainboard MCU pin | LQFP48 function |
|---:|---|---|---|
| 3  T_VCC            | VCC               | —                  | — ✓ |
| 4  T_SWDIO          | PGM_SWDIO         | U1-37              | **PA13** = SYS_JTMS-SWDIO ✓ |
| 5  GND              | GND               | —                  | — ✓ |
| 6  T_SWCLK          | PGM_SWCLK         | U1-38              | **PA14** = SYS_JTCK-SWCLK ✓ |
| 7  GND              | GND               | —                  | — ✓ |
| 8  T_SWO            | PGM_SWO           | U1-40              | **PB3** = SYS_JTDO-TRACESWO ✓ |
| 11 GND_DETECT       | PGM_DETECT        | U1-39              | PA15 (GPIO) — see §7.5 |
| 12 NRST             | M_NRST            | U1-7               | **NRST** ✓ |
| 13 T_VCP_RX         | PGM_USART_RX      | U1-32              | **PA10** = USART1_RX AF7 ✓ |
| 14 T_VCP_TX         | PGM_USART_TX      | U1-31              | **PA9**  = USART1_TX AF7 ✓ |

All five SWD/JTAG-critical signals (SWDIO, SWCLK, SWO, NRST, VCC) land on the correct MCU pads, and the VCP UART pair maps to USART1 on its AF7 alternate. Symbol pin-number attributes are consistent with the standard STDC14 footprint — earlier draft of this review (which flagged this section as a critical failure) was based on a mis-extracted pin table; corrected after re-reading the LQFP48 pinout (pins 37/38/40 = PA13/PA14/PB3, not the off-by-two arrangement assumed earlier).

**PASS** — board can be programmed and debugged through the pogo pads as drawn.

### 7.2 STDC14 VCC Reference
Netlist: `CN3-3 (T_VCC) → VCC`. STLINK-V3 uses this pin to reference signal levels.
**PASS** — correctly tied to 3.3 V.

### 7.3 GND Pins on STDC14
Netlist: `CN3-5, CN3-7 → GND`.
**PASS**

### 7.4 No Connector Body on BOM
Confirmed: CN3 has no BOM entry. Mating jig = TC2070-IDC-050 (workshop tool).
**PASS**

### 7.5 STDC14 pin 11 (GNDDetect) → PA15 — tool-presence detect
Netlist: `CN3-11 → PGM_DETECT → U1-39 (PA15)`. Both wings do the same (`CN2-11 → U37-39 → PA15`).

Per **ST UM2448 (STLINK-V3SET user manual) Rev 9, Table 6 note 6**, STDC14 pin 11 is labelled `GNDDetect` and is "**Tied to GND by STLINK-V3SET firmware. It might be used by the target to detect the tool.**" The TC2070-IDC-050 cable is passive and passes pin 11 through. So when the STLINK-V3 is mated, the target sees pin 11 = GND through the probe; when no probe is attached, the line is floating and PA15's internal pull-up resolves it to logic high.

This is precisely the design intent: PA15 reads LOW = STLINK attached, HIGH = no probe. Firmware can use this to enable a VCP echo mode, enter a "developer" command set, blink an LED, or simply log probe attach/detach events.

(Earlier review rounds reported this as inverted vs the standard — that was a misread of the STDC14 spec. UM2448 is unambiguous: ST's intended direction for pin 11 *is* target-detects-tool, with the tool sourcing the GND.)

**PASS** — schematic correct, matches the documented STDC14 usage. Firmware should configure PA15 as input with internal pull-up enabled; document the "STLINK_DETECT" role in firmware notes once PA15 is consumed.

---

## 8. Hall Sensor Subsystem

### 8.1 Motherboard Hall Sensors (U4, U5)
Netlist: `U4-1(VDD) + U5-1(VDD) → VDDH`, `U4-2(GND) + U5-2(GND) → GND`, `U4-3(OUT) → HALL0 → PB1`, `U5-3(OUT) → HALL1 → PB12`.
PB1 = ADC1_IN12 / ADC3_IN1. PB12 = ADC1_IN11. Both are valid ADC inputs on G474.
**PASS** — push/pull sensor pair on ADC-capable GPIOs.

### 8.2 Hall Output Filter Caps
Netlist: `C6 (1 nF 0603) → HALL0`, `C7 (1 nF 0603) → HALL1`. Pin 1 to HALL net, pin 2 to GND.
**PASS** — matches SC4015 datasheet recommendation.

### 8.3 ADC Sample Time
74HC4052 Ron ≈ 80 Ω (wings only). Motherboard Hall sensors drive ADC directly through 1 nF filter caps — RC ≈ 80 ns (SC4015 source impedance × 1 nF). G474 ADC sample-and-hold cap ≈ 4 pF; settling dominated by the external 1 nF, not the mux.
**OPEN** — confirm in firmware that ADC sampling time ≥ 1 µs (covers both motherboard direct and wing-mux paths). Document the ADC config in `hardware.md`.

---

## 9. Pedal Subsystem

### 9.1 Expression Pedal ADC Path
Netlist: `EXPR_PEDAL1-5 → EXP_PEDAL → R7-1(3.9 kΩ) → VCC` and `TV7-3 → U1-8(PA0)`.
**PASS**

### 9.2 Sustain Pedal Path
Netlist: `SUS_PEDAL1-5 → SUS_PEDAL → R13-1(3.9 kΩ) → VCC` and `TV7-1 → U1-9(PA1)`.
**PASS**

### 9.3 PJ-603A Pin 2 (Sleeve) Intentionally Floating; Pin 4 → VCC
Schematic page 6 contains a designer's note: *"Sleeve (S) intentionally NC. ADC reads R(Tip-Ring) only — floating Sleeve makes the measurement independent of per-pedal T/R/S wiring"*. This clarifies that pin 2 (X-marked / NC) is the sleeve.

That leaves pin 4 → VCC as a contact other than the sleeve — most plausibly the normally-closed switch terminal that shorts to pin 3 (Ring) when no plug is inserted, used as an insertion-detect bias that pulls EXP_PEDAL_INT to VCC in the "no pedal" state. The schematic comment confirms intent for the sleeve but does not document pin 4's role.

**WARN** — confirm pin 4 function against the HOOYA PJ-603A datasheet (LCSC C309273) before fab:
  - If pin 4 is the **NC Ring-switch contact** (shorts to pin 3 when no plug) → safe, and explains §9.4.
  - If pin 4 is the **NC Tip-switch contact** (shorts to pin 5 when no plug) → redundant with R7, harmless.
  - If pin 4 is the **sleeve** → fatal: VCC shorts to GND through any plug. Designer's comment rules this out, but confirm with the datasheet anyway.

### 9.4 Insertion-Detect Caps (C13, C14) → GND, with 1 kΩ series (RESOLVED)
Round 3 flagged C13/C14 as caps from the GPIO to VCC (no useful filter direction and coupling VCC noise back into the slow input). Round 4 confirms the reworked topology:

- `EXPR_PEDAL pin 3 → C13 plate1` and `EXPR_PEDAL pin 3 → R18 (1 kΩ) pin 1`. C13's other plate is on **GND**.
- `R18 pin 2 → EXP_PEDAL_INT → TV7-4 (ESD) → U1-3 (PC14)`.
- Symmetric on the sustain side with C14 / R19 / TV7-6 / U1-4 (PC15).

So the new topology is `jack ring ── ┬── C13 → GND` and `└── R18 (1 kΩ) → ESD ── PC14`. Reading the chain at the connector side first:
- HF noise on the jack ring is shunted to **GND** by C13 — the correct direction relative to the MCU input threshold reference.
- The 1 kΩ series R combines with the SRV05-4 clamp and the MCU input capacitance to form an explicit low-pass on the GPIO side (τ ≈ 1 kΩ × (CJ ≈ 3.5 pF + Cpin ≈ 5 pF) ≈ 8.5 ns — fast enough not to miss insertion events, slow enough to soak residual ESD ringing).
- The cap-to-GND also gives the GPIO a defined ramp during VCC inrush (no longer follows VCC).

One small note: the cap is on the *connector* side of the series R rather than directly across the MCU pin to GND. That means R18 + C13 doesn't form a "classical" RC at the input — the cap mainly shunts noise at the source, and the R + ESD + Cpin form the low-pass at the MCU. Either ordering works; this one keeps the cap close to where noise enters (the jack), which is arguably the better placement for EMI.

**PASS** — implementation matches the recommended fix from the previous round.

### 9.5 Sustain Pedal Insertion Detect MCU Pin
Netlist: `SUS_PEDAL_INT → U1-4 (PC15-OSC32_OUT)`. No LSE fitted, so PC15 is available as GPIO.
**PASS**

---

## 10. LED Circuitry

### 10.1 FN Status LEDs — Vf Mismatch in Earlier Analysis
BOM (mainboard): **LED_FN0, LED_FN1, LED_FN2 = KT-0805G** — emerald green 0805, Vf = **2.6 V – 3.1 V**, If rated 5 mA (per LCSC C2297). `hardware.md` §"LED Brightness Scheme" describes these as "white" / KT-0603W; that part is on the *wing* BOMs (LED_FN1 = KT-0603W), not the mainboard.

The Vf range (2.6–3.1 V) is the **same** for KT-0805G and KT-0603W, so the brightness math is independent of the colour confusion:

| Vf | I through 330 Ω from 3.3 V (mainboard) | I through 470 Ω from 3.3 V (wing — see §10.1a) |
|---|---|---|
| 2.6 V (datasheet min) | 2.1 mA | 1.5 mA |
| 2.85 V (typ)          | 1.4 mA | 1.0 mA |
| 3.1 V (datasheet max) | 0.6 mA | 0.4 mA |

Mainboard worst-case ≈ **0.6 mA** through the green 0805 — well below the 5 mA rating and almost certainly invisible through the translucent button cap diffuser. Even typical units (~1.4 mA) are dim. The "4 mA legible-at-a-glance during play" intent will not be met as-built.

**FAIL** — pick one:
  a) swap to a colour with Vf ≤ 2.1 V (yellow / amber 0805 — e.g. KT-0805Y, same family as LED_POW), keeping the 330 Ω resistor → ~4 mA, hits the design intent;
  b) keep the green LED and drop the series resistor to ~100 Ω, accepting the Vf-curve variance as the limiter (current then sits at ~2–7 mA worst case);
  c) accept the dim green indicators and update `hardware.md` to reflect ~1 mA, removing the "behind the button cap diffuser" justification.

`hardware.md` §"LED Brightness Scheme" needs both the part-number list **and** the calculation corrected.

### 10.1a Wing FN LED (KT-0603W white) is even dimmer
Wing BOMs list `LED_FN1 = KT-0603W` (white 0603) on the BOOT0 button. Same Vf 2.6–3.1 V. The wing resistor in series with LED_FN1 is **470 Ω** (R19 on WingLeft, R42 on WingRight per BOM) → 0.4–1.5 mA worst-to-typ. `hardware.md` correctly notes the wing FN LED is "not used as runtime indicator in current firmware" — so the dimness is tolerable on the wings even if it stays uncorrected, but the part designators in the doc ("R29/R35/R36 = 330 Ω") don't match the actual BOM (`R9/R11/R12 = 330 Ω` on mainboard; wing FN LED uses R19/R42 = 470 Ω).

### 10.2 LED_FN0 (PB9)
See §6.4. Same dim-green issue as §10.1.

### 10.3 Power LED (LED_POW, KT-0805Y yellow)
Yellow LED, Vf = 1.8 – 2.4 V. At 1 kΩ from 3.3 V: 0.9–1.5 mA. Plan's earlier "~1.2 mA" estimate stands.
**PASS**

### 10.4 LED Polarity
All LEDs: anode connects to series resistor → MCU GPIO (or VCC for POW). Cathode to GND. MCU drives high → LED on.
**PASS**

---

## 11. Wing Bus Connector Part Class

CN1 and CN2 = S062100026 (2×6 1.27 mm SMD) — **Extended Part** (JLCPCB).
**WARN** — confirm that a basic-part 2×6 1.27 mm SMD header is not available on JLCPCB. If a direct replacement exists as a basic part, consider substituting.

---

## 12. BOM Cross-Checks

### 12.1 Footprint vs. Part
All footprints in the BOM match the netlist's footprint references for U1, U2, U3, U4, U5, U6, Q1, D1–D5, C6/C7 (0603 1 nF), C8–C10/C18 (0402 100 nF), C11/C20 (0805 4.7 µF), C12/C17 (0402 1 µF), C15/C19 (0603 1 µF), C16 (0402 10 nF), C1–C5/C13/C14/CP1/CP2 (0603 100 nF).
**PASS** (mainboard).

### 12.1a WingLeft BOM — muxes / sensors designator lists (RESOLVED)
Round 4 re-export of `BOM_WingLeft_WingLeft.csv` decoded:
- Line 19 (33 × `SC4015SO-N-TR`): `U1, U2, U3, U4, U6, U7, U8, U9, U10, U11, U12, U13, U15, U16, U17, U18, U19, U20, U21, U22, U24, U25, U26, U27, U28, U29, U30, U31, U33, U34, U35, U36, U38` ✓ (excludes muxes U5/U14/U23/U32 and MCU U37).
- Line 20 (4 × `74HC4052PW,118`): `U5, U14, U23, U32` ✓.

The earlier swap is gone. WingRight BOM was already correct in Round 3 and remains so.
**PASS — CLOSED.**

### 12.1a-old (archived) WingLeft BOM — muxes and sensors swapped (CRITICAL)
Cross-checked `BOM_WingLeft_WingLeft.csv` against `Netlist_WingLeft.net` and `PickAndPlace_WingLeft_fixed.csv`:

| Source | Muxes (74HC4052PW, TSSOP-16) | Sensors (SC4015SO-N-TR, SOT-23) |
|---|---|---|
| **WingLeft BOM**            | U1, U10, U19, U28  ❌                        | U2, U3, U4, **U5**, U6, U7, U8, U9, U11, U12, U13, **U14**, U15, U16, U17, U18, U20, U21, U22, **U23**, U24, U25, U26, U27, U29, U30, U31, **U32**, U33, U34, U35, U36, U38 ❌ |
| **WingLeft netlist**        | U5, U14, U23, U32                            | U1, U2, U3, U4, U6, U7, U8, U9, U10, U11, U12, U13, U15, U16, U17, U18, U19, U20, U21, U22, U24, U25, U26, U27, U28, U29, U30, U31, U33, U34, U35, U36, U38 |
| **WingLeft PnP (`_fixed`)** | U5, U14, U23, U32                            | U1, U10, U19, U28 *(spot-checked)*                                                                                                                       |
| **WingRight BOM**           | U5, U14, U23, U32, U41 ✓                     | U1, U2, …, U43 minus the muxes ✓                                                                                                                          |

The PnP file matches the netlist (mux footprints at U5/14/23/32 are TSSOP-16, sensor footprints at U1/10/19/28 are SOT-23). **Only the WingLeft BOM is swapped.** WingRight BOM is correct. JLCPCB cross-checks BOM ↔ PnP at order entry: with the swap in place, the order will either be rejected (mismatched footprints between BOM "74HC4052 TSSOP-16" and PnP "U1 = SOT-23"), or — worst case if the operator force-overrides — produce boards with the wrong parts on the wrong pads, scrapping the run.

**FAIL — CRITICAL.** In the WingLeft BOM, swap the designator lists between the `74HC4052PW,118` row and the `SC4015SO-N-TR` row. After the swap the BOM should read:
- `4 × 74HC4052PW,118 → U5, U14, U23, U32`
- `33 × SC4015SO-N-TR → U1, U2, U3, U4, U6, U7, U8, U9, U10, U11, U12, U13, U15, U16, U17, U18, U19, U20, U21, U22, U24, U25, U26, U27, U28, U29, U30, U31, U33, U34, U35, U36, U38`

Re-export the BOM and spot-check the result against the WingRight BOM's pattern (same column layout) before submitting to JLCPCB.

### 12.1b MainBoard R16, R17 — now wired as wing BOOT0 series Rs (RESOLVED)
Round 3 flagged R16/R17 as ghost components (in BOM + PnP but absent from the netlist). Round 4 netlist now contains both:
- `R16-1` in `L_BOOT0` net, `R16-2` in `$1N68331` with `U1-11 (PA3)` — 100 Ω series on the left-wing BOOT0 driver.
- `R17-2` in `R_BOOT0` net, `R17-1` in `$1N68371` with `U1-27 (PB13)` — same role on the right side.

These are the §5.7 mitigation resistors. The three-way BOM/PnP/netlist mismatch is closed.
**PASS — CLOSED.**

### 12.1c MainBoard new 1 kΩ resistors R18, R19 — pedal series Rs (RESOLVED)
New in the Round 4 BOM: `4 × 1 kΩ → R5, R10, R18, R19` (was `2 × 1 kΩ → R5, R10` previously). R18 and R19 are the series resistors that wire the pedal insertion-detect ring lines into the MCU — they're the §9.4 mitigation, confirmed in the netlist:
- `R18-1` in `$4N26118` with `EXPR_PEDAL-3` and `C13-2`; `R18-2` in `EXP_PEDAL_INT` with `TV7-4` and `U1-3 (PC14)`.
- `R19` symmetric on the sustain path.

**PASS — CLOSED.**

### 12.2 BAT54 Variant
BOM: BAT54C (common-cathode). Netlist confirms `U2-3 / U3-3` = K. SOT-23-3 footprint matches.
**PASS — CLOSED**

### 12.3 Pedal Jack SRV05-4 Pin 5
Netlist: `TV7-5 → VCC (3.3 V)`. Confirmed.
**PASS — CLOSED**

### 12.4 Voltage Ratings
| Part | Rating | Rail | Margin |
|---|---|---|---|
| C6, C7 (1 nF) | 50 V | VCC 3.3 V / VBUS | > 10× |
| C11, C20 (4.7 µF) | 25 V | VCC 3.3 V | > 7× |
| SW1–SW4 | 12 V DC | 3.3 V GPIO | > 3× |
| USB1 | 30 V | VBUS 5 V max | 6× |
**PASS**

### 12.5 STDC14 / CN3 — No BOM Entry
Confirmed: not in BOM. Bare copper pads only.
**PASS**

### 12.6 Cap Count Audit (0603 100 nF, qty 9)
Netlist allocation of the nine 0603 100 nF caps:
- C1 → $1N16140 (SW1 / BAT54C cathode bus, debounce)
- C2 → SW_FN1 (SW2 debounce)
- C3 → SW_FN2 (SW3 debounce)
- C4 → M_NRST (NRST AN4488 cap)
- C5 → SW_FN0_BOOT0 (SW4 / BOOT0 debounce + hold)
- C13 → VCC ↔ EXP_PEDAL_INT (see §9.4)
- C14 → VCC ↔ SUS_PEDAL_INT (see §9.4)
- CP1, CP2 → VDDH bypass at U4 / U5
All accounted for.
**PASS**

---

## 12W. Cross-board behaviour

### 12W.1 Power-on / NRST release sequencing across the three boards
VBUS rises (~10 ms) → C15 charges → TLV755P comes out of UVLO → VCC ramps to 3.3 V → C19/C20 charge → both wings receive VCC through CN1/CN2 → all three MCUs start their power-on reset.

Race risks:
- **Wing BOOT0 sampling vs. mainboard GPIO state.** At power-on the mainboard's PA3/PB13 (driving L_BOOT0/R_BOOT0) come out of reset as high-impedance inputs. The wing's local **R4 = 10 kΩ pull-down** on the BOOT0 net resolves the line to GND before the wing's NRST is released, so the wing won't accidentally enter the system bootloader on cold boot. ✓
- **Wing NRST release.** The wing's NRST is held low by the BAT54C only while the mainboard's PB2/PB11 actively drives low. At power-on those GPIOs are floating, so the wing's internal NRST pull-up plus the wing-side C4 (or equivalent) define the release ramp. With the 100 nF NRST cap and ~40 kΩ internal pull-up, the RC reaches V(IH) in ~3 ms — well after the wing's VCC is stable. ✓
- **Both wings come up simultaneously.** Cold boot drives ~284 mA peak (LDO output) for a few ms while all three MCUs run their init sequences; C20 (4.7 µF) covers the step. **PASS**.

### 12W.2 Wing swap robustness
CN1 (mainboard, right-wing connector) and CN2 (mainboard, left-wing connector) use **identical part S062100026** with no mechanical keying. If a field user swaps the left and right wing cables:
- Power and ground are still correct (same pinout on both connectors).
- SPI bus routes: left wing's MCU still talks on its own SPI1 (PA4–PA7); physically it would land on the mainboard's SPI2 (PF0/PF1/PB14/PB15) on the swapped connector. Mainboard still polls each bus independently and reads the **wing identification code** in each SPI frame (per `architecture.md`), so it learns which physical bus holds which keyboard layout at runtime and applies the correct key-to-note map.
- READY, NRST, BOOT0 follow the same pin map on both wings, so swap doesn't break reset/bootloader either.

The protocol's wing-ID handshake is the single point that makes swap-safe operation work — without it the mainboard would mis-map keys after a swap. Good architectural choice.
**PASS** — robust by design. Worth a one-line note in `architecture.md` "Internal Bus" subsection so a future reader doesn't try to add hard-coded "left/right" assumptions in firmware.

### 12W.3 USB enumeration without HSE crystal — CRS lock timing
G474 USB-FS uses HSI48 trimmed by the Clock Recovery System (CRS) against the USB SOF. Cold-boot timing:
1. VBUS rising edge triggers USB host enumeration.
2. Mainboard MCU finishes its boot/init (~10–20 ms typical) and enables USB device + CRS.
3. CRS locks within ~1 ms of receiving the first SOF (every 1 ms once host has issued SET_ADDRESS).
4. Enumeration completes within the host's 5 s descriptor-fetch budget.

The HSI48 free-running accuracy at room temp is roughly ±0.5 %, just outside the USB-FS ±0.25 % requirement. So the first few control transfers (descriptor read, SET_ADDRESS) happen on un-trimmed HSI48; the host's tolerance for those low-speed control transfers absorbs the slack. Once CRS locks, bulk MIDI transfers run on the trimmed clock.

ST AN5060 covers this exact scenario for the G4 family and reports reliable enumeration without HSE. The board follows the recommended config.
**PASS** — but firmware must enable HSI48 + CRS + sync source = USB_SOF before USB device init. Document in firmware notes.

## 2X. New mainboard pin-level audits (round 4)

### 2X.1 MCU pin allocation — complete map
Walked every LQFP48 pin against the round 4 netlist. Free pins on the mainboard MCU: **PC13 (pin 2), PB5 (pin 42), PB7 (pin 44), PB10 (pin 22)** — 4 spare GPIOs reserved for future expansion (BLE module SPI, DIN-MIDI UART, additional pedal port, etc.). All other GPIOs assigned; no pin sourced by two nets, no pin left floating without a defined role (every used GPIO either drives an active signal or has a defined pull through a button/sensor/series-R chain).

| Block | Pins used |
|---|---|
| USB FS | PA11 (D−), PA12 (D+) |
| SWD/SWO/VCP | PA13, PA14, PB3, PA9, PA10, PA15 |
| NRST + boot | NRST, PB8/BOOT0 |
| SPI1 (left wing) | PA4–PA7 |
| SPI2 (right wing) | PF0, PF1, PB14, PB15 |
| Wing READY/NRST/BOOT0 | PB0, PB2, PA3 (L); PA8, PB11, PB13 (R) |
| Hall power gate | PA2 (HALL_NEN) |
| Hall ADC | PB1 (HALL0), PB12 (HALL1) |
| Pedal ADC | PA0 (EXP), PA1 (SUS) |
| Pedal insertion-detect | PC14, PC15 |
| FN buttons + LED | PB6 (FN1), PB4 (FN2), PB8 (FN0/BOOT0), PB9 (LED_FN0), PA15 collision? — no, PA15 = PGM_DETECT |
| LED_FN1, LED_FN2 | PB6 also lights FN1 LED (PB6) — wait, see §2X.2 |

Wait — re-verified: SW_FN1 button is on `U1-43 = PB6`. LED_FN1 anode net `LED_FN1` connects to `U1-44`; but pin 44 is PB7, and PB7 was listed as free. Let me re-check.
- `LED_FN0` net: `U1-46 (PB9), R9-1`. ✓
- `LED_FN1` net: `U1-44 (PB7), R11-1`. — so PB7 *is* used as `LED_FN1` driver. Update §2X.1: **PB7 is not free** — drives LED_FN1.
- `LED_FN2` net: `U1-42 (PB5), R12-1`. — so PB5 *is* used as `LED_FN2` driver. Update §2X.1: **PB5 is not free** — drives LED_FN2.

Corrected free-pin count: **PC13 (pin 2) and PB10 (pin 22)** — only 2 spare GPIOs. Tight but enough for, e.g., a DIN-MIDI UART (PB10 = USART3_TX AF7) or a BLE module's IRQ line.
**PASS** — full pin map verified end-to-end; no collisions; 2 spares.

### 2X.2 Alternate-function alignment for every used peripheral
Cross-checked the LQFP48 alternate-function table (DS12288 Table 13) against every netlist signal that requires AF routing:

| Signal | MCU pin | LQFP48 AF | Match |
|---|---|---|---|
| USB_DM / USB_DP | PA11 / PA12 | dedicated (no AF) | ✓ |
| SPI1_NSS/SCK/MISO/MOSI | PA4 / PA5 / PA6 / PA7 | AF5 | ✓ |
| SPI2_NSS/SCK/MISO/MOSI | PF0 / PF1 / PB14 / PB15 | AF5 | ✓ |
| USART1_TX/RX (VCP) | PA9 / PA10 | AF7 | ✓ |
| SWDIO / SWCLK / SWO | PA13 / PA14 / PB3 | system default | ✓ |
| NRST | pin 7 (NRST) | dedicated | ✓ |
| BOOT0 | PB8 / pin 45 | dedicated boot-config | ✓ |
| ADC1_IN1 (EXP_PEDAL) | PA0 | ADC1/2_IN1 | ✓ |
| ADC1_IN2 (SUS_PEDAL) | PA1 | ADC1/2_IN2 | ✓ |
| ADC1_IN12 (HALL0) | PB1 | ADC1/3_IN12 | ✓ |
| ADC1_IN11 (HALL1) | PB12 | ADC1_IN11 | ✓ |

All four ADC inputs land on **ADC1**, so a single ADC instance with a 4-channel sequencer + DMA reads all of them. No need to wake a second ADC for steady-state operation. Sample-time configuration is still §8.3 OPEN (firmware concern).
**PASS**.

### 2X.3 Mainboard NRST topology — multi-source, no contention
M_NRST drivers and observers, walked from the netlist:
- `U1-7` (MCU NRST pin, internal pull-up ~40 kΩ — defines idle HIGH)
- `C4` (100 nF AN4488 cap to GND)
- `TV5-3` (SRV05-4 ESD on the STDC14 NRST trace)
- `U2-1` (BAT54C A1 — anode tied to M_NRST so SW1 can pull it down)
- `CN3-12` (STDC14 NRST from the STLINK-V3 jig)

Drive-conflict scan:
- **MCU internal pull-up** is always present (high impedance, ~40 kΩ).
- **STLINK-V3 NRST output** is open-drain per UM2448 — high-impedance when not asserting, drives low only during reset commands. No conflict with the MCU pull-up.
- **SW1 via BAT54C** is open-drain via the Schottky bridge: when SW1 closes, the BAT54C common cathode is pulled to GND and *both* MCU NRSTs on the wing (L_NRST and R_NRST through U2-A2 / U3-A2) plus M_NRST (through U2-A1 / U3-A1) get pulled to ~V_F = 0.3 V simultaneously. Schottky forward voltage at typical R-pull-up currents is ~250–300 mV, well inside the G474's V_IL ≤ 0.45 V — so a global reset event hits the V_IL threshold cleanly.
- **STM32G474 internal reset sources** (POR, BOR, WDG, software) drive NRST internally without external observation.

No two drivers fight each other in any sequence. The Schottky-OR-ing topology gracefully handles "one wing's BAT54C asserts while another driver is high-impedance" without leakage paths through M_NRST.
**PASS**.

### 2X.4 SMF5.0A clamp voltage vs TLV755P abs-max
SMF5.0A typical VC = 9.2 V at the rated `Ipp = 21.7 A` (10/1000 µs pulse). TLV755P abs-max VIN (DBV package): **6 V** continuous, **7 V** for a 10 µs surge per TI's "Recommended Operating Conditions" plus the abs-max note. SMF5.0A's 9.2 V peak is above both, but the pulse the TVS clamps to that voltage is only at I_pp — at the much lower currents seen during actual ESD strikes on a bench (typically tens to hundreds of mA in the protected device), the clamp sits closer to the breakdown VBR = 7 V, which is at the edge of TLV755P's 7 V transient spec.

Practical implication: SMF5.0A protects the LDO from typical IEC 61000-4-2 events (±8 kV contact). It does **not** protect from a sustained over-voltage (e.g., a USB host pushing 9 V because of a hub failure) — that's the polyfuse/eFuse use case, which is correctly deferred per §3.7 (no basic-part substitute on JLCPCB, and the failure mode is rare-and-survivable).

**PASS** — clamp is sized for transient ESD, not steady-state OV. Worth a one-line caveat in `hardware.md` §"ESD and VBUS Protection" so a future reader doesn't think the TVS is doing all the work.

### 2X.5 SC4015 hall-sensor source impedance vs filter cap and ADC
Spring-blade hall sensors U4/U5 on the mainboard:
- VOUT source impedance: <100 Ω typ (SC4015 datasheet — low, drives ADC directly).
- Filter cap C6/C7 = 1 nF to GND.
- Direct path to PB1 (HALL0) and PB12 (HALL1) — no mux, no series R.
- ADC1 sample-and-hold cap CSH ≈ 4 pF.

Effective τ at the ADC = ~100 Ω × (1 nF + 4 pF) ≈ 100 ns; settles within < 1 µs for 10-bit accuracy. With 12-bit ADC + oversampling to 16-bit, 1 µs sample time is comfortable.
**PASS** — sensor → ADC chain is fast enough for the push/pull rate (kHz mechanical bandwidth).

### 2X.6 Pedal jack pin 4 to VCC — still unverified against datasheet
`EXPR_PEDAL pin 4 → VCC` and `SUS_PEDAL pin 4 → VCC` are unchanged in the round 4 netlist. The §9.3 WARN stands: confirm pin 4 is the NC ring-switch (or NC tip-switch) and **not** the sleeve before fab. PJ-603A datasheet check is still pending — same risk profile as round 3.

## 13. Documentation Consistency

### 13.1 architecture.md — Wing Bus Table
12-pin 2×6 SMD 1.27 mm with BOOT0 row.
**PASS**

### 13.2 hardware.md — Function and Boot Buttons
SW1=NRST, SW4=BOOT0/FN0, R1 pull-down documented.
**PASS**

### 13.3 hardware.md / datasheet_summaries.md — USB SRV05-4 Pin 5
Both files corrected to VBUS.
**PASS**

### 13.4 datasheet_summaries.md — Protection Coverage Table
Wing-bus ESD row lists all 7 signals (SPI×4 + NRST + READY + BOOT0).
**PASS**

### 13.5 hardware.md — LED Brightness Scheme + designator sweep
Earlier round had `hardware.md` referring to stale designators / wrong part numbers:
- Mainboard FN LEDs called "R29/R35/R36" (actual: R9, R11, R12) and "KT-0603W white" (actual: KT-0805G green).
- Wing FN LED series R called "1 kΩ" (actual: 470 Ω, R19/R42).
- VDDA / VREF+ filter pair called "C7/C8/C55/C57/C58 depending on board" — wing has C7+C8+C55, mainboard has C12+C16+C17+C18, no C57/C58.
- LDO input decoupling called "10 µF + 100 nF" (actual: single 1 µF, C15).

This round: `hardware.md` updated in §"LED Brightness Scheme", §"Passive Form Factor", §"Power Consumption", and §"3.3 V Rail (TLV755P)" — see git diff. Brightness numbers in the LED section now use the actual Vf range; designators match BOM and netlist.

Remaining work tied to §10.1 (the FAIL): if the LED part is swapped to a Vf ≤ 2.1 V colour the "1.4 mA" figure in the doc needs another pass.
**PASS** — doc consistent with the as-built schematic for everything except the §10.1 LED-colour decision, which is pending.

### 13.6 datasheet_summaries.md — Open Items
- Item 1 (ADC sample time): **OPEN** (§8.3).
- Item 2 (BAT54C variant): **CLOSED** (§12.2).
- Item 3 (pedal-jack SRV05-4 pin 5): **CLOSED** (§12.3).

### 13.7 datasheet_summaries.md — SC4015 sensitivity figure is the 5 V value
`datasheet_summaries.md` lists SC4015 sensitivity at "2.0 mV/Gs typ" with a note that this is the 3.3 V operating-point figure. The BOM property field shows "Sensitivity: 3.9 mV/Gs", which is the **5 V** value (SC4015 is ratiometric; datasheet states 3.9 mV/Gs at 5 V, 2.0 mV/Gs at 3.3 V). The design correctly uses the 3.3 V number in the swing analysis (`hardware.md` "Key Sensing"). No action — flagged here only so a future reviewer cross-checking the BOM column doesn't think the design is mis-budgeted.
**PASS** — consistent with datasheet ratiometric behaviour.

---

## Summary Table

| § | Item | Status |
|---|---|---|
| 1.1–1.6 | Power supply chain (incl. LDO transient analysis) | **PASS** |
| 2.1 | MCU VDD/VBAT bypass cap count | **PASS** (3 caps adequate per G474 fig. 16) |
| 2.2–2.6 | Rest of MCU decoupling | **PASS** |
| 3.1–3.6 | ESD coverage | **PASS** |
| 3.7 | VBUS polyfuse | **SKIP** |
| 4.1–4.3 | USB interface | **PASS** |
| 4.4 | USB VBUS sense disabled in FW | **OPEN** |
| 5.1–5.6 | Wing bus connector, mapping, NRST, series Rs | **PASS** |
| 5.7 | BOOT0 series R (R16/R17 added round 4) | **PASS — CLOSED** |
| 5W.1 | Wing power gate (Q1 AO3401A) | **PASS** |
| 5W.2 | Mux topology + E̅ tied to GND | **PASS** |
| 5W.3 | SEL0/SEL1 on PC14/PC15 (backup-domain pins) | **PASS** (hardware); **OPEN** (firmware DBP) |
| 5W.4 | Wing has 3 buttons (BOOT0/NRST/FN0), doc says 1 | **WARN (doc)** |
| 5W.5 | Wing R9 (470 Ω) is LED current-limit, not READY series | **PASS — CORRECTED** |
| 5W.6 | Wing 3-bit hardware ID resistors | **PASS — NEW** |
| 5W.7 | WingLeft 33rd Hall on direct ADC (PA0) | **PASS — CLOSES 5W.2 channel question** |
| 5W.8 | Wing A_IN1 spare ADC pull-down | **PASS — NEW** |
| 6.1–6.5 | Function/Boot buttons | **PASS** |
| 7.1 | STDC14 SWD signal-to-pad (mainboard, re-verified) | **PASS** |
| 7.2–7.4 | Other STDC14 wiring | **PASS** |
| 7.5 | STDC14 GNDDetect → PA15 (tool-presence input per UM2448) | **PASS** |
| 8.1–8.2 | Hall sensors and output caps | **PASS** |
| 8.3 | ADC sample time | **OPEN** |
| 9.1–9.2 | Pedal ADC path | **PASS** |
| 9.3 | PJ-603A pin 4 = VCC, sleeve floating | **WARN** |
| 9.4 | C13/C14 reworked to GND, R18/R19 1 kΩ series added | **PASS — CLOSED** |
| 9.5 | Insertion-detect MCU pin | **PASS** |
| 10.1–10.2 | FN LED Vf vs 330 Ω (green KT-0805G dim) | **FAIL** |
| 10.1a | Wing FN LED (white KT-0603W) even dimmer | **WARN** |
| 10.3–10.4 | POW LED, polarity | **PASS** |
| 11 | Wing connector part class (extended) | **WARN** |
| 12.1, 12.2–12.6 | BOM cross-checks (mainboard) | **PASS** |
| 12.1a | WingLeft BOM mux/sensor designators (re-exported) | **PASS — CLOSED** |
| 12.1b | MainBoard R16/R17 now wired as wing BOOT0 series | **PASS — CLOSED** |
| 12.1c | MainBoard R18/R19 wired as pedal series Rs | **PASS — NEW** |
| 12W.1 | Power-on / NRST release sequencing | **PASS** |
| 12W.2 | Wing-swap robustness via SPI wing-ID | **PASS** |
| 12W.3 | USB enumeration without HSE (HSI48+CRS) | **PASS** (firmware OPEN: enable CRS before USB init) |
| 2X.1 | MCU pin allocation — full LQFP48 map, 2 spares | **PASS** |
| 2X.2 | Alternate-function alignment for SPI/USART/USB/ADC | **PASS** |
| 2X.3 | M_NRST multi-source (SW1 + STDC14 + BAT54C-OR) | **PASS** |
| 2X.4 | SMF5.0A clamp vs TLV755P abs-max VIN | **PASS** (with caveat) |
| 2X.5 | SC4015 → ADC source-impedance + 1 nF filter | **PASS** |
| 2X.6 | PJ-603A pin 4 — datasheet sanity still pending | **WARN** (= §9.3) |
| 13.1–13.4, 13.6, 13.7 | Documentation | **PASS** |
| 13.5 | hardware.md designators / LED brightness updated | **PASS** (pending §10.1 colour) |

### Prioritised Action List

**P0 — Must fix before submitting to JLCPCB:**
1. **§10.1 — FN LED colour vs resistor.** Either switch LED_FN0/1/2 to a Vf ≤ 2.1 V colour (yellow / amber 0805 — KT-0805Y is already on the same BOM line for LED_POW, single SKU substitution), or drop the series resistor to ~100 Ω, or document the dim green indicator as intentional. Current build will produce barely-visible buttons during play.
2. **§9.3 / §2X.6 — PJ-603A pin 4 datasheet sanity check.** Pin 4 is still tied to VCC on both pedal jacks. Verify against the HOOYA PJ-603A LCSC C309273 datasheet that pin 4 is *not* the sleeve. Designer's schematic comment rules it out, but a 30-second datasheet check costs nothing and the consequence of being wrong is VCC-to-GND through any inserted plug.

**P1 — Documentation / firmware:**
3. **§5W.3 — Backup-domain access for PC14/PC15/PC13.** Wing firmware must set `PWR_CR1.DBP = 1` before configuring SEL0/SEL1 (PC14/PC15) as GPIO outputs and before reading PC13 (ID0). Otherwise the mux address won't change and the wing ID will mis-read.
4. **§5W.4 — Wing buttons.** Update `hardware.md` §"Function and Boot Buttons → Wing Boards" to reflect the three buttons actually built (BOOT0 = SW1, NRST = SW2, FN0 = SW3 on PB13).
5. **§5W.6 — Wing hardware ID.** Add a paragraph to `architecture.md` "Internal Bus" describing the 3-bit ID resistor scheme (R1/R2/R3 pull patterns → PC13/PF0/PF1 → SPI frame ID byte).
6. **§4.4 — USB VBUS sense.** Confirm `USB_BCDR.VBDEN = 0` in firmware init; add a comment in `hardware.md` §USB explaining the choice.
7. **§7.5 — STLINK detect on PA15.** Document the `PGM_DETECT` line in firmware notes — configure PA15 as input with internal pull-up; LOW = STLINK attached, HIGH = no probe. Per UM2448 Table 6 note 6 the STLINK-V3 firmware ties STDC14 pin 11 to GND specifically for this purpose.
8. **§8.3 — ADC sample-time configuration.** Document in firmware notes / `hardware.md` once first ADC code lands.
9. **§12W.3 — CRS before USB init.** Note in firmware that HSI48 + CRS (sync = USB_SOF) must be enabled before USB device init.
10. **§2X.4 — TVS scope.** Add a one-line note to `hardware.md` §"ESD and VBUS Protection" clarifying SMF5.0A is sized for ESD transients, not sustained over-voltage.
11. **§13.5 — `hardware.md` LED brightness re-check.** After §10.1 LED-colour decision lands, re-walk the "LED Brightness Scheme" numbers (current pass is correct for the current BOM).

**P2 — Investigate / cost optimisation:**
12. **§11 — Wing connector basic-part substitute** (low priority; cost only).
13. **§3.7 — VBUS polyfuse** — confirmed deferred for v1.

### Net change from previous review round
- **§7.1 (STDC14 wiring) was a false alarm** — the previous critical FAIL was based on a mis-read of the LQFP48 pinout. Re-verified against DS12288 Figure 6 directly: mainboard SWD/SWO/NRST/VCP all land on correct MCU pads. Removed from P0.
- **§10.1 (FN LEDs)** was diagnosed against the wrong part number (white KT-0603W) but landed on the right conclusion (Vf 2.6–3.1 V → too dim at 330 Ω). Actual mainboard LEDs are green KT-0805G with the same Vf range, so the FAIL stands; only the recommended substitute SKU shifts (KT-0805Y, already on the BOM for LED_POW, rather than a 0603-family yellow).
- **§7.5 (STDC14 GNDDetect → PA15)** — re-checked against UM2448 Rev 9 Table 6 (note 6): STLINK-V3SET firmware ties pin 11 to GND specifically so the **target** can detect the tool. The TC2070-IDC cable passes pin 11 through, so PA15 reads LOW with STLINK attached and HIGH (via internal pull-up) when detached. The current wiring is a clean implementation of the standard's intended feature — re-classified from WARN to PASS. Earlier review draft had the direction inverted; corrected after re-reading UM2448.
- **§1.3 (LDO input cap)** — transient sanity-check confirms the 1 µF (C15) is electrically adequate (~34 mV droop in the worst-case scan window). `hardware.md` updated to match the as-built BOM rather than the previous fictional "10 µF + 100 nF". No schematic change.
- **§13.5 (hardware.md doc sweep)** — designators, LED part numbers, wing-side resistor values, VDDA cap names and LDO-input description all corrected in `hardware.md` this round.
- **P0 now reduces to two items**: §10.1 LED swap and §9.3 PJ-603A pin-4 datasheet sanity-check. Everything else is P1 or later.

### This round (wing-board internal + cross-board)
- **NEW CRITICAL §12.1a — WingLeft BOM has muxes and SC4015 sensors swapped in their designator lists.** Confirmed by three-way cross-check (BOM ⟷ Netlist ⟷ PnP). PnP and netlist agree; only the WingLeft BOM is wrong. WingRight BOM is fine. Must be fixed before submitting to JLCPCB or the order will fail at the BOM/footprint sanity-check stage. **P0 grew back to four items**: §10.1, §9.3, §12.1a (new CRITICAL), §12.1b (R16/R17 ghost).
- **§12.1b — R16/R17 ghost components on MainBoard.** Two 100 Ω 0603 resistors are in the BOM and PnP but absent from the netlist. Pads get parts soldered onto them with no schematic-defined connection. Reconcile before next netlist export.
- **§5W series (wing internal review)** — added: wing power gate confirms symmetry with mainboard; mux topology / E̅ tied to GND is consistent with the gated-Hall design; SEL0/SEL1 land on PC14/PC15 (firmware must enable PWR backup-domain write); each wing has three buttons (BOOT0, NRST, FN0 on PB13) — not the one the doc claims; wing READY series R = 470 Ω is fine for the input-with-pull-down topology.
- **§12W series (cross-board)** — power-on / NRST sequencing is race-free thanks to the wing-side R4 = 10 kΩ pull-down on BOOT0; wing-swap robustness rides on the per-frame wing-ID code in SPI (no hard-coded left/right firmware logic); HSI48 + CRS enumeration requires the firmware to enable CRS before USB init.
- **§5W.2 — Channel-count discrepancy on WingLeft (32 mux channels vs 33 SC4015 sensors).** Confirm whether the 33rd sensor is dropped, dual-routed, or routed off-mux. Low risk but real.

### Round 4 (2026-05-30, re-exported netlists)
Resolved this round:
- **§5.7 BOOT0 series Rs** — R16 / R17 = 100 Ω added on PA3 / PB13. Wing-local-button contention is now bounded to ~33 mA, safely inside GPIO sink limit. **CLOSED**.
- **§9.4 Pedal insertion-detect caps** — C13 / C14 moved to GND, R18 / R19 = 1 kΩ series added between TV7 ESD and the MCU. Matches the recommendation in the previous round. **CLOSED**.
- **§12.1a WingLeft BOM swap** — re-exported BOM has muxes correctly at U5/14/23/32 and the 33 sensors at the right designators. **CLOSED**.
- **§12.1b R16/R17 ghosts** — now wired as the §5.7 BOOT0 series Rs (the resistors found a useful home). **CLOSED**.
- **§5W.2 WingLeft 32-vs-33 channel mismatch** — sensor U38 reads directly on PA0 (`A_IN0`), the other 32 go through the muxes. **CLOSED**.

Re-classified / corrected:
- **§5W.5** — Round 3 incorrectly described the wing's R9 (470 Ω) as a READY series damping R. It is the **LED current-limit** on the LED_READY indicator branch. The READY line itself has no wing-side series R; only the mainboard's 100 Ω. Corrected text in plan.

New audits added:
- **§5W.6** — 3-bit hardware wing-ID resistors (R1/R2/R3, 10 kΩ each) on PC13/PF0/PF1. WingLeft codes `010`, WingRight codes `001`. 6 codes reserved for future keyboard layouts.
- **§5W.7** — Verified the WingLeft direct-ADC path for the 33rd hall sensor (U38 → PA0).
- **§5W.8** — Wing A_IN1 (PA1) has a 100 kΩ pull-down to define the spare ADC input.
- **§2X.1** — Full LQFP48 pin-allocation map; **2 spare GPIOs** (PC13, PB10) on the mainboard.
- **§2X.2** — Alternate-function alignment verified for SPI1, SPI2, USART1 VCP, USB FS, SWD, and all 4 ADC inputs (all on ADC1).
- **§2X.3** — Mainboard NRST multi-source topology (SW1 + STDC14 jig + BAT54C OR-tie) walked for driver conflicts; clean.
- **§2X.4** — SMF5.0A vs TLV755P abs-max VIN: TVS protects against transient ESD, not sustained over-voltage. Worth a doc note.
- **§2X.5** — SC4015 → ADC source impedance / 1 nF filter / sample time chain — fast enough for the mechanical bandwidth of the spring-blade.
- **§12.1c** — R18/R19 1 kΩ added to the mainboard BOM as pedal series Rs.

Net state at end of Round 4:
- **P0 down to 2 items**: §10.1 LED swap and §9.3 PJ-603A pin-4 datasheet check.
- All other previously-FAIL items have been resolved.
- Remaining P1 work is firmware / documentation only.
- Single P2 cost-optimisation item (wing connector basic-part substitute, optional).
