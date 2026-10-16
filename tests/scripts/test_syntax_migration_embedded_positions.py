"""Embedded migration findings must point into the host file."""
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from syntax_migration_inventory import SourceKind, SourcePosition, SourceRange, _scan_path


class EmbeddedPositionTests(unittest.TestCase):
    def scan(self, text):
        with mock.patch.object(Path, "read_text", return_value=text):
            return _scan_path(Path("embedded.c"), file="embedded.c",
                              source_kind=SourceKind.EMBEDDED_ZR_FIXTURE)

    def assert_host_span(self, text, finding, spelling):
        offset = text.index(spelling)
        line = text.count("\n", 0, offset) + 1
        column = offset - text.rfind("\n", 0, offset)
        self.assertEqual(SourceRange(SourcePosition(line, column),
                                     SourcePosition(line, column + len(spelling))),
                         finding.source_range)

    def test_decoded_newline_keeps_both_host_columns(self):
        text = r'const char *source = "%foo\n%await";'
        findings = self.scan(text)
        self.assertEqual(2, len(findings))
        for finding, spelling in zip(findings, ("%foo", "%await")):
            self.assert_host_span(text, finding, spelling)

    def test_decoded_comment_boundary_still_uses_host_coordinates(self):
        text = r'const char *source = "%foo\n// ignore %hidden\n%await";'
        findings = self.scan(text)
        self.assertEqual(2, len(findings))
        self.assert_host_span(text, findings[1], "%await")

    def test_each_adjacent_literal_preserves_its_own_host_range(self):
        text = 'const char *source =\n' + r'  "%foo\n"' + '\n' + r'  "\n%await";'
        findings = self.scan(text)
        self.assertEqual(2, len(findings))
        for finding, spelling in zip(findings, ("%foo", "%await")):
            self.assert_host_span(text, finding, spelling)

    def test_real_newline_and_shortened_escape_map_independently(self):
        text = 'const char *source = `prefix\\n\n\\t%await`;'
        findings = self.scan(text)
        self.assertEqual(1, len(findings))
        self.assert_host_span(text, findings[0], "%await")


if __name__ == "__main__":
    unittest.main()
