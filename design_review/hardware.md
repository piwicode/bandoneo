
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

Fast charging negociation is done by the MCU and the information is passed to powerline via I2C. The charge D+ and D- are left floating
to avoid interferences with the MCU.

## Power Consumption

Estimated from the BOMs in [export/](export/) at the 3.3 V rail, with typical datasheet values at room temperature. Hall sensor count: **33 (WingLeft) + 38 (WingRight) = 71** (of which WingLeft U11–U18 are still SC4011 pending migration; both parts spec the same ICC).

| Block | Part | Qty | I typ (mA) | Subtotal (mA) |
|---|---|---:|---:|---:|
| Main MCU, BLE active | STM32WB5MMG | 1 | 10 | 10 |
| Wing MCU @ 72 MHz | STM32F103C8T6 | 2 | 25 | 50 |
| Key-position Hall sensor | SC4015 / SC4011 | 71 | 2.5 | 178 |
| Linear Hall (WingLeft) | HAL403 | 1 | 6 | 6 |
| Analog mux (static) | 74HC4052 | 9 | <0.01 | ~0 |
| Load-cell ADC | CS1237 | 1 | 1.5 | 1.5 |
| LEDs (POW + BT + ~2 status) | — | ~4 | 3 | ~10 |
| LDO Iq + leakage | TLV755P + misc | — | — | ~1 |
| **Total (playing, BLE streaming)** | | | | **~257** |

At ~257 mA × 3.3 V ≈ **0.85 W** steady-state. Hall sensors dominate (~69 %); duty-cycling their supply via a side-channel FET — or gating banks through the 74HC4052 Ē pins — is the largest available power-saving lever.

Peak transient: BLE TX burst (+10 mA on U1) plus all status LEDs lit (+25 mA) bumps the rail to ~290 mA briefly. TLV755P is rated 500 mA, comfortable margin.

**Battery runtime.** 2200 mAh Li-ion at 3.7 V nominal, LDO efficiency ≈ 3.3 / 3.7 = 89 %: battery draw ≈ 257 mA, runtime ≈ **8.5 h**, well above the 2 h requirement. Headroom covers aging and cold-temperature capacity loss.

**Charge mode** (USB in, MCU idle, wings powered but not sampling):
wing MCUs in STOP + Hall sensors still powered ≈ 150 mA; with wing
rails gated off this drops to ~15 mA (charge-indicator LED + charger
Iq + MCU idle). Worth gating the wing VCC from the motherboard in this
state.

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
the HAL403 linear Hall and the mux outputs are sampled via ADC.

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

