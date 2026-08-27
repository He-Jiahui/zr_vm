from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


def _repository_root_from_arguments() -> Path:
    if "--repository" not in sys.argv:
        return Path(__file__).resolve().parents[2]
    argument_index = sys.argv.index("--repository")
    try:
        root = Path(sys.argv[argument_index + 1]).resolve()
    except IndexError as error:
        raise SystemExit("--repository requires a path") from error
    del sys.argv[argument_index : argument_index + 2]
    return root


REPOSITORY_ROOT = _repository_root_from_arguments()
SCRIPT_ROOT = REPOSITORY_ROOT / "scripts"
sys.path.insert(0, str(SCRIPT_ROOT))

from syntax_status_records import (  # noqa: E402
    EXPECTED_DIRECTORY_COUNTS,
    collect_syntax_status_records,
    validate_syntax_status_records,
)


class SyntaxStatusRecordTests(unittest.TestCase):
    @staticmethod
    def _write_record(root: Path, relative_path: str, status: str, time: str) -> None:
        path = root / "docs" / "plans" / "syntax" / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            "# Fixture\n\n"
            "## Status\n\n"
            f"- Status: {status}\n"
            f"- Completion time: {time}\n",
            encoding="utf-8",
        )

    def test_selector_uses_numbered_directories_and_excludes_support_records(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self._write_record(root, "01-types/m1.md", "completed", "2026-01-01 01:00 +08:00")
            self._write_record(
                root,
                "02-references/nested/m2.md",
                "completed_with_known_baseline_markers",
                "2026-01-01 02:00 +08:00",
            )
            self._write_record(
                root,
                "01-types/m3-implementation-plan.md",
                "completed",
                "2026-01-01 03:00 +08:00",
            )
            self._write_record(
                root,
                "05-property-unified-ast/m5-task4-property-import-bootstrap.md",
                "completed",
                "2026-01-01 04:00 +08:00",
            )
            self._write_record(root, "support/m4.md", "completed", "2026-01-01 05:00 +08:00")
            self._write_record(root, "top-level.md", "completed", "2026-01-01 06:00 +08:00")

            report = collect_syntax_status_records(root)

        self.assertEqual(
            ("01-types/m1.md", "02-references/nested/m2.md"),
            tuple(record.relative_path for record in report.records),
        )
        self.assertEqual({"01": 1, "02": 1}, report.directory_counts)
        self.assertEqual(2, report.complete_count)
        self.assertEqual(0, len(report.missing_status))
        self.assertEqual(0, len(report.non_complete))
        self.assertEqual(0, len(report.missing_time))

    def test_status_parser_accepts_chinese_and_qualified_completion(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            path = root / "docs" / "plans" / "syntax" / "01-types" / "m1.md"
            path.parent.mkdir(parents=True)
            path.write_text(
                "# Fixture\n\n"
                "- \u5b8c\u6210\u65f6\u95f4\uff1a2026-01-01 01:00 +08:00\n"
                "- \u72b6\u6001\uff1a\u5df2\u5b8c\u6210\uff08M1 gate\uff09\n",
                encoding="utf-8",
            )

            report = collect_syntax_status_records(root)

        self.assertEqual(1, report.complete_count)
        self.assertEqual(0, len(report.missing_status))
        self.assertEqual(0, len(report.non_complete))
        self.assertEqual(0, len(report.missing_time))

    def test_validation_rejects_non_complete_or_drifted_record_sets(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self._write_record(
                root,
                "01-types/m1.md",
                "in_progress",
                "2026-01-01 01:00 +08:00",
            )
            report = collect_syntax_status_records(root)
            issues = validate_syntax_status_records(report)

        self.assertEqual(("01-types/m1.md",), report.non_complete)
        self.assertTrue(any("expected 55 records" in issue for issue in issues))
        self.assertTrue(any("directory distribution differs" in issue for issue in issues))
        self.assertTrue(any("non-complete status" in issue for issue in issues))

    def test_repository_matches_frozen_55_record_contract(self) -> None:
        report = collect_syntax_status_records(REPOSITORY_ROOT)
        issues = validate_syntax_status_records(report)

        self.assertEqual(55, len(report.records))
        self.assertEqual(55, report.complete_count)
        self.assertEqual(EXPECTED_DIRECTORY_COUNTS, report.directory_counts)
        self.assertEqual((), issues)


if __name__ == "__main__":
    unittest.main(verbosity=2)
