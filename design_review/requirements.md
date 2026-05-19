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
A **spring-blade flexure** measures bellows-replacement effort (push vs. pull): the blade deflects proportionally to the player's effort and two hall sensors read its position. No moving air parts — the goal is to avoid brittle mechanical assemblies. A loadcell was evaluated and rejected: the near-zero compliance of a loadcell removes the proprioceptive feedback that bandoneonists rely on (it plays like pressing on a wall), whereas the spring blade preserves the small, defined travel that lets the player modulate effort by feel. If the spring-blade approach proves impractical, fallback is a pedal-based push/pull mode switch.

### Motherboard
**Role:** Central collection, processing, and I/O.

- **Input:** Key state from both wing boards; push/pull spring-blade hall sensors; pedals.
- **Processing:** Velocity computation, MIDI event generation, threshold tuning.
- **Output:** USB-MIDI.
- **Features:** Threshold configuration, latency optimization, MIDI panic button (sends note-off to all notes for stage emergency reset).
- **Power:** Bus-powered from USB VBUS (5 V, USB 2.0) — no internal battery, no on-board charger. A powered hub between the host phone/computer and a wall charger supplies the instrument (~1.25 W at the VBUS budget) while keeping the host charged. Unplugging the cable powers the instrument off; there is no on/off switch. **Connector choice (USB-A / USB-B / USB-C / Micro-B) is a separate decision driven by mechanical reliability and is not constrained by the electrical design.**

## Peripherals

**Pedal Inputs (two TRS jacks with presence sensing):**
- **Sustain Pedal:** Piano-mode sustain (note release hold).
- **Expression Pedal:** Continuous control (volume, push/pull effort, or other CC mapping). Compatible with M-Audio EX-P / Roland EV-5 (see [hardware.md](hardware.md)).

**Connectivity:**
- **USB-MIDI:** Connect to phone, tablet, or computer for practice, performance, and composition. The host runs the soft-synth; the instrument is a MIDI controller only.
- **Hub workflow (reference setup):** A powered hub at the host combines (a) wall-charger power passthrough to the host, (b) a 3.5 mm headphone jack tied to the host's audio output, and (c) a downstream USB port for the instrument. One cable from the hub to the instrument carries both VBUS power and USB-MIDI; headphones and charger plug into the hub, not the instrument.


**Deferred (not in v1):**
- **DIN 5-pin MIDI:** No clear use case; proper opto-isolation adds parts and board area. Revisit post-release.
- **Wireless (BLE-MIDI):** Removed from v1 in favour of the wired hub workflow above, which makes wireless redundant for the supported use cases (phone practice, DAW studio, stage). Revisit only if a battery-powered standalone mode is added later — that would re-introduce battery, charger, and a wireless module together.
- **On-board audio jack:** Not needed — the hub at the host exposes a headphone jack tied to the host's audio output, which already carries the soft-synth output. Putting an audio DAC on the instrument would duplicate that path.

## User Feedback & Status

- **Power state** (USB connected / not connected) — single LED tied to the 3.3 V rail is sufficient.
- **USB-MIDI enumeration state** (host present, data activity) — second LED driven by the MCU.

Battery, charging, ship-mode, auto-power-off, and BLE pairing indicators are all dropped along with the subsystems they reported on.

## Developer Friendliness

- **Programming Ports:** Each board (wing boards and motherboard) has a dedicated programming port for debugging and firmware updates.

