"""Unit tests for gen_note_table.py: parsing, inconsistency detection and
table generation for the keyboard layout, driven entirely by in-memory
strings (no filesystem access)."""

import unittest

import gen_note_table as gnt


class ParseIdKeysTest(unittest.TestCase):
    def test_valid(self):
        text = "PRESS 2 3\nPRESS 2 1\nPRESS 2 0\n"
        self.assertEqual(gnt.parse_id_keys(text), ([3, 1, 0], 2))

    def test_ignores_blank_lines(self):
        text = "PRESS 2 3\n\nPRESS 2 1\n"
        self.assertEqual(gnt.parse_id_keys(text), ([3, 1], 2))

    def test_malformed_line(self):
        text = "PRESS 2 3\nnonsense\n"
        with self.assertRaisesRegex(gnt.InputError, "malformed"):
            gnt.parse_id_keys(text)

    def test_inconsistent_wing_id(self):
        text = "PRESS 2 3\nPRESS 1 4\n"
        with self.assertRaisesRegex(gnt.InputError, "wing id"):
            gnt.parse_id_keys(text)

    def test_duplicate_key_id(self):
        text = "PRESS 2 3\nPRESS 2 3\n"
        with self.assertRaisesRegex(gnt.InputError, "duplicate"):
            gnt.parse_id_keys(text)


class ParseNotesTest(unittest.TestCase):
    def test_empty_text_returns_empty(self):
        self.assertEqual(gnt.parse_notes(""), ([], []))

    def test_valid_multi_row(self):
        text = "1\tC4, D4\t2\n2\tE4, F4, G4\t3\n"
        self.assertEqual(gnt.parse_notes(text), (["C4", "D4", "E4", "F4", "G4"], [2, 3]))

    def test_wrong_field_count(self):
        text = "1\tC4, D4\t2\textra\n"
        with self.assertRaisesRegex(gnt.InputError, "3 tab-separated fields"):
            gnt.parse_notes(text)

    def test_count_mismatch(self):
        text = "1\tC4, D4\t3\n"
        with self.assertRaisesRegex(gnt.InputError, "declares count"):
            gnt.parse_notes(text)

    def test_row_number_out_of_sequence(self):
        text = "1\tC4\t1\n3\tD4\t1\n"
        with self.assertRaisesRegex(gnt.InputError, "expected row 2"):
            gnt.parse_notes(text)


class NoteToMacroTest(unittest.TestCase):
    def test_natural(self):
        self.assertEqual(gnt.note_to_macro("C4"), "NOTE(C,4)")

    def test_sharp(self):
        self.assertEqual(gnt.note_to_macro("G♯2"), "NOTE(Gs,2)")

    def test_unrecognized_token(self):
        with self.assertRaisesRegex(gnt.InputError, "unrecognized note"):
            gnt.note_to_macro("H2")

    def test_flat_unsupported(self):
        with self.assertRaisesRegex(gnt.InputError, "unrecognized note"):
            gnt.note_to_macro("G♭2")

    def test_collides_with_note_none(self):
        # NOTE(C,-1) == 0, same as NOTE_NONE.
        with self.assertRaisesRegex(gnt.InputError, "NOTE_NONE"):
            gnt.note_to_macro("C-1")

    def test_out_of_midi_range(self):
        # NOTE(G,10) == 139, out of [0, 127].
        with self.assertRaisesRegex(gnt.InputError, "out of range"):
            gnt.note_to_macro("G10")


class BuildTableFromSourcesTest(unittest.TestCase):
    NUM_KEYS = gnt.NUM_KEYS

    def minimal_sources(self, left_pull="1\tC2, D2\t2\n", right_pull="1\tC4, D4, E4\t3\n",
                        right_push="1\tC4, D4, E4\t3\n"):
        return {
            "id_keys_left": "PRESS 2 2\nPRESS 2 0\nPRESS 2 4\n",
            "id_keys_right": "PRESS 1 0\nPRESS 1 1\nPRESS 1 2\n",
            "notes_left_pull": left_pull,
            "notes_left_push": "",
            "notes_right_pull": right_pull,
            "notes_right_push": right_push,
        }

    def test_unmapped_keys_become_none(self):
        sources = self.minimal_sources()
        warnings = []
        table = gnt.build_table_from_sources(sources, warnings)

        # left, pull: key ids 2, 0 get notes (in id_keys order); key id 4 and
        # the never-referenced ids 1, 3 fall back to NOTE_NONE.
        left_pull = table[gnt.WING_ID["left"]][gnt.BELLOWS["pull"]]
        self.assertEqual(len(left_pull), self.NUM_KEYS)
        self.assertEqual(left_pull[2], "NOTE(C,2)")
        self.assertEqual(left_pull[0], "NOTE(D,2)")
        for k in (1, 3, 4):
            self.assertEqual(left_pull[k], "NOTE_NONE")

        # left, push: empty notes -> everything NOTE_NONE.
        left_push = table[gnt.WING_ID["left"]][gnt.BELLOWS["push"]]
        self.assertTrue(all(c == "NOTE_NONE" for c in left_push))

        # right, pull: all 3 key ids get notes; ids 3, 4 never seen -> NOTE_NONE.
        right_pull = table[gnt.WING_ID["right"]][gnt.BELLOWS["pull"]]
        self.assertEqual(right_pull[:3], ["NOTE(C,4)", "NOTE(D,4)", "NOTE(E,4)"])
        for k in (3, 4):
            self.assertEqual(right_pull[k], "NOTE_NONE")

        self.assertTrue(any("is empty" in w for w in warnings))
        self.assertTrue(any("with no note" in w for w in warnings))
        self.assertTrue(any("not present" in w for w in warnings))

    def test_more_notes_than_keys_is_an_error(self):
        # left has 3 key ids but 4 notes declared.
        sources = self.minimal_sources(left_pull="1\tC2, D2, E2, F2\t4\n")
        with self.assertRaisesRegex(gnt.InputError, "notes but only"):
            gnt.build_table_from_sources(sources, [])

    def test_key_id_exceeding_num_keys_is_an_error(self):
        sources = self.minimal_sources()
        sources["id_keys_left"] = f"PRESS 2 2\nPRESS 2 0\nPRESS 2 {gnt.NUM_KEYS}\n"
        with self.assertRaisesRegex(gnt.InputError, "NUM_KEYS"):
            gnt.build_table_from_sources(sources, [])

    def test_pull_push_row_size_mismatch_is_an_error(self):
        # right pull has one row of 3 notes; right push has one row of 2.
        sources = self.minimal_sources(right_push="1\tC5, D5\t2\n")
        with self.assertRaisesRegex(gnt.InputError, "disagree on notes per row"):
            gnt.build_table_from_sources(sources, [])

    def test_error_message_includes_file_context(self):
        sources = self.minimal_sources(left_pull="1\tC2, H2\t2\n")
        with self.assertRaisesRegex(gnt.InputError, "notes_left_pull.*unrecognized note"):
            gnt.build_table_from_sources(sources, [])


class GenerateHeaderTest(unittest.TestCase):
    def test_balanced_header(self):
        cells_pull = ["NOTE(C,4)"] + ["NOTE_NONE"] * (gnt.NUM_KEYS - 1)
        cells_none = ["NOTE_NONE"] * gnt.NUM_KEYS
        table = {
            gnt.WING_ID["right"]: {gnt.BELLOWS["pull"]: cells_pull, gnt.BELLOWS["push"]: cells_none},
            gnt.WING_ID["left"]: {gnt.BELLOWS["pull"]: cells_none, gnt.BELLOWS["push"]: cells_none},
        }
        header = gnt.generate_header(table)

        self.assertEqual(header.count("{"), header.count("}"))
        self.assertIn("typedef enum { BELLOWS_PULL = 0, BELLOWS_PUSH = 1 } bellows_t;", header)
        self.assertIn(f"#define NUM_KEYS {gnt.NUM_KEYS}", header)
        self.assertIn("note_table[3][2][NUM_KEYS]", header)
        self.assertIn("#define NOTE_NONE 0", header)
        self.assertIn("NOTE(C,4)", header)
        self.assertIn('case 1: return "rheinische_tonlage_142_tones_right";', header)
        self.assertIn('case 2: return "rheinische_tonlage_142_tones_left";', header)


if __name__ == "__main__":
    unittest.main()
