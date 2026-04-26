# KB2040 CircuitPython.
# read from STEMA port ADC 24 Bits 4538 NAU7802.
# output midi events on USB port.
#
# Required libraries on CIRCUITPY/lib:
#   cedargrove_nau7802.mpy
#   adafruit_midi/  (folder)
#   adafruit_bus_device/
#
# Connect to serial with: tio /dev/ttyACM1
import board
import busio
import usb_midi
import adafruit_midi
import time

from adafruit_midi.control_change import ControlChange
from adafruit_midi.note_on import NoteOn
from adafruit_midi.note_off import NoteOff
from cedargrove_nau7802 import NAU7802

print(" === Presure to USB midi ===")

i2c = busio.I2C(board.SCL, board.SDA)  # STEMMA QT
nau = NAU7802(i2c, address=0x2A, active_channels=1)
nau.channel = 1
nau.gain = 128
nau.enable(True)

while not nau.available:
    print("Enable to connect to NAU device")
    time.sleep(1)
print("NAU device OK")
nau.read()  # discard first sample

midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=0)
print("USB MIDI OK")

NOTE = 60                 # middle C
ON_THRESHOLD = 50_000     # raw counts — tune for your load cell
OFF_THRESHOLD = 20_000
VEL_SCALE = 500_000       # raw counts mapping to velocity 127

samples = [nau.read() for i in range(20)]
lc_center = sum(samples) / len(samples)
lc_min = min(samples)
lc_max = max(samples)
print("Loadcell center:", lc_center)

lc_center = -9.580e+05
lc_min = -1.2e+06
lc_max = 5.e+04
dead_zone = 0.05
dz_low = lc_center * (1. - dead_zone) + lc_min*dead_zone
dz_high = lc_center * (1. - dead_zone) + lc_max*dead_zone


playing = {}
EXPRESSION_CC = 11

def play(value, start, end, notes):
    if end < start:
        value, start, end = -value, -start, -end
    if value < start:
        # Not playing
        for note in notes:
            if note in playing:
                midi.send(NoteOff(note, 0))
                del playing[note]
                print("stop ", note)
    else:
        intensity = (value - start) / (end - start)
        velocity = min(127,max(1, int(127 * intensity)))
        for note in notes:
            if note not in playing:
               print("play ", note, "vel:", velocity)
               midi.send(NoteOn(note, velocity))
            elif playing[note] != velocity:
               print("play ", note, "cc11:", velocity)
               midi.send(ControlChange(EXPRESSION_CC, velocity)) 
            playing[note] = velocity

while True:
    if not nau.available:
        print("NAU connection lost")
        continue
    raw = nau.read()
    lc_min = min(lc_min, raw)
    lc_max = max(lc_max, raw)
    if raw < lc_center:
        value = (raw - lc_center)/(lc_center - lc_min)
    else:
        value = (raw - lc_center)/(lc_max - lc_center)

    play(raw, dz_low, lc_min, (60,))
    play(raw, dz_high, lc_max, (62,))

    # print("min: %8.3e  center: %8.3e  max: %8.3e  raw: %8.3e  value: %+8.2f" % (lc_min, lc_center, lc_max, raw, value))

    # time.sleep(.01)
   
