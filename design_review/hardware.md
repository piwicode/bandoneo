
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

