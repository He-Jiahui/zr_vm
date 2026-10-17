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


# 报告测量契约与 CSV 导出共用字段和合法值，比较映射绑定目标运行时名。
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

    # JSON 数组和对象不可哈希；先核类型，保留诊断并让上层撤销无效比值。
    measurement_scope = implementation.get("measurement_scope")
    if "measurement_scope" in implementation and (
        not isinstance(measurement_scope, str) or measurement_scope not in VALID_MEASUREMENT_SCOPES
    ):
        issues.append(f"{location}: invalid measurement_scope {measurement_scope!r}")

    prepare_scope = implementation.get("prepare_scope")
    if "prepare_scope" in implementation and (
        not isinstance(prepare_scope, str) or prepare_scope not in VALID_PREPARE_SCOPES
    ):
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
    # 汇总只撤销不可信比值，不重新计算；原报告供追溯，副本供查看器消费。
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
    # 比较报告的比例必须能对应唯一基准 case、唯一实现及相同测量范围。
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
    # 缺失或损坏的可选报告仍写入诊断汇总；比例门控使用已读取的基准记录。
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
    # The first marker is the data script; leave the later JS guard literal intact.
    return html_template.replace(marker, b64, 1)


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

# @unit MEASUREMENT_FIELDS
# Summary: 定义两个报告消费者的五字段协议、合法scope集合和八种runtime到实现名称的精确映射。
#
# Covered units: MEASUREMENT_FIELDS; VALID_MEASUREMENT_SCOPES; VALID_PREPARE_SCOPES; COMPARISON_IMPLEMENTATION_NAMES
# Constraints and lifecycle: tuple/set/dict是模块级共享对象；本模块仅查询，不冻结set/dict，也不自动同步另一个消费者。字段存在、字符串合法及三个flag为精确bool是独立检查；名称/数字ratio不在此表验证。
# References: scripts/benchmark/aggregate_benchmark_summary.py:20

# @unit _record_name_is_valid
# Summary: 确认dict记录具有去空白后非空的字符串name。
#
# @param[in] record 任意借用对象；不修改。
# @return bool，缺失/非dict/非法name为False。
# Constraints and lifecycle: 只用strip判空，不修改或trim身份值；Mapping非dict不获接受。
# References: scripts/benchmark/aggregate_benchmark_summary.py:50;scripts/benchmark/aggregate_benchmark_summary.py:54

# @unit _duplicate_record_names
# Summary: 收集list中合法name的重复原字符串。
#
# @param[in] records 借用候选记录集合；非list返回空set。
# @return 新set[str]，精确name重复而非规范化身份。
# Constraints and lifecycle: 只接受list；无效记录忽略；Counter/set新建且不改输入、不排序结果。
# References: scripts/benchmark/aggregate_benchmark_summary.py:58;scripts/benchmark/aggregate_benchmark_summary.py:63

# @unit _read_json_optional
# Summary: 读取一个可选UTF-8 JSON文件并将常见读取/语法失败降为缺失。
#
# @param[in] path 借用Path；文件可缺失。
# @return 任意JSON值或None；并非保证dict。
# Constraints and lifecycle: is_file在try外；文件上下文退出关闭。捕获OSError/JSONDecodeError并写stderr；UnicodeDecodeError及其它异常继续传播。JSON null也成为None，不能与缺失区分；无写入。
# References: scripts/benchmark/aggregate_benchmark_summary.py:69;scripts/benchmark/aggregate_benchmark_summary.py:70;scripts/benchmark/aggregate_benchmark_summary.py:73;scripts/benchmark/aggregate_benchmark_summary.py:75

# @unit _implementation_contract_issue
# Summary: 逐项验证五个测量字段并返回具体字段诊断。
#
# Covered units: _implementation_contract_issue; _implementation_contract_issue/measurement_scope集合检查
# @param[in] implementation 借用任意值。
# @param[in] location 只用于诊断前缀的字符串。
# @return 新list[str]；空表仅表示五字段有效。
# Constraints and lifecycle: 非dict立即一个问题；字符串类型先于set成员检查，数组/对象不会在该入口触发unhashable。缺字段只报missing；三个重用字段用type(v)is bool，1不等同True。此函数不检验status/name/ratio，也不改变对象。
# References: scripts/benchmark/aggregate_benchmark_summary.py:80;scripts/benchmark/aggregate_benchmark_summary.py:84;scripts/benchmark/aggregate_benchmark_summary.py:90;scripts/benchmark/aggregate_benchmark_summary.py:94;scripts/benchmark/aggregate_benchmark_summary.py:102

# @unit _validate_benchmark_measurement_contract
# Summary: 校验benchmark cases/implementations结构、精确身份唯一性及五字段测量契约。
#
# @param[in] report 借用外部JSON值。
# @return 新问题list；不执行ratio撤销。
# Constraints and lifecycle: 非dict或cases非list先拒绝。逐项收集错误、不修复记录；空list没有必需case约束。名称只strip判空，重复判断使用原值；不验证比值数值类型、统计量或环境。
# References: scripts/benchmark/aggregate_benchmark_summary.py:108;scripts/benchmark/aggregate_benchmark_summary.py:112;scripts/benchmark/aggregate_benchmark_summary.py:118;scripts/benchmark/aggregate_benchmark_summary.py:145;scripts/benchmark/aggregate_benchmark_summary.py:151

# @unit _validate_comparison_identity_contract
# Summary: 校验可选comparison报告的case身份与relative_to对象形状。
#
# @param[in] report 借用任意comparison JSON或None。
# @return 新问题list；None返回空表。
# Constraints and lifecycle: None被当作未提供；cases非list先拒绝，记录须dict、name须非空字符串，relative_to须dict。此入口不验证measurement_scope或各ratio数值。输入保持不变。
# References: scripts/benchmark/aggregate_benchmark_summary.py:156;scripts/benchmark/aggregate_benchmark_summary.py:157;scripts/benchmark/aggregate_benchmark_summary.py:159;scripts/benchmark/aggregate_benchmark_summary.py:173;scripts/benchmark/aggregate_benchmark_summary.py:179

# @unit _task3_record_is_comparable
# Summary: 遵守可选Task3稳定性、可比性及门控拒绝标记。
#
# @param[in] record 借用dict。
# @return bool；缺三标记可以为True。
# Constraints and lifecycle: 传入可查询dict；未出现的标记保留旧报告兼容性。出现时stability须STABLE，comparable/gate_eligible须is True；不推断测量有效性或数值范围。
# References: scripts/benchmark/aggregate_benchmark_summary.py:184;scripts/benchmark/aggregate_benchmark_summary.py:185;scripts/benchmark/aggregate_benchmark_summary.py:187;scripts/benchmark/aggregate_benchmark_summary.py:189

# @unit _contract_is_comparable
# Summary: 在两记录五字段有效、PASS且Task3门槛满足时要求相同scope。
#
# @param[in] left 借用任意左实现。
# @param[in] right 借用任意右实现。
# @return bool；两边scope字符串必须相等且非空。
# Constraints and lifecycle: 验证失败短路，不读取未通过形状的后续字段；prepare_scope和三个重用flag各自合法即可，不要求两边相等。仅验证scope/门控，不验证ratio/median/环境。对象不变。
# References: scripts/benchmark/aggregate_benchmark_summary.py:194;scripts/benchmark/aggregate_benchmark_summary.py:195;scripts/benchmark/aggregate_benchmark_summary.py:198;scripts/benchmark/aggregate_benchmark_summary.py:200;scripts/benchmark/aggregate_benchmark_summary.py:202

# @unit _gate_benchmark_relative_to_c
# Summary: 复制benchmark并撤销无法唯一对应合法C基线的已有relative_to_c。
#
# @param[in] benchmark_report 借用可deepcopy的JSON式对象。
# @return (深副本,已撤销非None路径list)；深拷贝异常不捕获。
# Constraints and lifecycle: 首先deepcopy，因此嵌套benchmark结果与输入隔离；只处理dict/cases list及各impl dict，结构坏项由validator另报。case或任一impl身份不合法/重复使整case比值不可信。只将原非None比值设None并记录路径，绝不重算或填回；保留原顺序及其它字段。
# References: scripts/benchmark/aggregate_benchmark_summary.py:205;scripts/benchmark/aggregate_benchmark_summary.py:207;scripts/benchmark/aggregate_benchmark_summary.py:216;scripts/benchmark/aggregate_benchmark_summary.py:225;scripts/benchmark/aggregate_benchmark_summary.py:233;scripts/benchmark/aggregate_benchmark_summary.py:239

# @unit _gate_comparison_ratios
# Summary: 复制comparison并按benchmark唯一case/实现及comparison自身scope撤销不可信比例。
#
# @param[in] benchmark_report 借用benchmark值，只读。
# @param[in] comparison_report 借用可deepcopy值。
# @return (comparison深副本,原非None被清空路径list)。
# Constraints and lifecycle: 只深拷贝comparison，benchmark只读。benchmark重复case排除，重复/坏impl使case拒绝；runtime采用固定映射，未知runtime拒绝。comparison scope须str非空且等于ZR interp scope；不自动补scope、不重算/验证ratio数值。迭代dict快照便于写副本；只对原非None撤销记录路径，保留列表和映射顺序。
# References: scripts/benchmark/aggregate_benchmark_summary.py:243;scripts/benchmark/aggregate_benchmark_summary.py:246;scripts/benchmark/aggregate_benchmark_summary.py:255;scripts/benchmark/aggregate_benchmark_summary.py:280;scripts/benchmark/aggregate_benchmark_summary.py:293;scripts/benchmark/aggregate_benchmark_summary.py:304;scripts/benchmark/aggregate_benchmark_summary.py:311

# @unit _build_summary
# Summary: 读取五份可选JSON，诊断测量协议、在副本上门控并组装schema-2汇总。
#
# @param[in] tests_generated 借用根Path，调用者选定可读目录。
# @param[in] performance_subdir 性能子目录字符串，可被Path绝对路径覆盖规则解释。
# @param[in] baseline_summary 可选基线Path或None。
# @return 新汇总dict，schema_version=2；measurement_contract.valid只反映所收集测量/身份问题；baseline缺失返回INCOMPARABLE记录。
# Constraints and lifecycle: 本函数读文件但不写报告；UTC时间使结果非纯函数。present按固定五名称顺序，只列非None；relative_paths仍列全部五名称。缺benchmark也报测量问题，缺comparison无问题。仅benchmark/comparison深拷贝；其它已读JSON按引用放进结果，mock reader若复用对象则仍共享。performance_subdir只用于Path拼接，绝对路径/..未做containment检查，展示路径的斜线处理不改变实际读取路径。可选baseline调用Task4比较器，可能计算新的baseline speedup，不能将整个汇总称为永不计算比值。BUG: 当 benchmark_report.environment 为 dict 且 status != COMPARABLE 时，scripts/benchmark/aggregate_benchmark_summary.py:356 调用 scripts/benchmark/benchmark_task4_contract.py:55 递归撤销比值；scripts/benchmark/aggregate_benchmark_summary.py:357 却只收集343行先前scope门控返回的路径，使 measurement_contract.ratios_removed 漏记仅由环境撤销的字段；比值已清除，违反 tests/benchmarks/README.md:268 的完整路径记录约定。此为静态路径证据，未运行该反例。
# References: scripts/benchmark/aggregate_benchmark_summary.py:315;scripts/benchmark/aggregate_benchmark_summary.py:332;scripts/benchmark/aggregate_benchmark_summary.py:337;scripts/benchmark/aggregate_benchmark_summary.py:343;scripts/benchmark/aggregate_benchmark_summary.py:352;scripts/benchmark/aggregate_benchmark_summary.py:356;scripts/benchmark/aggregate_benchmark_summary.py:357;scripts/benchmark/aggregate_benchmark_summary.py:366;scripts/benchmark/aggregate_benchmark_summary.py:383;scripts/benchmark/aggregate_benchmark_summary.py:392

# @unit _inject_bundle
# Summary: 把UTF-8汇总JSON编码为base64并替换模板的首个嵌入占位符。
#
# @param[in] html_template 借用HTML模板字符串。
# @param[in] summary 可json.dumps的汇总dict。
# @return 新字符串；缺占位符不返回。
# Constraints and lifecycle: 输入str不变；序列化ensure_ascii=False且紧凑；仅replace(...,1)，后续同字面量保留。缺marker抛ValueError；不可序列化对象等异常传播。此helper无文件写入，模板读写由main负责。
# References: scripts/benchmark/aggregate_benchmark_summary.py:399;scripts/benchmark/aggregate_benchmark_summary.py:400;scripts/benchmark/aggregate_benchmark_summary.py:402;scripts/benchmark/aggregate_benchmark_summary.py:404;scripts/benchmark/aggregate_benchmark_summary.py:406

# @unit main
# Summary: 解析聚合CLI并顺序写汇总、可选viewer JSON与可选bundle HTML。
#
# @return int 0正常完成；根目录或显式模板缺失为1；其它异常/argparse退出不统一转换。
# Constraints and lifecycle: --tests-generated必需且resolve后须目录，否则return1在写前。默认performance输出根下benchmark_suite_summary.json及benchmark_html_viewer.json；自定义subdir改变汇总文件名且不自动写viewer。显式out-json可相对cwd；bundle先resolve，输出父目录mkdir。每个文件直接open(w)/write_text，无临时文件、原子替换或回滚，允许路径碰撞；后续模板缺失return1仍保留已写JSON，IO/序列化/模板错误可传播。out/viewer仅用resolve避免二者同路径；不全面防输入/输出别名。argparse可SystemExit(2)，脚本guard将整数变进程状态。
# References: scripts/benchmark/aggregate_benchmark_summary.py:409;scripts/benchmark/aggregate_benchmark_summary.py:454;scripts/benchmark/aggregate_benchmark_summary.py:459;scripts/benchmark/aggregate_benchmark_summary.py:461;scripts/benchmark/aggregate_benchmark_summary.py:470;scripts/benchmark/aggregate_benchmark_summary.py:479;scripts/benchmark/aggregate_benchmark_summary.py:487;scripts/benchmark/aggregate_benchmark_summary.py:492;scripts/benchmark/aggregate_benchmark_summary.py:500
