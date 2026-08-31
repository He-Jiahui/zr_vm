#!/usr/bin/env python3
"""Finalize benchmark reports and compare schema-3 benchmark summaries."""

from __future__ import annotations

import argparse
import copy
import math
from pathlib import Path
from typing import Any

from benchmark_environment_schema import (
    atomic_write_json,
    compare_environment_contracts,
    strict_json_loads,
    validate_environment_contract,
)


COMPARISON_ALGORITHM = "median_wall_time_ratio_v1"
_NONFATAL_ENVIRONMENT_ISSUES = {"SOURCE_CHANGED_DURING_RUN"}
_RATIO_FIELDS = {
    "overhead_percent",
    "relative_to",
    "relative_to_c",
    "stress_vs_baseline",
}


def _unique(values: list[str]) -> list[str]:
    return list(dict.fromkeys(values))


def _load_json(path: Path) -> Any:
    try:
        return strict_json_loads(path.read_bytes())
    except OSError as exc:
        raise ValueError(f"could not read {path}: {exc}") from exc


def _remove_untrusted_speedups(value: Any) -> None:
    if isinstance(value, dict):
        for key in list(value):
            if "speedup" in key.lower():
                del value[key]
            else:
                _remove_untrusted_speedups(value[key])
    elif isinstance(value, list):
        for item in value:
            _remove_untrusted_speedups(item)


def _null_ratios_and_gates(value: Any) -> None:
    if isinstance(value, dict):
        for key in list(value):
            lowered = key.lower()
            if "speedup" in lowered:
                del value[key]
            elif key == "gate_eligible" or key in _RATIO_FIELDS or lowered.endswith("_ratio"):
                value[key] = None
            else:
                _null_ratios_and_gates(value[key])
    elif isinstance(value, list):
        for item in value:
            _null_ratios_and_gates(item)


def _record_median(record: dict[str, Any]) -> float | None:
    summary = record.get("summary")
    value = summary.get("median_wall_ms") if isinstance(summary, dict) else None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    value = float(value)
    return value if math.isfinite(value) and value > 0.0 else None


def _recompute_benchmark_ratios(report: dict[str, Any], *, comparable: bool) -> None:
    for case in report.get("cases", []):
        if not isinstance(case, dict) or not isinstance(case.get("implementations"), list):
            continue
        implementations = [
            record for record in case["implementations"] if isinstance(record, dict)
        ]
        baseline = next(
            (
                record
                for record in implementations
                if record.get("id") == "c" or record.get("name") == "C"
            ),
            None,
        )
        baseline_median = _record_median(baseline) if baseline is not None else None
        for record in implementations:
            eligible = (
                comparable
                and record.get("status") == "PASS"
                and record.get("stability") == "STABLE"
                and baseline_median is not None
                and _record_median(record) is not None
            )
            if not eligible:
                record["relative_to_c"] = None
                record["gate_eligible"] = None if not comparable else False
                continue
            record["comparable"] = True
            record["gate_eligible"] = True
            record["relative_to_c"] = round(
                _record_median(record) / baseline_median,
                6,
            )


def _environment_attachment(
    environment: Any,
    *,
    report_reference: str,
    profile: bool,
) -> tuple[dict[str, Any], list[str]]:
    issues = validate_environment_contract(environment)
    fatal_issues = [
        issue for issue in issues if issue not in _NONFATAL_ENVIRONMENT_ISSUES
    ]
    if fatal_issues:
        raise ValueError("invalid benchmark environment: " + ", ".join(fatal_issues))

    reasons: list[str] = []
    if environment["isolation"]["status"] != "ISOLATED":
        reasons.append("NON_ISOLATED")
    if environment["source"].get("changed_during_run") is True:
        reasons.append("SOURCE_CHANGED_DURING_RUN")
    if profile:
        reasons.append("PROFILE_NONCOMPARABLE")
    reasons = _unique(reasons)
    return (
        {
            "report_path": report_reference,
            "capture_status": environment["capture_status"],
            "fingerprint": environment["stable_fingerprint"]["value"],
            "fingerprint_algorithm": environment["stable_fingerprint"]["algorithm"],
            "isolation_status": environment["isolation"]["status"],
            "isolation_policy": environment["isolation"]["policy"],
            "selected_cpu": environment["isolation"].get("selected_cpu"),
            "status": "COMPARABLE" if not reasons else "INCOMPARABLE",
            "reasons": reasons,
        },
        reasons,
    )


def attach_environment_contract(
    report: Any,
    environment: Any,
    *,
    report_reference: str = "environment_report.json",
) -> dict[str, Any]:
    if not isinstance(report, dict):
        raise ValueError("benchmark report must be an object")
    if report.get("schema_version") != 3:
        raise ValueError("benchmark report schema_version must be 3")
    policy = report.get("measurement_policy")
    profile = isinstance(policy, dict) and policy.get("profile") is True
    attachment, reasons = _environment_attachment(
        environment,
        report_reference=report_reference,
        profile=profile,
    )
    result = copy.deepcopy(report)
    result["environment"] = attachment
    _remove_untrusted_speedups(result)
    if reasons:
        _null_ratios_and_gates(result)
    else:
        _recompute_benchmark_ratios(result, comparable=True)
    return result


def finalize_report_directory(
    report_directory: str | Path,
    environment_path: str | Path,
) -> dict[str, Any]:
    report_root = Path(report_directory).resolve()
    environment_file = Path(environment_path).resolve()
    benchmark_path = report_root / "benchmark_report.json"
    if not benchmark_path.is_file():
        raise ValueError(f"missing benchmark report: {benchmark_path}")
    environment = _load_json(environment_file)
    try:
        reference = environment_file.relative_to(report_root).as_posix()
    except ValueError:
        reference = str(environment_file)
    benchmark = attach_environment_contract(
        _load_json(benchmark_path),
        environment,
        report_reference=reference,
    )
    atomic_write_json(benchmark_path, benchmark)
    attachment = benchmark["environment"]

    for path in sorted(report_root.glob("*_report.json")):
        if path == benchmark_path or path == environment_file:
            continue
        report = _load_json(path)
        if not isinstance(report, dict):
            raise ValueError(f"report must be an object: {path}")
        report["environment"] = copy.deepcopy(attachment)
        _remove_untrusted_speedups(report)
        if attachment["status"] != "COMPARABLE":
            _null_ratios_and_gates(report)
        elif path.name == "benchmark_report.json":
            _recompute_benchmark_ratios(report, comparable=True)
        atomic_write_json(path, report)
    return attachment


def _summary_report(summary: Any, name: str, side: str, reasons: list[str]) -> Any:
    if not isinstance(summary, dict):
        reasons.append(f"{side}_SUMMARY_INVALID")
        return None
    reports = summary.get("reports")
    if not isinstance(reports, dict):
        reasons.append(f"{side}_REPORTS_MISSING")
        return None
    report = reports.get(name)
    if not isinstance(report, dict):
        suffix = "ENVIRONMENT_MISSING" if name == "environment_report" else "BENCHMARK_REPORT_MISSING"
        reasons.append(f"{side}_{suffix}")
        return None
    return report


def _identity(value: Any) -> str | None:
    if not isinstance(value, dict):
        return None
    for field in ("id", "name"):
        candidate = value.get(field)
        if isinstance(candidate, str) and candidate.strip():
            return candidate.strip()
    return None


def _index_report(
    report: Any,
    side: str,
    reasons: list[str],
) -> dict[tuple[str, str], tuple[dict[str, Any], dict[str, Any]]]:
    if not isinstance(report, dict) or not isinstance(report.get("cases"), list):
        reasons.append(f"{side}_CASES_INVALID")
        return {}
    result: dict[tuple[str, str], tuple[dict[str, Any], dict[str, Any]]] = {}
    case_names: set[str] = set()
    for case in report["cases"]:
        case_name = _identity(case)
        if case_name is None:
            reasons.append(f"{side}_CASE_IDENTITY_INVALID")
            continue
        if case_name in case_names:
            reasons.append(f"{side}_DUPLICATE_CASE")
        case_names.add(case_name)
        implementations = case.get("implementations") if isinstance(case, dict) else None
        if not isinstance(implementations, list):
            reasons.append(f"{side}_IMPLEMENTATIONS_INVALID")
            continue
        implementation_names: set[str] = set()
        for record in implementations:
            implementation_name = _identity(record)
            if implementation_name is None:
                reasons.append(f"{side}_IMPLEMENTATION_IDENTITY_INVALID")
                continue
            if implementation_name in implementation_names:
                reasons.append(f"{side}_DUPLICATE_IMPLEMENTATION")
            implementation_names.add(implementation_name)
            if isinstance(record, dict):
                result[(case_name, implementation_name)] = (case, record)
    return result


def _statistical_algorithm(policy: Any) -> Any:
    if not isinstance(policy, dict):
        return None
    return {
        "calibration_strategy": policy.get("calibration_strategy"),
        "stability": policy.get("stability"),
        "bootstrap": policy.get("bootstrap"),
    }


def _record_reasons(record: dict[str, Any], side: str) -> list[str]:
    reasons: list[str] = []
    if record.get("status") != "PASS":
        reasons.append(f"{side}_RECORD_NOT_PASS")
    if record.get("stability") != "STABLE":
        reasons.append(f"{side}_RECORD_UNSTABLE")
    if record.get("comparable") is not True:
        reasons.append(f"{side}_RECORD_NONCOMPARABLE")
    if record.get("gate_eligible") is not True:
        reasons.append(f"{side}_RECORD_NOT_GATE_ELIGIBLE")
    return reasons


def _median(record: dict[str, Any], side: str, reasons: list[str]) -> float | None:
    summary = record.get("summary")
    value = summary.get("median_wall_ms") if isinstance(summary, dict) else None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        reasons.append(f"{side}_MEDIAN_MISSING")
        return None
    result = float(value)
    if not math.isfinite(result) or result <= 0.0:
        reasons.append(f"{side}_MEDIAN_INVALID")
        return None
    return result


def compare_benchmark_summaries(current: Any, baseline: Any) -> dict[str, Any]:
    reasons: list[str] = []
    current_environment = _summary_report(current, "environment_report", "CURRENT", reasons)
    baseline_environment = _summary_report(baseline, "environment_report", "BASELINE", reasons)
    current_report = _summary_report(current, "benchmark_report", "CURRENT", reasons)
    baseline_report = _summary_report(baseline, "benchmark_report", "BASELINE", reasons)
    if current_environment is not None and baseline_environment is not None:
        reasons.extend(compare_environment_contracts(baseline_environment, current_environment))

    for side, report in (("CURRENT", current_report), ("BASELINE", baseline_report)):
        if isinstance(report, dict):
            policy = report.get("measurement_policy")
            if not isinstance(policy, dict):
                reasons.append(f"{side}_MEASUREMENT_POLICY_MISSING")
            elif policy.get("profile") is True:
                reasons.append(f"{side}_PROFILE_NONCOMPARABLE")

    if isinstance(current_report, dict) and isinstance(baseline_report, dict):
        current_policy = current_report.get("measurement_policy")
        baseline_policy = baseline_report.get("measurement_policy")
        if current_policy != baseline_policy:
            reasons.append("MEASUREMENT_POLICY_MISMATCH")
        if _statistical_algorithm(current_policy) != _statistical_algorithm(baseline_policy):
            reasons.append("STATISTICAL_ALGORITHM_MISMATCH")

    current_records = _index_report(current_report, "CURRENT", reasons)
    baseline_records = _index_report(baseline_report, "BASELINE", reasons)
    if set(current_records) != set(baseline_records):
        reasons.append("BENCHMARK_RECORD_SET_MISMATCH")

    for identity in sorted(set(current_records) & set(baseline_records)):
        current_case, current_record = current_records[identity]
        baseline_case, baseline_record = baseline_records[identity]
        if current_case.get("expected_checksum") != baseline_case.get("expected_checksum"):
            reasons.append("CHECKSUM_CONTRACT_MISMATCH")
        if current_record.get("measurement_scope") != baseline_record.get("measurement_scope"):
            reasons.append("MEASUREMENT_SCOPE_MISMATCH")
        reasons.extend(_record_reasons(current_record, "CURRENT"))
        reasons.extend(_record_reasons(baseline_record, "BASELINE"))

    reasons = _unique(reasons)
    if reasons:
        return {
            "status": "INCOMPARABLE",
            "algorithm": COMPARISON_ALGORITHM,
            "reasons": reasons,
            "records": [],
        }

    records: list[dict[str, Any]] = []
    for case_name, implementation_name in sorted(current_records):
        _, current_record = current_records[(case_name, implementation_name)]
        _, baseline_record = baseline_records[(case_name, implementation_name)]
        median_reasons: list[str] = []
        current_median = _median(current_record, "CURRENT", median_reasons)
        baseline_median = _median(baseline_record, "BASELINE", median_reasons)
        if median_reasons:
            reasons.extend(median_reasons)
            continue
        records.append(
            {
                "case": case_name,
                "implementation": implementation_name,
                "current_median_wall_ms": current_median,
                "baseline_median_wall_ms": baseline_median,
                "baseline_to_current_speedup": baseline_median / current_median,
            }
        )
    reasons = _unique(reasons)
    return {
        "status": "COMPARABLE" if not reasons else "INCOMPARABLE",
        "algorithm": COMPARISON_ALGORITHM,
        "reasons": reasons,
        "records": records if not reasons else [],
    }


def _main_attach(args: argparse.Namespace) -> None:
    attachment = finalize_report_directory(args.report_dir, args.environment)
    print(attachment["status"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    attach_parser = subparsers.add_parser("attach")
    attach_parser.add_argument("--report-dir", required=True)
    attach_parser.add_argument("--environment", required=True)
    attach_parser.set_defaults(handler=_main_attach)
    args = parser.parse_args()
    try:
        args.handler(args)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
