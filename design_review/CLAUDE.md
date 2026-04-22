# Context

Recreates the Argentine bandoneon with authentic "Rheinische Lage" 142-key layout. Hall-effect sensors measure key position 1000×/second for precision and responsiveness with zero mechanical wear. A loadcell replaces the acoustic bellows to sense push/pull effort. v1 connectivity: Bluetooth LE-MIDI (wireless) and USB-MIDI (studio). DIN-MIDI and Bluetooth Audio are deferred — see [requirements.md](requirements.md).

# Goal

Building a markdown-based review framework/checklist for electronic design.

This repository describes a hierarchy of decisions:
- [vision.md](vision.md): intent and purpose of the product
- [requirements.md](requirements.md): system-level requirements and 3-board modular rationale
- [architecture.md](architecture.md): MCU, power, and connectivity architecture choices
- [hardware.md](hardware.md): hardware design notes (ESD, protection, part-level decisions)
- BOMs and netlists exported to [export/](export/); component datasheets in [datasheets/](datasheets/)
- [datasheet_summaries.md](datasheet_summaries.md): integration-focused summaries of every datasheet (pinout, externals, protection limits, layout notes) — the reference material for the design review

You are welcome to propose changes at various levels for creative improvement or consistency.