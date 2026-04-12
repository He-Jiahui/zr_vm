#!/usr/bin/env python3
"""Merge benchmark JSON artifacts under <build>/tests_generated into one summary file."""

from __future__ import annotations

import argparse
import base64
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def _read_json_optional(path: Path) -> Any | None:
    if not path.is_file():
        return None
    try:
        with path.open(encoding="utf-8") as handle:
            return json.load(handle)
    except OSError as exc:
        print(f"warning: could not read {path}: {exc}", file=sys.stderr)
        return None


def _build_summary(tests_generated: Path, performance_subdir: str) -> dict[str, Any]:
    perf_dir = tests_generated / performance_subdir
    names = (
        "benchmark_report",
        "comparison_report",
        "instruction_report",
        "hotspot_report",
    )
    reports: dict[str, Any] = {}
    present: list[str] = []
    for key in names:
        data = _read_json_optional(perf_dir / f"{key}.json")
        if data is not None:
            reports[key] = data
            present.append(key)
    rel = performance_subdir.strip("/").replace("\\", "/")
    return {
        "schema_version": 1,
        "aggregated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "tests_generated_dir": str(tests_generated.resolve()),
        "relative_paths": {
            "performance_dir": rel,
            "files": {k: f"{rel}/{k}.json" for k in names},
        },
        "reports": reports,
        "present": present,
    }


def _inject_bundle(html_template: str, summary: dict[str, Any]) -> str:
    raw = json.dumps(summary, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    b64 = base64.b64encode(raw).decode("ascii")
    marker = "__ZR_BENCH_SUMMARY_B64__"
    if marker not in html_template:
        raise ValueError(f"template missing placeholder {marker}")
    return html_template.replace(marker, b64)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Write benchmark_suite_summary.json under tests_generated/."
    )
    parser.add_argument(
        "--tests-generated",
        type=Path,
        required=True,
        help="Path to CMake tests_generated directory (e.g. build/benchmark-gcc-release/tests_generated).",
    )
    parser.add_argument(
        "--performance-subdir",
        default="performance",
        help="Subdirectory under tests_generated containing *._report.json (default: performance).",
    )
    parser.add_argument(
        "--out-json",
        type=Path,
        default=None,
        help="Output JSON path (default: <tests-generated>/benchmark_suite_summary.json).",
    )
    parser.add_argument(
        "--skip-viewer-json",
        action="store_true",
        help="Do not write benchmark_html_viewer.json (default: write when --performance-subdir is performance).",
    )
    parser.add_argument(
        "--bundle-html",
        type=Path,
        default=None,
        help="Optional self-contained HTML path (embeds summary as base64; open in browser without file picker).",
    )
    parser.add_argument(
        "--viewer-template",
        type=Path,
        default=None,
        help="Viewer HTML template (default: benchmark_compare_viewer.html next to this script).",
    )
    args = parser.parse_args()
    tests_generated = args.tests_generated.resolve()
    if not tests_generated.is_dir():
        print(f"error: not a directory: {tests_generated}", file=sys.stderr)
        return 1

    summary = _build_summary(tests_generated, args.performance_subdir)
    default_name = (
        "benchmark_suite_summary.json"
        if args.performance_subdir == "performance"
        else f"benchmark_suite_summary__{args.performance_subdir.replace('/', '_')}.json"
    )
    out_json = args.out_json or (tests_generated / default_name)
    out_json.parent.mkdir(parents=True, exist_ok=True)
    with out_json.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    print(f"Wrote {out_json}")

    if (
        args.performance_subdir == "performance"
        and not args.skip_viewer_json
    ):
        viewer_json = tests_generated / "benchmark_html_viewer.json"
        if viewer_json.resolve() != out_json.resolve():
            with viewer_json.open("w", encoding="utf-8") as handle:
                json.dump(summary, handle, ensure_ascii=False, indent=2)
                handle.write("\n")
            print(f"Wrote {viewer_json} (open with benchmark_compare_viewer.html)")

    if args.bundle_html:
        script_dir = Path(__file__).resolve().parent
        template_path = args.viewer_template or (script_dir / "benchmark_compare_viewer.html")
        if not template_path.is_file():
            print(f"error: viewer template not found: {template_path}", file=sys.stderr)
            return 1
        template = template_path.read_text(encoding="utf-8")
        bundled = _inject_bundle(template, summary)
        bundle_out = args.bundle_html.resolve()
        bundle_out.parent.mkdir(parents=True, exist_ok=True)
        bundle_out.write_text(bundled, encoding="utf-8")
        print(f"Wrote {bundle_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
