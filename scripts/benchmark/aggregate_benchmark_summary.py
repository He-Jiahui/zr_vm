#!/usr/bin/env python3
"""Merge benchmark JSON artifacts under <build>/tests_generated into one summary file."""

from __future__ import annotations

import argparse
import base64
import copy
import json
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from benchmark_task4_contract import compare_benchmark_summaries, _null_ratios_and_gates


MEASUREMENT_FIELDS = (
    "measurement_scope",
    "prepare_scope",
    "runtime_reused",
    "compiler_reused",
    "jit_state_reused",
)
VALID_MEASUREMENT_SCOPES = {"process_end_to_end", "persistent_runtime"}
VALID_PREPARE_SCOPES = {
    "none",
    "source_load_compile_in_measurement",
    "bytecode_compile_before_measurement",
    "script_load_in_measurement",
    "runtime_start_jit_in_measurement",
    "script_load_before_measurement",
    "runtime_start_before_measurement",
    "bytecode_compile_and_load_before_measurement",
}
COMPARISON_IMPLEMENTATION_NAMES = {
    "c": "C",
    "lua": "Lua",
    "qjs": "QuickJS",
    "node": "Node.js",
    "python": "Python",
    "dotnet": "C#/.NET",
    "java": "Java",
    "rust": "Rust",
}


def _record_name_is_valid(record: Any) -> bool:
    return (
        isinstance(record, dict)
        and isinstance(record.get("name"), str)
        and bool(record["name"].strip())
    )


def _duplicate_record_names(records: Any) -> set[str]:
    if not isinstance(records, list):
        return set()
    counts = Counter(
        record.get("name")
        for record in records
        if _record_name_is_valid(record)
    )
    return {name for name, count in counts.items() if count > 1}


def _read_json_optional(path: Path) -> Any | None:
    if not path.is_file():
        return None
    try:
        with path.open(encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"warning: could not read {path}: {exc}", file=sys.stderr)
        return None


def _implementation_contract_issue(implementation: Any, location: str) -> list[str]:
    if not isinstance(implementation, dict):
        return [f"{location}: implementation record is not an object"]

    issues: list[str] = []
    for field in MEASUREMENT_FIELDS:
        if field not in implementation:
            issues.append(f"{location}: missing {field}")

    measurement_scope = implementation.get("measurement_scope")
    if "measurement_scope" in implementation and measurement_scope not in VALID_MEASUREMENT_SCOPES:
        issues.append(f"{location}: invalid measurement_scope {measurement_scope!r}")

    prepare_scope = implementation.get("prepare_scope")
    if "prepare_scope" in implementation and prepare_scope not in VALID_PREPARE_SCOPES:
        issues.append(f"{location}: invalid prepare_scope {prepare_scope!r}")

    for field in ("runtime_reused", "compiler_reused", "jit_state_reused"):
        if field in implementation and type(implementation[field]) is not bool:
            issues.append(f"{location}: {field} must be a JSON boolean")
    return issues


def _validate_benchmark_measurement_contract(report: Any) -> list[str]:
    if not isinstance(report, dict):
        return ["benchmark_report: report is not an object"]
    cases = report.get("cases")
    if not isinstance(cases, list):
        return ["benchmark_report: cases must be an array"]

    issues: list[str] = []
    duplicate_case_names = _duplicate_record_names(cases)
    for case_index, case in enumerate(cases):
        location = f"benchmark_report.cases[{case_index}]"
        if not isinstance(case, dict):
            issues.append(f"{location}: case record is not an object")
            continue
        case_name = case.get("name")
        if not _record_name_is_valid(case):
            issues.append(f"{location}: name must be a non-empty string")
        if _record_name_is_valid(case) and case_name in duplicate_case_names:
            issues.append(f"{location}: duplicate benchmark case name {case_name!r}")
        implementations = case.get("implementations")
        if not isinstance(implementations, list):
            issues.append(f"{location}: implementations must be an array")
            continue
        duplicate_implementation_names = _duplicate_record_names(implementations)
        for implementation_index, implementation in enumerate(implementations):
            if isinstance(implementation, dict) and not _record_name_is_valid(implementation):
                issues.append(
                    f"{location}.implementations[{implementation_index}]: "
                    "name must be a non-empty string"
                )
            if (
                isinstance(implementation, dict)
                and _record_name_is_valid(implementation)
                and implementation.get("name") in duplicate_implementation_names
            ):
                issues.append(
                    f"{location}.implementations[{implementation_index}]: "
                    f"duplicate implementation name {implementation.get('name')!r}"
                )
            issues.extend(
                _implementation_contract_issue(
                    implementation,
                    f"{location}.implementations[{implementation_index}]",
                )
            )
    return issues


def _validate_comparison_identity_contract(report: Any) -> list[str]:
    if report is None:
        return []
    if not isinstance(report, dict) or not isinstance(report.get("cases"), list):
        return ["comparison_report: cases must be an array"]

    issues: list[str] = []
    cases = report["cases"]
    duplicate_case_names = _duplicate_record_names(cases)
    for case_index, case in enumerate(cases):
        location = f"comparison_report.cases[{case_index}]"
        if not isinstance(case, dict):
            issues.append(f"{location}: case record is not an object")
            continue
        if not _record_name_is_valid(case):
            issues.append(
                f"{location}: name must be a non-empty string"
            )
        if not isinstance(case.get("relative_to"), dict):
            issues.append(f"{location}: relative_to must be an object")
        if _record_name_is_valid(case) and case.get("name") in duplicate_case_names:
            issues.append(
                f"{location}: "
                f"duplicate comparison case name {case.get('name')!r}"
            )
    return issues


def _task3_record_is_comparable(record: dict[str, Any]) -> bool:
    if "stability" in record and record["stability"] != "STABLE":
        return False
    if "comparable" in record and record["comparable"] is not True:
        return False
    if "gate_eligible" in record and record["gate_eligible"] is not True:
        return False
    return True


def _contract_is_comparable(left: Any, right: Any) -> bool:
    if _implementation_contract_issue(left, "left") or _implementation_contract_issue(right, "right"):
        return False
    if left.get("status") != "PASS" or right.get("status") != "PASS":
        return False
    if not _task3_record_is_comparable(left) or not _task3_record_is_comparable(right):
        return False
    left_scope = left.get("measurement_scope")
    return bool(left_scope) and left_scope == right.get("measurement_scope")


def _gate_benchmark_relative_to_c(benchmark_report: Any) -> tuple[Any, list[str]]:
    gated = copy.deepcopy(benchmark_report)
    removed: list[str] = []
    if not isinstance(gated, dict) or not isinstance(gated.get("cases"), list):
        return gated, removed

    duplicate_case_names = _duplicate_record_names(gated["cases"])
    for case_index, case in enumerate(gated["cases"]):
        if not isinstance(case, dict) or not isinstance(case.get("implementations"), list):
            continue
        duplicate_implementation_names = _duplicate_record_names(case["implementations"])
        identity_ambiguous = (
            not _record_name_is_valid(case)
            or case.get("name") in duplicate_case_names
            or any(not _record_name_is_valid(item) for item in case["implementations"])
            or bool(duplicate_implementation_names)
        )
        baseline = next(
            (
                implementation
                for implementation in case["implementations"]
                if _record_name_is_valid(implementation) and implementation.get("name") == "C"
            ),
            None,
        )
        for implementation_index, implementation in enumerate(case["implementations"]):
            if not isinstance(implementation, dict) or implementation.get("relative_to_c") is None:
                continue
            if identity_ambiguous or not _contract_is_comparable(implementation, baseline):
                implementation["relative_to_c"] = None
                removed.append(
                    "benchmark_report."
                    f"cases[{case_index}].implementations[{implementation_index}].relative_to_c"
                )
    return gated, removed


def _gate_comparison_ratios(benchmark_report: Any, comparison_report: Any) -> tuple[Any, list[str]]:
    gated = copy.deepcopy(comparison_report)
    removed: list[str] = []
    if not isinstance(gated, dict) or not isinstance(gated.get("cases"), list):
        return gated, removed

    benchmark_cases: dict[str, Any] = {}
    duplicate_benchmark_case_names: set[str] = set()
    if isinstance(benchmark_report, dict) and isinstance(benchmark_report.get("cases"), list):
        duplicate_benchmark_case_names = _duplicate_record_names(benchmark_report["cases"])
        for case in benchmark_report["cases"]:
            if (
                isinstance(case, dict)
                and _record_name_is_valid(case)
                and case["name"] not in duplicate_benchmark_case_names
            ):
                benchmark_cases[case["name"]] = case

    duplicate_comparison_case_names = _duplicate_record_names(gated["cases"])
    for case_index, comparison_case in enumerate(gated["cases"]):
        if not isinstance(comparison_case, dict) or not isinstance(comparison_case.get("relative_to"), dict):
            continue
        case_name = comparison_case.get("name")
        comparison_case_name_valid = _record_name_is_valid(comparison_case)
        benchmark_case = benchmark_cases.get(case_name) if comparison_case_name_valid else None
        implementations_by_name: dict[str, Any] = {}
        duplicate_implementation_names: set[str] = set()
        if isinstance(benchmark_case, dict) and isinstance(benchmark_case.get("implementations"), list):
            duplicate_implementation_names = _duplicate_record_names(benchmark_case["implementations"])
            for implementation in benchmark_case["implementations"]:
                if (
                    _record_name_is_valid(implementation)
                    and implementation["name"] not in duplicate_implementation_names
                ):
                    implementations_by_name[implementation["name"]] = implementation

        interp = implementations_by_name.get("ZR interp")
        interp_scope = interp.get("measurement_scope") if isinstance(interp, dict) else None
        comparison_scope = comparison_case.get("measurement_scope")
        comparison_scope_matches = (
            isinstance(comparison_scope, str)
            and bool(comparison_scope)
            and comparison_scope == interp_scope
        )
        identity_ambiguous = (
            not comparison_case_name_valid
            or case_name in duplicate_benchmark_case_names
            or case_name in duplicate_comparison_case_names
            or (
                isinstance(benchmark_case, dict)
                and any(
                    not _record_name_is_valid(item)
                    for item in benchmark_case.get("implementations", [])
                )
            )
            or bool(duplicate_implementation_names)
        )
        for runtime_id, ratio in list(comparison_case["relative_to"].items()):
            target_name = COMPARISON_IMPLEMENTATION_NAMES.get(runtime_id)
            target = implementations_by_name.get(target_name) if target_name else None
            if (
                identity_ambiguous
                or not comparison_scope_matches
                or not _contract_is_comparable(interp, target)
            ):
                if ratio is not None:
                    removed.append(f"comparison_report.cases[{case_index}].relative_to.{runtime_id}")
                comparison_case["relative_to"][runtime_id] = None
    return gated, removed


def _build_summary(
    tests_generated: Path,
    performance_subdir: str,
    baseline_summary: Path | None = None,
) -> dict[str, Any]:
    perf_dir = tests_generated / performance_subdir
    names = (
        "benchmark_report",
        "comparison_report",
        "instruction_report",
        "hotspot_report",
        "environment_report",
    )
    reports: dict[str, Any] = {}
    present: list[str] = []
    for key in names:
        data = _read_json_optional(perf_dir / f"{key}.json")
        if data is not None:
            reports[key] = data
            present.append(key)
    benchmark_report = reports.get("benchmark_report")
    contract_issues = _validate_benchmark_measurement_contract(benchmark_report)
    contract_issues.extend(
        _validate_comparison_identity_contract(reports.get("comparison_report"))
    )
    ratios_removed: list[str] = []
    if "benchmark_report" in reports:
        reports["benchmark_report"], benchmark_ratios_removed = _gate_benchmark_relative_to_c(
            benchmark_report
        )
        benchmark_report = reports["benchmark_report"]
        environment_attachment = (
            benchmark_report.get("environment")
            if isinstance(benchmark_report, dict)
            else None
        )
        if (
            isinstance(environment_attachment, dict)
            and environment_attachment.get("status") != "COMPARABLE"
        ):
            _null_ratios_and_gates(benchmark_report)
        ratios_removed.extend(benchmark_ratios_removed)
    if "comparison_report" in reports:
        reports["comparison_report"], comparison_ratios_removed = _gate_comparison_ratios(
            benchmark_report,
            reports["comparison_report"],
        )
        ratios_removed.extend(comparison_ratios_removed)
    rel = performance_subdir.strip("/").replace("\\", "/")
    summary = {
        "schema_version": 2,
        "aggregated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "tests_generated_dir": str(tests_generated.resolve()),
        "relative_paths": {
            "performance_dir": rel,
            "files": {k: f"{rel}/{k}.json" for k in names},
        },
        "reports": reports,
        "present": present,
        "measurement_contract": {
            "version": 1,
            "valid": not contract_issues,
            "issues": contract_issues,
            "ratios_removed": ratios_removed,
        },
    }
    if baseline_summary is not None:
        baseline = _read_json_optional(baseline_summary)
        if baseline is None:
            summary["baseline_comparison"] = {
                "status": "INCOMPARABLE",
                "algorithm": "median_wall_time_ratio_v1",
                "reasons": ["BASELINE_SUMMARY_MISSING_OR_INVALID"],
                "records": [],
            }
        else:
            summary["baseline_comparison"] = compare_benchmark_summaries(
                summary,
                baseline,
            )
    return summary


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
        "--baseline-summary",
        type=Path,
        default=None,
        help="Optional prior schema-3 summary used for fail-closed median comparison.",
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

    summary = _build_summary(
        tests_generated,
        args.performance_subdir,
        baseline_summary=args.baseline_summary,
    )
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
