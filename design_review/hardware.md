
## ESD Protection

High speed charging can use 12V on VBUS, and BQ25895 VBUS max is 22V.
SMBJ13A diode protects VBUS clamping at 21.5V < BQ25895 VBUS max of 22V
and with Reverse standoff voltage 13V >  12V max charging voltage


D+ and D- are protected with a regular SRV05 with pin 5 floating.
Connecting pin 5 to VCC could make current flow to the voltage regulator
and the MCU which is detrimental. Floating pin 5 forces the current to
flow to GND. This does not prevent the component to protect D+ and D- from high voltage.

## Charging

The board supports 2200 mAh batteries charging at 1C with internal thermistor protection.
Such as [18650CA-1S-3J](https://fr.farnell.com/bak-technology/18650ca-1s-3j/batt-rechargeable-li-ion-3-7v/dp/2401852) with a 10K NTC


Fast charging negociation is done by the MCU and the information is passed to powerline via I2C. The charge D+ and D- are left floating
to avoid interferences with the MCU.

## Power Consumption

Estimated from the BOMs in [export/](export/) at the 3.3 V rail, with typical datasheet values at room temperature. Hall sensor count: **33 (WingLeft) + 38 (WingRight) = 71**.

| Block | Part | Qty | I typ (mA) | Subtotal (mA) |
|---|---|---:|---:|---:|
| Main MCU, BLE active | STM32WB5MMG | 1 | 10 | 10 |
| Wing MCU @ 72 MHz | STM32F103C8T6 | 2 | 25 | 50 |
| Key-position Hall sensor | SC4015 | 71 | 2.5 | 178 |
| Analog mux (static) | 74HC4052 | 9 | <0.01 | ~0 |
| Load-cell ADC | CS1237 | 1 | 1.5 | 1.5 |
| LEDs (POW + BT + ~2 status) | — | ~4 | 3 | ~10 |
| LDO Iq + leakage | TLV755P + misc | — | — | ~1 |
| **Total (playing, BLE streaming)** | | | | **~251** |

At ~251 mA × 3.3 V ≈ **0.83 W** steady-state. Hall sensors dominate (~71 %); duty-cycling their supply via a side-channel FET — or gating banks through the 74HC4052 Ē pins — is the largest available power-saving lever.

Peak transient: BLE TX burst (+10 mA on U1) plus all status LEDs lit (+25 mA) bumps the rail to ~285 mA briefly. TLV755P is rated 500 mA, comfortable margin.

**Battery runtime.** 2200 mAh Li-ion at 3.7 V nominal, LDO efficiency ≈ 3.3 / 3.7 = 89 %: battery draw ≈ 251 mA, runtime ≈ **8.8 h**, well above the 2 h requirement. Headroom covers aging and cold-temperature capacity loss.

**Charge mode** (USB in, MCU idle, wings powered but not sampling):
wing MCUs in STOP + Hall sensors still powered ≈ 150 mA; with wing
rails gated off this drops to ~15 mA (charge-indicator LED + charger
Iq + MCU idle). Worth gating the wing VCC from the motherboard in this
state.

## Wing Power Gating and Back-feed Isolation

Each wing's VCC rail (`L_VCC`, `R_VCC`) is switched on the motherboard by a per-wing TPS2553 current-limited power switch. The main MCU can cut either wing independently — used in charge mode to save ~150 mA per idle wing, and as a fault response if OCP/OVP trips.

**Why back-feed matters.** When a wing rail is cut but the main MCU keeps driving signal lines into the wing connector, every signal line becomes a back-feed path: main drives 3.3 V → trace → wing MCU pin ESD clamp → dead wing VDD. With five bus signals each capable of sourcing ~20 mA, aggregate injection can latch up the wing MCU (STM32 latch-up threshold ≈ 100 mA/pin) and pushes the "off" wing rail to ~2.6 V, defeating the isolation the TPS2553 was meant to provide.

**Isolation strategy — SN74LVC125A on the gated rail (U61 left, U63 right).** One quad tri-state buffer per wing sits at the wing connector on the motherboard, **powered from the gated wing rail** (`L_VCC` / `R_VCC`, downstream of TPS2553). When the wing rail drops, the buffer loses VCC, and its `Ioff` feature forces every input and output to high-Z (<10 µA leakage). No path exists for current to flow main → buffer → wing in either direction. All four OE# pins tie to GND; tri-state is controlled by the VCC side, not by firmware.

Channel assignment:

| Ch | A (input) | Y (output) | Signal / direction |
|---|---|---|---|
| 1 | Main SCK | Wing SCK | main → wing |
| 2 | Main NSS | Wing NSS | main → wing |
| 3 | Main MOSI | Wing MOSI | main → wing |
| 4 | Wing MISO | Main MISO | wing → main |

**Signals that bypass the buffer.**

- **NRST** is driven open-drain by the main MCU through BAT54C to 3.3 V; idle high is provided by the wing MCU's internal NRST pull-up. When the wing rail is cut, the main releases NRST — nothing sources current into the dead wing.
- **READY** is direct-connected (no buffer). The main MCU keeps READY as a passive input with its internal pull-down enabled and no external pull-up. The wing drives actively high to signal ready, low to signal busy. With the wing off the wing pin is high-Z, the pull-down resolves the line to "not ready", and because the main never sources current onto the line it cannot back-power the wing MCU.

**Firmware ordering.** Before deasserting `L_EN` / `R_EN`, configure the READY pin as input with internal pull-down. The buffered SPI signals can be left driven — Ioff handles them. On power-up, wait for the TPS2553 soft-start (~2 ms) before sampling READY or initiating SPI traffic, so the buffer has a stable VCC before passing logic.

**Wing-port ESD steering.** The SRV05-4 arrays at each wing port (TV4/TV5 left, TV2/TV3 right) tie pin 5 to the **gated wing rail**, not to main 3.3 V. This keeps ESD energy in the same power domain as the buffer's output drivers, so transient steering currents don't cross power-domain boundaries.

**Required externals, not yet in the BOM.** Add a 100 nF bypass cap from pin 14 to GND at each buffer (U61, U63), plus a ≥10 µF bulk cap on each TPS2553 output (`L_VCC`, `R_VCC`) close to the wing connector. The TPS2553 datasheet requires output capacitance for stable regulation; the local 100 nF is standard logic IC hygiene.

## Main MCU

The main board module includes filtering capacitors.

## Wing MCU Decoupling (STM32F103C8T6, LQFP48)

Per datasheet Figure 14 (Power supply scheme) and AN2586, the LQFP48
package needs:

- **4 × 100 nF**, one per VDD-type pin: VBAT (1), VDD_1 (24),
  VDD_2 (36), VDD_3 (48). The "5 × 100 nF" figure in the datasheet
  refers to LQFP100, which has an extra VDD pin.
- **1 × 4.7 µF bulk** on VDD_3 (10 µF used, exceeds spec).
- **1 µF + 10 nF** on VDDA (pin 9), referenced to VSSA (pin 8).

Wing design matches this: C1/C2/C4/C5 = 4 × 100 nF, C10 = 10 µF bulk,
C7 = 1 µF and C8 = 10 nF on VDDA. Placement rule: each 100 nF within
a few mm of its VDD pin; VDDA pair adjacent to pins 8/9.

A ferrite bead between VDD and VDDA is an AN2586 enhancement for ADC
noise (not a datasheet requirement). Justified here because PA1 reads
an SC4015 linear Hall directly (bypassing the mux) and the mux outputs
are sampled via ADC.

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

TLV755P input (pin 1, IN) is fed from **VSYS** (BQ25895 pin 15/16), not directly from the battery. This gives two properties:

- Power is sourced from USB or battery through the BQ25895 power-path — the 3.3 V rail is continuous across source transitions.
- The LDO sits **behind** BQ25895's battery protection (OVP, OCP, thermal, BATFET); nothing on the 3.3 V rail can bypass those safeguards.

VSYS(MIN) = 3.5 V guarantees the LDO stays out of dropout at full load (needs ≥ 3.55 V for 3.3 V / 500 mA per datasheet).

## Push/Pull Sensor (CS1237)

The instrument replaces the acoustic bellows with a loadcell that measures the push/pull effort the player applies. The loadcell is a 4-wire Wheatstone bridge; the **CS1237** 24-bit Σ-Δ ADC digitizes the bridge output directly (differential AINP/AINN, internal REFOUT tied to REFIN, PGA ×128). Output rate is configured at 40 or 640 Hz — well above the mechanical bandwidth of breath-like dynamics and far below the 1 kHz key-sampling loop on the wings.

The CS1237 lives on the motherboard, next to the loadcell connector. Its 2-wire interface is bit-banged from the WB5MMG.

## Pedals

Two TRS 6.35 mm jacks on the motherboard, with mechanical presence sensing (switched contact that reports whether a plug is inserted):

- **Sustain port** — expects a TS pedal shorting Tip–Sleeve. Digital input with pull-up; presence sense gates the MIDI sustain event.
- **Expression port** — accepts M-Audio EX-P / Roland EV-5 and compatible TRS pots. The MCU measures **R(Tip-Ring)** via a 4 kΩ pull-up, which ignores per-pedal tip/ring wiring tuning. Voltage varies from ~0.66 V (min) to ~2.5 V (max) — 56 % of the ADC range, drawing ≤ 0.8 mA. The same measurement also accepts a TS sustain pedal plugged into this jack (Tip–Ring short = full deflection), so users who only have a sustain pedal are not stuck.

### Expression pedal reference measurements

M-Audio EX-P and Roland EV-5 have the wiper on the TRS tip.

| Knob | R(Ring-Sleeve) | R(Tip-Sleeve) heel / tip | R(Tip-Ring) heel / tip |
|---|---|---|---|
| max | 65 kΩ | 53.64 / 64 kΩ | 12.46 / 1 kΩ |
| min | 11.47 kΩ | 1 / 12.46 kΩ | 12.46 / 1 kΩ |

Setting M-Audio mode to "Other" swaps tip and ring.

