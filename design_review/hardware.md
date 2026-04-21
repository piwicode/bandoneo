
## ESD Protection

High speed charging can use 12V on VBUS, and BQ25895 VBUS max is 22V.
SMAJ13A diode protects VBUS clamping at 21.5V < BQ25895 VBUS max of 22V
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

Estimated from the BOMs in [export/](export/) at the 3.3 V rail, with
typical datasheet values at room temperature. Hall sensor count: 32
(left wing) + 38 (right wing) = 70.

| Block | Part | Qty | I typ (mA) | Subtotal (mA) |
|---|---|---:|---:|---:|
| Main MCU, BLE active | STM32WB5MMG | 1 | 10 | 10 |
| Wing MCU @ 72 MHz | STM32F103C8T6 | 2 | 25 | 50 |
| Key position Hall switch | SC4011 | 70 | 2 | 140 |
| Linear Hall (wing L) | HAL403 | 1 | 6 | 6 |
| Analog mux (static) | 74HC4052 | 9 | <0.01 | ~0 |
| Load-cell ADC | CS1237 | 1 | 1.5 | 1.5 |
| LEDs (POW + BT + ~2 status) | — | ~4 | 3 | ~10 |
| LDO Iq + leakage | TLV75533 + misc | — | — | ~1 |
| **Total (playing, BLE streaming)** | | | | **~220** |

At ~220 mA × 3.3 V ≈ **0.73 W** steady-state. Hall sensors dominate
(~64 %); cutting their average draw (duty-cycled power via a
side-channel FET or lower-Iq replacement) is the largest lever.

Peak transient: BLE TX burst (+10 mA on U1) plus all 8 channel LEDs lit
(+25 mA) bumps the rail to ~255 mA briefly. TLV75533 is rated 500 mA,
comfortable margin.

**Battery runtime.** 2200 mAh Li-ion at 3.7 V nominal, LDO efficiency
≈ 3.3 / 3.7 = 89 %: battery draw ≈ 220 mA, runtime ≈ **10 h**, well
above the 2 h requirement. Headroom covers aging and cold-temperature
capacity loss.

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

## Expression pedal: 

M-Audio EX-P and Roland EV-5 have wiper on the TRS tip.

With knob at maximum:
R(Ring-Sleeve) = 65 kΩ constant
R(Tip-Sleeve) = 53.64 kΩ heel, 64 kΩ tip
R(Tip-Ring) = 12.46 kΩ heel, 1 kΩ tip

With knob at minimum:
R(Ring-Sleeve) = 11.47 kΩ constant
R(Tip-Sleeve) = 1 kΩ heel, 12.46 kΩ tip
R(Tip-Ring) = 12.46 kΩ heel, 1 kΩ tip

Changing M-AUDIO mode to "Other" swaps tip and ring.

The port should also accept a sustain pedal with TS jack
which shorts ring and the sleeve. In that case pressing the pedal should be equivalent to pushing the expression pedal tip.

Measuring R(Tip-Ring) ignores all pedal tunning which seems nice.
Using a 4 kΩ pullup results in a voltage variation from .66V to 
2.5V (56% of the range) using a maximum of 0.8mA.

