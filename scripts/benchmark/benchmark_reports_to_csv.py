#!/usr/bin/env python3
"""Convert performance_report JSON artifacts under tests_generated/performance to CSV."""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Any, Mapping


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
    for case in meta.get("cases", []):
        cname = _scalar(case.get("name"))
        cdesc = _scalar(case.get("description"))
        wtag = _scalar(case.get("workload_tag"))
        scale = _scalar(case.get("scale"))
        exp = _scalar(case.get("expected_checksum"))
        for impl in case.get("implementations", []):
            summary = impl.get("summary") or {}
            note = impl.get("note")
            rel = impl.get("relative_to_c")
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


def _write_comparison(comp: Mapping[str, Any], out_path: Path) -> None:
    fieldnames = [
        "generated_at_utc",
        "suite_tier",
        "case_name",
        "workload_tag",
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
    for case in comp.get("cases", []):
        rel = case.get("relative_to") or {}
        rows.append(
            {
                "generated_at_utc": gen,
                "suite_tier": tier,
                "case_name": _scalar(case.get("name")),
                "workload_tag": _scalar(case.get("workload_tag")),
                "zr_interp_vs_c": _scalar(rel.get("c")),
                "zr_interp_vs_lua": _scalar(rel.get("lua")),
                "zr_interp_vs_qjs": _scalar(rel.get("qjs")),
                "zr_interp_vs_node": _scalar(rel.get("node")),
                "zr_interp_vs_python": _scalar(rel.get("python")),
                "zr_interp_vs_dotnet": _scalar(rel.get("dotnet")),
                "zr_interp_vs_java": _scalar(rel.get("java")),
                "zr_interp_vs_rust": _scalar(rel.get("rust")),
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
        _write_comparison(comp, comp_out)
        print(f"Wrote {comp_out}")
    else:
        print(f"skip: no {comp_json} (comparison CSV not generated)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
