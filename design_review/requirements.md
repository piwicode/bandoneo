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
  - Enables expressive performance.
  - Allows configurable triggering thresholds.
- **Travel:** Linear (vs. pivot). Minor impact on feel vs. acoustic bandoneon.
- **Resistance:** Matched to bandoneon course for familiar playability.
- **Sampling:** Dedicated MCU on each wing board samples switches at 1 kHz.
  - Reduces latency; decouples keyboard capture from I/O processing.
- **Output:** Raw key state data → motherboard via internal bus.

### Push/Pull Sensing
A loadcell measures bellows-replacement effort (push vs. pull). No moving air parts — the goal is to avoid brittle mechanical assemblies. If the loadcell approach proves impractical, fallback is a pedal-based push/pull mode switch.

### Motherboard
**Role:** Central collection, processing, and I/O.

- **Input:** Key state from both wing boards; push/pull loadcell; pedals.
- **Processing:** Velocity computation, MIDI event generation, threshold tuning.
- **Output:**
  - Wireless: Bluetooth LE-MIDI
  - Wired: USB-MIDI
- **Features:** Threshold configuration, latency optimization, MIDI panic button (sends note-off to all notes for stage emergency reset).
- **Power:** Rechargeable battery with USB charging. Minimum 2 h continuous operation on battery. Manual force power-off for debug/reset or user emergency shutdown.

## Peripherals

**Pedal Inputs (two TRS jacks with presence sensing):**
- **Sustain Pedal:** Piano-mode sustain (note release hold).
- **Expression Pedal:** Continuous control (volume, push/pull effort, or other CC mapping). Compatible with M-Audio EX-P / Roland EV-5 (see [hardware.md](hardware.md)).

**Connectivity:**
- **Bluetooth LE-MIDI:** Pair with phone or computer for practice and composition.
- **USB-MIDI:** Studio / DAW use; also charges the battery.

**Deferred (not in v1):**
- **DIN 5-pin MIDI:** No clear use case; proper opto-isolation adds parts and board area. Revisit post-release.
- **Bluetooth Audio:** STM32WB5MMG lacks classic A2DP. Waiting on STM32WBA6M general availability (expected end of 2026) rather than adding a separate audio frontend.
- **Audio jack:** Not needed if BT-Audio path is added later; redundant with USB-MIDI for now.

## User Feedback & Status

- **Charging progress** while USB is connected.
- **Fast-charge active** indicator.
- **Battery level** readout.
- **Power state** (on / off / charging).
- **BLE-MIDI connection state** (pairing, paired, active).
- **Auto power-off:** after a period of inactivity, the device enters ship mode to preserve battery (see [architecture.md](architecture.md)).

## Developer Friendliness

- **Programming Ports:** Each board (wing boards and motherboard) has a dedicated programming port for debugging and firmware updates.

