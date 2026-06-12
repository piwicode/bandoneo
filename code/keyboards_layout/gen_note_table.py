#!/usr/bin/env python3
"""Generate note_table[wing][bellows][key] from the digitized keyboard layout.

Inputs (in this directory):
  id_keys_<side>.txt        - key press order, top-to-bottom / left-to-right,
                               one "<n>\tPRESS <wing_id> <key_id>" line per key.
  notes_<side>_<bellows>.txt - one row of notes per physical key row, as
                               "<n>\t<row>\t<note>, <note>, ...\t<count>".

<side> is "left" or "right", <bellows> is "pull" or "push".

Output: keyboard_layout.h, a C header defining NOTE_NONE, BELLOWS_PULL/PUSH,
NUM_KEYS and note_table[3][2][NUM_KEYS], in the same layout/style as the
note_table in main-g474/Core/Src/main.c so it can drop in as a replacement.

Each id_keys entry is paired, in order, with the flattened notes for that
side/bellows. Keys left over once notes run out (and key ids never seen in
id_keys at all) are mapped to NOTE_NONE.
"""

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

WING_ID = {"right": 1, "left": 2}
BELLOWS = {"pull": 0, "push": 1}

NUM_KEYS = 40

# Name of the digitized keyboard layout, also the subdirectory holding its
# id_keys_<side>.txt and notes_<side>_<bellows>.txt input files.
LAYOUT_NAME = "rheinische_tonlage_142_tones"

NOTE_OFFSET = {
    "C": 0, "Cs": 1, "D": 2, "Ds": 3, "E": 4, "F": 5,
    "Fs": 6, "G": 7, "Gs": 8, "A": 9, "As": 10, "B": 11,
}

NOTE_TOKEN_RE = re.compile(r"^([A-G])(♯)?(-?\d+)$")
ID_KEYS_LINE_RE = re.compile(r"^PRESS (\d+) (\d+)$")


class InputError(Exception):
    """An inconsistency in the input text files."""


def parse_id_keys(text):
    """Returns (ordered list of key ids top-to-bottom / left-to-right, wing id).

    `text` is the contents of an id_keys_<side>.txt file. All lines must
    reference the same wing id. Raises InputError (without file context; the
    caller adds that) on malformed input or an inconsistent wing id.
    """
    key_ids = []
    seen = set()
    wing_id = None
    for lineno, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        m = ID_KEYS_LINE_RE.match(line)
        if not m:
            raise InputError(f"line {lineno}: malformed line {line!r}")
        line_wing_id, key_id = int(m.group(1)), int(m.group(2))
        if wing_id is None:
            wing_id = line_wing_id
        elif line_wing_id != wing_id:
            raise InputError(f"line {lineno}: wing id {line_wing_id}, expected {wing_id}")
        if key_id in seen:
            raise InputError(f"line {lineno}: duplicate key id {key_id}")
        seen.add(key_id)
        key_ids.append(key_id)
    return key_ids, wing_id


def parse_notes(text):
    """Returns (flattened row-ordered list of note tokens, per-row note counts).

    `text` is the contents of a notes_<side>_<bellows>.txt file (possibly
    empty). Raises InputError (without file context; the caller adds that) on
    malformed input.
    """
    flattened = []
    row_sizes = []
    expected_row = 1
    for lineno, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) != 3:
            raise InputError(f"line {lineno}: expected 3 tab-separated fields, got {len(fields)}")
        row_str, notes_field, count_str = fields
        if not (row_str.isdigit() and count_str.isdigit()):
            raise InputError(f"line {lineno}: malformed line {line!r}")
        if int(row_str) != expected_row:
            raise InputError(f"line {lineno}: expected row {expected_row}, got {row_str}")
        notes = [n.strip() for n in notes_field.split(",")]
        if len(notes) != int(count_str):
            raise InputError(
                f"line {lineno}: row {row_str} lists {len(notes)} notes "
                f"but declares count {count_str}"
            )
        flattened.extend(notes)
        row_sizes.append(len(notes))
        expected_row += 1
    return flattened, row_sizes


def note_to_macro(token):
    """Returns the NOTE(name, octave) macro call for a note token (e.g. "G♯2").

    Raises InputError (without file context; the caller adds that) if the
    token can't be parsed or maps to a MIDI number that's invalid or collides
    with NOTE_NONE.
    """
    m = NOTE_TOKEN_RE.match(token)
    if not m:
        raise InputError(f"unrecognized note {token!r}")
    letter, sharp, octave_str = m.groups()
    name = letter + "s" if sharp else letter
    octave = int(octave_str)
    midi = (octave + 1) * 12 + NOTE_OFFSET[name]
    if midi == 0:
        raise InputError(f"note {token!r} maps to MIDI 0, which collides with NOTE_NONE")
    if midi > 127:
        raise InputError(f"note {token!r} maps to MIDI {midi}, out of range [0, 127]")
    return f"NOTE({name},{octave})"


def build_table_from_sources(sources, warnings):
    """Returns table[wing_id][bellows_idx] = list of num_keys macro strings.

    `sources` maps "id_keys_<side>" and "notes_<side>_<bellows>" to the text
    contents of the corresponding file (empty string if missing/empty).
    `warnings` is a list that informational messages are appended to.

    Every key id across both id_keys_<side> files must be < NUM_KEYS.
    """
    key_ids_by_side = {}
    wing_id_by_side = {}
    for side in WING_ID:
        id_keys_source = f"id_keys_{side}"
        try:
            key_ids_by_side[side], wing_id_by_side[side] = parse_id_keys(sources[id_keys_source])
        except InputError as e:
            raise InputError(f"{id_keys_source}: {e}") from e

    all_key_ids = [k for ids in key_ids_by_side.values() for k in ids]
    if not all_key_ids:
        raise InputError("id_keys_left and id_keys_right are both empty")
    if max(all_key_ids) >= NUM_KEYS:
        raise InputError(f"key id {max(all_key_ids)} >= NUM_KEYS ({NUM_KEYS})")

    table = {}
    for side in WING_ID:
        id_keys_source = f"id_keys_{side}"
        key_ids = key_ids_by_side[side]
        wing_id = wing_id_by_side[side]

        table[wing_id] = {}
        row_sizes_by_bellows = {}
        for bellows, bellows_idx in BELLOWS.items():
            notes_source = f"notes_{side}_{bellows}"
            notes_text = sources[notes_source]
            try:
                notes, row_sizes = parse_notes(notes_text)
            except InputError as e:
                raise InputError(f"{notes_source}: {e}") from e
            row_sizes_by_bellows[bellows] = (notes_source, row_sizes)

            if len(notes) > len(key_ids):
                raise InputError(
                    f"{notes_source}: {len(notes)} notes but only "
                    f"{len(key_ids)} keys in {id_keys_source}"
                )
            if not notes_text.strip():
                warnings.append(f"{notes_source} is empty: all keys map to NOTE_NONE")

            cells = ["NOTE_NONE"] * NUM_KEYS
            for key_id, note in zip(key_ids, notes):
                try:
                    cells[key_id] = note_to_macro(note)
                except InputError as e:
                    raise InputError(f"{notes_source}: {e}") from e

            unmapped = key_ids[len(notes):]
            if unmapped:
                warnings.append(
                    f"{notes_source}: {len(unmapped)} key id(s) with no note: "
                    f"{sorted(unmapped)}"
                )

            never_seen = sorted(set(range(NUM_KEYS)) - set(key_ids))
            if never_seen:
                warnings.append(
                    f"{id_keys_source}: key id(s) not present, mapped to NOTE_NONE: "
                    f"{never_seen}"
                )

            table[wing_id][bellows_idx] = cells

        # Pull and push describe the same physical rows, so when both files
        # have been digitized their per-row note counts must match.
        (pull_source, pull_sizes) = row_sizes_by_bellows["pull"]
        (push_source, push_sizes) = row_sizes_by_bellows["push"]
        if pull_sizes and push_sizes and pull_sizes != push_sizes:
            raise InputError(
                f"{pull_source} and {push_source} disagree on notes per row: "
                f"{pull_sizes} vs {push_sizes}"
            )
    return table


def read_sources(layout_dir):
    """Reads the id_keys_<side>.txt and notes_<side>_<bellows>.txt files from
    `layout_dir` into the `sources` dict expected by build_table_from_sources."""
    sources = {}
    for side in WING_ID:
        sources[f"id_keys_{side}"] = (layout_dir / f"id_keys_{side}.txt").read_text(encoding="utf-8")
        for bellows in BELLOWS:
            path = layout_dir / f"notes_{side}_{bellows}.txt"
            sources[f"notes_{side}_{bellows}"] = path.read_text(encoding="utf-8") if path.exists() else ""
    return sources


def format_cells(cells):
    lines = []
    for i in range(0, len(cells), 5):
        chunk = cells[i:i + 5]
        entries = ", ".join(chunk)
        lines.append(f"      /* {i:2d} */ {entries},")
    return "\n".join(lines)


def generate_header(table):
    out = []
    out.append(f"/* Auto-generated by gen_note_table.py from code/keyboards_layout/{LAYOUT_NAME}/. Do not edit. */")
    out.append("")
    out.append("#ifndef KEYBOARD_LAYOUT_H_")
    out.append("#define KEYBOARD_LAYOUT_H_")
    out.append("")
    out.append("/* MIDI note numbers by name: NOTE(<letter>, <octave>), C4 = 60 (middle C).")
    out.append(" * Append 's' to the letter for sharp, e.g. NOTE(As, 3) = 58.")
    out.append(" * NOTE_NONE marks a key id with no assigned note. */")
    out.append("#define NOTE_NONE 0")
    out.append("#define NOTE_C  0")
    out.append("#define NOTE_Cs 1")
    out.append("#define NOTE_D  2")
    out.append("#define NOTE_Ds 3")
    out.append("#define NOTE_E  4")
    out.append("#define NOTE_F  5")
    out.append("#define NOTE_Fs 6")
    out.append("#define NOTE_G  7")
    out.append("#define NOTE_Gs 8")
    out.append("#define NOTE_A  9")
    out.append("#define NOTE_As 10")
    out.append("#define NOTE_B  11")
    out.append("#define NOTE(name, octave) (((octave) + 1) * 12 + NOTE_##name)")
    out.append("")
    out.append("typedef enum { BELLOWS_PULL = 0, BELLOWS_PUSH = 1 } bellows_t;")
    out.append("")
    out.append(f"#define NUM_KEYS {NUM_KEYS}")
    out.append("")
    out.append(f"static const uint8_t note_table[3][2][NUM_KEYS] =")
    out.append("{")
    side_by_wing_id = {wing_id: side for side, wing_id in WING_ID.items()}
    for wing_id in sorted(table):
        out.append(f"  [{wing_id}] = /* {LAYOUT_NAME}, {side_by_wing_id[wing_id]} */")
        out.append("  {")
        for bellows_idx in BELLOWS.values():
            macro = "BELLOWS_PULL" if bellows_idx == 0 else "BELLOWS_PUSH"
            out.append(f"    [{macro}] =")
            out.append("    {")
            out.append(format_cells(table[wing_id][bellows_idx]))
            out.append("    },")
        out.append("  },")
    out.append("};")
    out.append("")
    out.append("#endif /* KEYBOARD_LAYOUT_H_ */")
    out.append("")
    return "\n".join(out)


def main():
    try:
        warnings = []
        sources = read_sources(HERE / LAYOUT_NAME)
        table = build_table_from_sources(sources, warnings)
    except InputError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    for w in warnings:
        print(f"warning: {w}", file=sys.stderr)

    header = generate_header(table)
    out_dir = HERE.parent / "main-g474" / "keyboard"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "keyboard_layout.h"
    out_path.write_text(header, encoding="utf-8")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
