#!/usr/bin/env python3
"""Convert performance_report JSON artifacts under tests_generated/performance to CSV."""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Mapping


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
        isinstance(record, Mapping)
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


def _implementation_contract_valid(implementation: Any) -> bool:
    if not isinstance(implementation, Mapping):
        return False
    if any(field not in implementation for field in MEASUREMENT_FIELDS):
        return False
    if implementation.get("measurement_scope") not in VALID_MEASUREMENT_SCOPES:
        return False
    if implementation.get("prepare_scope") not in VALID_PREPARE_SCOPES:
        return False
    return all(
        type(implementation.get(field)) is bool
        for field in ("runtime_reused", "compiler_reused", "jit_state_reused")
    )


def _task3_record_is_comparable(record: dict[str, Any]) -> bool:
    if "stability" in record and record["stability"] != "STABLE":
        return False
    if "comparable" in record and record["comparable"] is not True:
        return False
    if "gate_eligible" in record and record["gate_eligible"] is not True:
        return False
    return True


def _records_are_comparable(left: Any, right: Any) -> bool:
    if not _implementation_contract_valid(left) or not _implementation_contract_valid(right):
        return False
    if left.get("status") != "PASS" or right.get("status") != "PASS":
        return False
    if not _task3_record_is_comparable(left) or not _task3_record_is_comparable(right):
        return False
    left_scope = left.get("measurement_scope")
    return bool(left_scope) and left_scope == right.get("measurement_scope")


def _implementations_by_name(case: Any) -> dict[str, Mapping[str, Any]]:
    result: dict[str, Mapping[str, Any]] = {}
    if not isinstance(case, Mapping) or not isinstance(case.get("implementations"), list):
        return result
    duplicate_names = _duplicate_record_names(case["implementations"])
    if duplicate_names or any(not _record_name_is_valid(item) for item in case["implementations"]):
        return result
    for implementation in case["implementations"]:
        if _record_name_is_valid(implementation):
            result[implementation["name"]] = implementation
    return result


def _gated_relative_to_c(case: Any, implementation: Any, case_identity_ambiguous: bool) -> Any:
    if case_identity_ambiguous:
        return None
    implementations = _implementations_by_name(case)
    baseline = implementations.get("C")
    if not _records_are_comparable(implementation, baseline):
        return None
    return implementation.get("relative_to_c")


def _gated_comparison_ratio(
    benchmark_case: Any,
    comparison_case: Any,
    runtime_id: str,
    ratio: Any,
    identity_ambiguous: bool,
) -> Any:
    if identity_ambiguous:
        return None
    implementations = _implementations_by_name(benchmark_case)
    interp = implementations.get("ZR interp")
    target_name = COMPARISON_IMPLEMENTATION_NAMES.get(runtime_id)
    target = implementations.get(target_name) if target_name else None
    comparison_scope = comparison_case.get("measurement_scope") if isinstance(comparison_case, Mapping) else None
    if not isinstance(comparison_scope, str) or not comparison_scope:
        return None
    if not _records_are_comparable(interp, target):
        return None
    if comparison_scope != interp.get("measurement_scope"):
        return None
    return ratio


def _bytes_to_mib(value: Any) -> str:
    if value is None or value == "":
        return ""
    try:
        b = float(value)
    except (TypeError, ValueError):
        return ""
    if math.isnan(b):
        return ""
    return f"{b / (1024.0 * 1024.0):.6f}"


def _one_shot_compile_excluded_from_wall_ms(mode: Any) -> str:
    """Suite runs `zr_vm_cli --compile` in prepare; perf_runner times run-only for these modes."""
    m = (mode if isinstance(mode, str) else "").strip().lower()
    if m == "binary":
        return "true"
    return "false"


def _scalar(v: Any) -> str:
    if v is None:
        return ""
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, float):
        if math.isnan(v):
            return ""
        return f"{v:.9g}"
    if isinstance(v, int):
        return str(v)
    return str(v)


def _write_benchmark_timings(report: Mapping[str, Any], out_path: Path) -> None:
    fieldnames = [
        "generated_at_utc",
        "suite_tier",
        "warmup_iterations",
        "measured_iterations",
        "case_name",
        "case_description",
        "workload_tag",
        "case_scale",
        "expected_checksum",
        "implementation_name",
        "language",
        "mode",
        "status",
        "measurement_scope",
        "prepare_scope",
        "runtime_reused",
        "compiler_reused",
        "jit_state_reused",
        "failure_or_skip_note",
        "mean_wall_ms",
        "median_wall_ms",
        "min_wall_ms",
        "max_wall_ms",
        "stddev_wall_ms",
        "mean_peak_mib",
        "max_peak_mib",
        "mean_peak_working_set_bytes",
        "max_peak_working_set_bytes",
        "speed_ratio_vs_c_baseline",
        "one_shot_compile_excluded_from_wall_ms",
    ]
    rows: list[dict[str, str]] = []
    meta = report
    gen = _scalar(meta.get("generated_at_utc"))
    tier = _scalar(meta.get("tier"))
    warmup = _scalar(meta.get("warmup"))
    iters = _scalar(meta.get("iterations"))
    cases = meta.get("cases", [])
    duplicate_case_names = _duplicate_record_names(cases)
    for case in cases:
        cname = _scalar(case.get("name"))
        cdesc = _scalar(case.get("description"))
        wtag = _scalar(case.get("workload_tag"))
        scale = _scalar(case.get("scale"))
        exp = _scalar(case.get("expected_checksum"))
        for impl in case.get("implementations", []):
            summary = impl.get("summary") or {}
            note = impl.get("note")
            rel = _gated_relative_to_c(
                case,
                impl,
                not _record_name_is_valid(case) or case.get("name") in duplicate_case_names,
            )
            if rel is None:
                rel_s = ""
            else:
                rel_s = _scalar(rel)
            row = {
                "generated_at_utc": gen,
                "suite_tier": tier,
                "warmup_iterations": warmup,
                "measured_iterations": iters,
                "case_name": cname,
                "case_description": cdesc,
                "workload_tag": wtag,
                "case_scale": scale,
                "expected_checksum": exp,
                "implementation_name": _scalar(impl.get("name")),
                "language": _scalar(impl.get("language")),
                "mode": _scalar(impl.get("mode")),
                "status": _scalar(impl.get("status")),
                "measurement_scope": _scalar(impl.get("measurement_scope")),
                "prepare_scope": _scalar(impl.get("prepare_scope")),
                "runtime_reused": _scalar(impl.get("runtime_reused")),
                "compiler_reused": _scalar(impl.get("compiler_reused")),
                "jit_state_reused": _scalar(impl.get("jit_state_reused")),
                "failure_or_skip_note": _scalar(note) if note else "",
                "mean_wall_ms": _scalar(summary.get("mean_wall_ms")) if summary else "",
                "median_wall_ms": _scalar(summary.get("median_wall_ms")) if summary else "",
                "min_wall_ms": _scalar(summary.get("min_wall_ms")) if summary else "",
                "max_wall_ms": _scalar(summary.get("max_wall_ms")) if summary else "",
                "stddev_wall_ms": _scalar(summary.get("stddev_wall_ms")) if summary else "",
                "mean_peak_mib": _bytes_to_mib(summary.get("mean_peak_working_set_bytes"))
                if summary
                else "",
                "max_peak_mib": _bytes_to_mib(summary.get("max_peak_working_set_bytes"))
                if summary
                else "",
                "mean_peak_working_set_bytes": _scalar(summary.get("mean_peak_working_set_bytes"))
                if summary
                else "",
                "max_peak_working_set_bytes": _scalar(summary.get("max_peak_working_set_bytes"))
                if summary
                else "",
                "speed_ratio_vs_c_baseline": rel_s,
                "one_shot_compile_excluded_from_wall_ms": _one_shot_compile_excluded_from_wall_ms(
                    impl.get("mode")
                ),
            }
            rows.append(row)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def _write_comparison(comp: Mapping[str, Any], benchmark_report: Mapping[str, Any], out_path: Path) -> None:
    fieldnames = [
        "generated_at_utc",
        "suite_tier",
        "case_name",
        "workload_tag",
        "measurement_scope",
        "zr_interp_vs_c",
        "zr_interp_vs_lua",
        "zr_interp_vs_qjs",
        "zr_interp_vs_node",
        "zr_interp_vs_python",
        "zr_interp_vs_dotnet",
        "zr_interp_vs_java",
        "zr_interp_vs_rust",
    ]
    rows: list[dict[str, str]] = []
    gen = _scalar(comp.get("generated_at_utc"))
    tier = _scalar(comp.get("tier"))
    benchmark_case_records = benchmark_report.get("cases", [])
    duplicate_benchmark_case_names = _duplicate_record_names(benchmark_case_records)
    benchmark_cases = {
        case.get("name"): case
        for case in benchmark_case_records
        if isinstance(case, Mapping)
        and _record_name_is_valid(case)
        and case.get("name") not in duplicate_benchmark_case_names
    }
    comparison_cases = comp.get("cases", [])
    duplicate_comparison_case_names = _duplicate_record_names(comparison_cases)
    for case in comparison_cases:
        rel = case.get("relative_to") or {}
        comparison_case_name_valid = _record_name_is_valid(case)
        benchmark_case = benchmark_cases.get(case.get("name")) if comparison_case_name_valid else None
        identity_ambiguous = (
            not comparison_case_name_valid
            or case.get("name") in duplicate_benchmark_case_names
            or case.get("name") in duplicate_comparison_case_names
            or bool(_duplicate_record_names(benchmark_case.get("implementations", [])))
            or any(
                not _record_name_is_valid(item)
                for item in benchmark_case.get("implementations", [])
            )
            if isinstance(benchmark_case, Mapping)
            else True
        )
        rows.append(
            {
                "generated_at_utc": gen,
                "suite_tier": tier,
                "case_name": _scalar(case.get("name")),
                "workload_tag": _scalar(case.get("workload_tag")),
                "measurement_scope": _scalar(case.get("measurement_scope")),
                "zr_interp_vs_c": _scalar(_gated_comparison_ratio(benchmark_case, case, "c", rel.get("c"), identity_ambiguous)),
                "zr_interp_vs_lua": _scalar(_gated_comparison_ratio(benchmark_case, case, "lua", rel.get("lua"), identity_ambiguous)),
                "zr_interp_vs_qjs": _scalar(_gated_comparison_ratio(benchmark_case, case, "qjs", rel.get("qjs"), identity_ambiguous)),
                "zr_interp_vs_node": _scalar(_gated_comparison_ratio(benchmark_case, case, "node", rel.get("node"), identity_ambiguous)),
                "zr_interp_vs_python": _scalar(_gated_comparison_ratio(benchmark_case, case, "python", rel.get("python"), identity_ambiguous)),
                "zr_interp_vs_dotnet": _scalar(_gated_comparison_ratio(benchmark_case, case, "dotnet", rel.get("dotnet"), identity_ambiguous)),
                "zr_interp_vs_java": _scalar(_gated_comparison_ratio(benchmark_case, case, "java", rel.get("java"), identity_ambiguous)),
                "zr_interp_vs_rust": _scalar(_gated_comparison_ratio(benchmark_case, case, "rust", rel.get("rust"), identity_ambiguous)),
            }
        )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Emit CSV reports from benchmark_report.json and comparison_report.json."
    )
    parser.add_argument(
        "--report-dir",
        type=Path,
        required=True,
        help="Directory containing benchmark_report.json (e.g. build/tests_generated/performance).",
    )
    parser.add_argument(
        "--timings-out",
        type=Path,
        default=None,
        help="Output path for per-implementation timings CSV (default: <report-dir>/benchmark_speed_timings.csv).",
    )
    parser.add_argument(
        "--comparison-out",
        type=Path,
        default=None,
        help="Output path for ZR interp vs languages CSV (default: <report-dir>/zr_interp_vs_languages.csv).",
    )
    args = parser.parse_args()
    report_dir: Path = args.report_dir.resolve()
    bench_json = report_dir / "benchmark_report.json"
    comp_json = report_dir / "comparison_report.json"
    if not bench_json.is_file():
        print(f"error: missing {bench_json}", file=sys.stderr)
        return 1
    timings_out = args.timings_out or (report_dir / "benchmark_speed_timings.csv")
    comp_out = args.comparison_out or (report_dir / "zr_interp_vs_languages.csv")
    with bench_json.open(encoding="utf-8") as handle:
        bench = json.load(handle)
    _write_benchmark_timings(bench, timings_out)
    print(f"Wrote {timings_out}")
    if comp_json.is_file():
        with comp_json.open(encoding="utf-8") as handle:
            comp = json.load(handle)
        _write_comparison(comp, bench, comp_out)
        print(f"Wrote {comp_out}")
    else:
        print(f"skip: no {comp_json} (comparison CSV not generated)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
