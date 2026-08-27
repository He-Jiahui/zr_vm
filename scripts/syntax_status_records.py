from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


EXPECTED_DIRECTORY_COUNTS = {
    "01": 5,
    "02": 6,
    "03": 5,
    "04": 7,
    "05": 6,
    "06": 2,
    "07": 1,
    "10": 5,
    "12": 15,
    "13": 3,
}
EXPECTED_RECORD_COUNT = 55

_EXCLUDED_SUPPORT_RECORD = (
    "05-property-unified-ast/m5-task4-property-import-bootstrap.md"
)
_STATUS_PATTERN = re.compile(
    r"^\s*-\s*(?:Status|\u72b6\u6001)\s*[:\uff1a]\s*(?P<value>.+?)\s*$",
    re.IGNORECASE,
)
_TIME_PATTERN = re.compile(
    r"^\s*-\s*(?:Completion\s+time|Completed\s+at|\u5b8c\u6210\u65f6\u95f4)"
    r"\s*[:\uff1a]\s*(?P<value>.+?)\s*$",
    re.IGNORECASE,
)
_ENGLISH_COMPLETION_PATTERN = re.compile(r"^completed(?:\b|_)", re.IGNORECASE)


@dataclass(frozen=True)
class SyntaxStatusRecord:
    relative_path: str
    directory: str
    status: str | None
    completion_time: str | None

    @property
    def is_complete(self) -> bool:
        if self.status is None:
            return False
        normalized = self.status.strip().strip("`").strip()
        return "\u5df2\u5b8c\u6210" in normalized or _ENGLISH_COMPLETION_PATTERN.match(
            normalized
        ) is not None


@dataclass(frozen=True)
class SyntaxStatusReport:
    records: tuple[SyntaxStatusRecord, ...]
    directory_counts: dict[str, int]
    missing_status: tuple[str, ...]
    non_complete: tuple[str, ...]
    missing_time: tuple[str, ...]

    @property
    def complete_count(self) -> int:
        return sum(record.is_complete for record in self.records)

    def to_json(self) -> str:
        payload = {
            "schemaVersion": 1,
            "total": len(self.records),
            "complete": self.complete_count,
            "missingStatus": list(self.missing_status),
            "nonComplete": list(self.non_complete),
            "missingTime": list(self.missing_time),
            "directoryCounts": self.directory_counts,
            "records": [
                {
                    "path": record.relative_path,
                    "directory": record.directory,
                    "status": record.status,
                    "completionTime": record.completion_time,
                }
                for record in self.records
            ],
        }
        return json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def _selected_markdown_paths(status_root: Path) -> Iterable[Path]:
    if not status_root.is_dir():
        return ()

    selected: list[Path] = []
    for child in sorted(status_root.iterdir(), key=lambda path: path.name):
        if not child.is_dir() or not child.name or not child.name[0].isdigit():
            continue
        for path in child.rglob("*.md"):
            relative_path = path.relative_to(status_root).as_posix()
            if path.name.endswith("-implementation-plan.md"):
                continue
            if relative_path == _EXCLUDED_SUPPORT_RECORD:
                continue
            selected.append(path)
    return sorted(selected, key=lambda path: path.relative_to(status_root).as_posix())


def _first_match(lines: Iterable[str], pattern: re.Pattern[str]) -> str | None:
    for line in lines:
        match = pattern.match(line)
        if match is not None:
            return match.group("value").strip()
    return None


def collect_syntax_status_records(repository_root: Path) -> SyntaxStatusReport:
    status_root = repository_root / "docs" / "plans" / "syntax"
    records: list[SyntaxStatusRecord] = []
    directory_counts: dict[str, int] = {}

    for path in _selected_markdown_paths(status_root):
        relative_path = path.relative_to(status_root).as_posix()
        directory = relative_path.split("/", 1)[0].split("-", 1)[0]
        lines = path.read_text(encoding="utf-8").splitlines()
        record = SyntaxStatusRecord(
            relative_path=relative_path,
            directory=directory,
            status=_first_match(lines, _STATUS_PATTERN),
            completion_time=_first_match(lines, _TIME_PATTERN),
        )
        records.append(record)
        directory_counts[directory] = directory_counts.get(directory, 0) + 1

    missing_status = tuple(
        record.relative_path for record in records if record.status is None
    )
    non_complete = tuple(
        record.relative_path
        for record in records
        if record.status is not None and not record.is_complete
    )
    missing_time = tuple(
        record.relative_path for record in records if record.completion_time is None
    )
    return SyntaxStatusReport(
        records=tuple(records),
        directory_counts=dict(sorted(directory_counts.items())),
        missing_status=missing_status,
        non_complete=non_complete,
        missing_time=missing_time,
    )


def validate_syntax_status_records(report: SyntaxStatusReport) -> tuple[str, ...]:
    issues: list[str] = []
    if len(report.records) != EXPECTED_RECORD_COUNT:
        issues.append(
            f"expected {EXPECTED_RECORD_COUNT} records, found {len(report.records)}"
        )
    if report.directory_counts != EXPECTED_DIRECTORY_COUNTS:
        issues.append(
            "directory distribution differs: "
            f"expected {EXPECTED_DIRECTORY_COUNTS}, found {report.directory_counts}"
        )
    if report.missing_status:
        issues.append("missing status: " + ", ".join(report.missing_status))
    if report.non_complete:
        issues.append("non-complete status: " + ", ".join(report.non_complete))
    if report.missing_time:
        issues.append("missing completion time: " + ", ".join(report.missing_time))
    return tuple(issues)


def _format_text(report: SyntaxStatusReport, issues: tuple[str, ...]) -> str:
    distribution = " ".join(
        f"{directory}={count}"
        for directory, count in report.directory_counts.items()
    )
    lines = [
        f"TOTAL={len(report.records)}",
        f"COMPLETE={report.complete_count}",
        f"MISSING_STATUS={len(report.missing_status)}",
        f"NON_COMPLETE={len(report.non_complete)}",
        f"MISSING_TIME={len(report.missing_time)}",
        distribution,
    ]
    lines.extend(f"ERROR={issue}" for issue in issues)
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Verify the frozen docs/plans/syntax 55-record status set."
    )
    parser.add_argument(
        "--repository",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the parent of scripts/)",
    )
    parser.add_argument("--format", choices=("text", "json"), default="text")
    arguments = parser.parse_args(argv)

    report = collect_syntax_status_records(arguments.repository.resolve())
    issues = validate_syntax_status_records(report)
    if arguments.format == "json":
        sys.stdout.write(report.to_json())
    else:
        sys.stdout.write(_format_text(report, issues))
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
