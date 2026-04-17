# System Architecture

## Design Rationale: 3-Board Modular Architecture

The system is split into three independent boards:
- **Two wing boards** (one per keyboard): Replaceable, independently designed and iterated.
- **One motherboard** (central I/O): Decoupled from keyboard design.

**Benefits:**
- Wing boards can be redesigned (different keyboard layouts, switch types) without touching the motherboard.
- Enables support for multiple keyboard layouts (e.g., Bandoneon vs. Concertina) from the same core electronics.
- Modularity reduces complexity; each board has a focused role.

**Internal Wired Bus:**
- Hardwired connection between wing boards and motherboard tolerates contact issues and mechanical stress.
- Reduces motherboard MCU load; dedicated wing board MCUs handle sampling and local processing.

## Overall Form Factor
Cube enclosure with two vertical keyboards (wing boards) facing outward, mirroring traditional bandoneon geometry.

## Subsystems

### Wing Boards (×2)
**Role:** Keyboard interface and local sampling.

- **Input:** Small form-factor magnetic switches (velocity-sensitive).
  - Enables expressive performance
  - Eable configurable triggering thresholds.
- **Travel:** Linear (vs. pivot). Minor impact on feel vs. acoustic bandoneon.
- **Resistance:** Matched to bandoneon course for familiar playability.
- **Sampling:** Dedicated MCU on each wing board samples switches at 1 kHz.
  - Reduces latency; decouples keyboard capture from I/O processing.
- **Output:** Raw key state data → motherboard via internal bus.

### Air Device: Not Implemented

No air sensor or bellows simulation:
- **Rationale:** Avoid complicated mechanical design and moving parts (which become brittle).
- **Push/Pull Sensing:** Under investigation—may use loadcell to measure push/pull effort (requires experimentation).
- **Alternative:** Pedal-based switch between push and pull modes if loadcell proves impractical.

### Motherboard
**Role:** Central collection, processing, and I/O.

- **Input:** Key state from both wing boards.
- **Processing:** Velocity computation, MIDI event generation, threshold tuning.
- **Output:** 
  - Wireless: Bluetooth LE-MIDI
  - Wired: USB-MIDI
  - Legacy: DIN 5-pin MIDI
- **Features:** Threshold configuration, latency optimization, MIDI panic button (sends note-off to all notes for stage emergency reset).
- **Power:** Rechargeable battery with USB charging. Minimum 2 hours continuous operation on battery. Manual force power-off for debug/reset or user emergency shutdown.

## Peripherals

**Input Devices:**
- **Sustain Pedal:** Play as piano; sustain note release.
- **Volume Pedal:** Control push/pull effort or switch modes.

**Connectivity & Output:**
- **Bluetooth LE-MIDI:** Pair with phone or computer for on-the-go practice and composition.
- **Bluetooth Audio:** Minimal latency audio synthesis to wireless headsets.
- **DIN 5-pin MIDI:** Hardware compatibility (to be confirmed in design constraints).
- **No Audio Jack:** Omitted (not used, adds complexity).

## User Feedback & Status

- **Charging Progress:** User can observe charging progress while connected to USB.
- **Fast Charging:** User knows when fast charging is active.
- **Battery Level:** User can check remaining battery capacity.
- **Power State:** User knows whether device is on or off.
- **Bluetooth Audio Status:** User knows BT audio connection state (pairing, paired, active).
- **Bluetooth MIDI Status:** User knows BT MIDI connection state (pairing, paired, active).
- **Auto Power-Off:** Device automatically turns off after period of inactivity to preserve battery.

## Developer Friendliness

- **Programming Ports:** Each board (wing boards and motherboard) has a dedicated programming port for debugging and firmware updates.

