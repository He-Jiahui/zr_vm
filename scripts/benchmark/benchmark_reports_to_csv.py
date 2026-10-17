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


# 保留完整测量契约并按运行时名查找基准记录，比例只转录合格输入。
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
    # 先核 JSON 类型再查询合法值，非法容器字段只使比值留空。
    measurement_scope = implementation.get("measurement_scope")
    if not isinstance(measurement_scope, str) or measurement_scope not in VALID_MEASUREMENT_SCOPES:
        return False
    prepare_scope = implementation.get("prepare_scope")
    if not isinstance(prepare_scope, str) or prepare_scope not in VALID_PREPARE_SCOPES:
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
    # 原报告比例只能在实现及 C 基线均满足测量契约时进入 CSV。
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
    # 比较 case 自己声明的 scope 也须与 ZR interp 一致，不能由基准记录代填。
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
    # 保留原测量字段与非比例统计量，只对 C 基线比值执行可信度门控。
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
    # 比较 CSV 的身份关联从基准报告读取；重复或缺失身份使对应比例留空。
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

# @unit MEASUREMENT_FIELDS
# Summary: 定义两个报告消费者的五字段协议、合法scope集合和八种runtime到实现名称的精确映射。
#
# Covered units: MEASUREMENT_FIELDS; VALID_MEASUREMENT_SCOPES; VALID_PREPARE_SCOPES; COMPARISON_IMPLEMENTATION_NAMES
# Constraints and lifecycle: tuple/set/dict是模块级共享对象；本模块仅查询，不冻结set/dict，也不自动同步另一个消费者。字段存在、字符串合法及三个flag为精确bool是独立检查；名称/数字ratio不在此表验证。
# References: scripts/benchmark/benchmark_reports_to_csv.py:17

# @unit _record_name_is_valid
# Summary: 判定Mapping记录是否具有非空字符串name。
#
# @param[in] record 借用任意值。
# @return bool。
# Constraints and lifecycle: 只strip判空，比较/索引仍用原name，不规范化；比聚合器dict检查允许更广Mapping。只读。
# References: scripts/benchmark/benchmark_reports_to_csv.py:47;scripts/benchmark/benchmark_reports_to_csv.py:48;scripts/benchmark/benchmark_reports_to_csv.py:52

# @unit _duplicate_record_names
# Summary: 返回list中有效原name的重复集合。
#
# @param[in] records 借用任意记录集合。
# @return 新set[str]。
# Constraints and lifecycle: 非list返回空set；坏记录忽略；无排序，无输入变更。
# References: scripts/benchmark/benchmark_reports_to_csv.py:55;scripts/benchmark/benchmark_reports_to_csv.py:56;scripts/benchmark/benchmark_reports_to_csv.py:60

# @unit _implementation_contract_valid
# Summary: 以字符串类型先行检查五字段契约，返回有效性。
#
# Covered units: _implementation_contract_valid; _implementation_contract_valid/measurement_scope集合检查
# @param[in] implementation 借用任意值，只读。
# @return bool；无诊断写入。
# Constraints and lifecycle: 输入须Mapping、五字段全部存在，两个scope须str且属于合法集合，三个flag须精确bool。数组/对象scope先False而非set成员TypeError。此入口不验证name/status/统计量/ratio。
# References: scripts/benchmark/benchmark_reports_to_csv.py:66;scripts/benchmark/benchmark_reports_to_csv.py:67;scripts/benchmark/benchmark_reports_to_csv.py:70;scripts/benchmark/benchmark_reports_to_csv.py:74;scripts/benchmark/benchmark_reports_to_csv.py:78;scripts/benchmark/benchmark_reports_to_csv.py:81

# @unit _task3_record_is_comparable
# Summary: 尊重可选Task3三项采样门槛，同时兼容未带标记的旧记录。
#
# @param[in] record 借用Mapping。
# @return bool。
# Constraints and lifecycle: 调用方先保证Mapping；出现的stability须STABLE，其余两项须is True，缺失则不拒绝；不代表数值已验证。
# References: scripts/benchmark/benchmark_reports_to_csv.py:84;scripts/benchmark/benchmark_reports_to_csv.py:85;scripts/benchmark/benchmark_reports_to_csv.py:87;scripts/benchmark/benchmark_reports_to_csv.py:89

# @unit _records_are_comparable
# Summary: 检查两条五字段有效记录的PASS、可选门槛与相同measurement_scope。
#
# @param[in] left 借用任意实现。
# @param[in] right 借用任意实现。
# @return bool，scope须str非空且精确相等。
# Constraints and lifecycle: 验证先行并短路；prepare_scope各自合法即可，不强制相等；reuse旗标也非两边相等条件。只读，不检验原ratio数值或环境报告。
# References: scripts/benchmark/benchmark_reports_to_csv.py:94;scripts/benchmark/benchmark_reports_to_csv.py:95;scripts/benchmark/benchmark_reports_to_csv.py:98;scripts/benchmark/benchmark_reports_to_csv.py:100;scripts/benchmark/benchmark_reports_to_csv.py:102

# @unit _implementations_by_name
# Summary: 仅在整个case实现身份可用且唯一时建立名称索引。
#
# @param[in] case 借用任意case。
# @return 新dict[str,Mapping]；失败空dict。
# Constraints and lifecycle: case须Mapping且implementations须list；任意坏name或重复name使整个索引为空。返回新dict但value借用原Mapping，并非深拷贝/所有权转移；保留原迭代插入顺序。
# References: scripts/benchmark/benchmark_reports_to_csv.py:105;scripts/benchmark/benchmark_reports_to_csv.py:106;scripts/benchmark/benchmark_reports_to_csv.py:110;scripts/benchmark/benchmark_reports_to_csv.py:113;scripts/benchmark/benchmark_reports_to_csv.py:115

# @unit _gated_relative_to_c
# Summary: 将已有relative_to_c仅在本实现与C基线可比时交给CSV。
#
# @param[in] case 借用case对象。
# @param[in] implementation 调用方提供可查询实现Mapping。
# @param[in] case_identity_ambiguous 身份歧义布尔值。
# @return 原ratio对象或None；不是数值转换器。
# Constraints and lifecycle: identity_ambiguous先拒绝；C由唯一实现索引查找。返回原值Any不重算、不检验数字或范围，容许数字字符串；None由_scalar写空。输入不变。
# References: scripts/benchmark/benchmark_reports_to_csv.py:118;scripts/benchmark/benchmark_reports_to_csv.py:119;scripts/benchmark/benchmark_reports_to_csv.py:122;scripts/benchmark/benchmark_reports_to_csv.py:126

# @unit _gated_comparison_ratio
# Summary: 按benchmark的ZR interp/目标实现与comparison自有scope筛选一个既有ratio。
#
# @param[in] benchmark_case 借用benchmark case。
# @param[in] comparison_case 借用comparison case。
# @param[in] runtime_id 固定映射键字符串。
# @param[in] ratio 借用输入值，可能任意类型。
# @param[in] identity_ambiguous 身份歧义标记。
# @return 原ratio或None。
# Constraints and lifecycle: identity_ambiguous先False路径；runtime用固定映射，未知值无目标。comparison scope须Mapping中str非空且等于interp，两个benchmark记录须可比。原ratio原样返回，不补scope、不计算或检验数值；不修改输入。
# References: scripts/benchmark/benchmark_reports_to_csv.py:129;scripts/benchmark/benchmark_reports_to_csv.py:136;scripts/benchmark/benchmark_reports_to_csv.py:140;scripts/benchmark/benchmark_reports_to_csv.py:145;scripts/benchmark/benchmark_reports_to_csv.py:150

# @unit _bytes_to_mib
# Summary: 把可float转换的字节数格式化为六位小数MiB文本。
#
# @param[in] value 借用任意原指标。
# @return str；失败为空串。
# Constraints and lifecycle: None/空串/TypeError/ValueError/NaN为空；负数、bool、数值字符串及无穷值未被统一拒绝。float转换OverflowError不在捕获范围。本函数不验证指标真实性，不能把返回非空视为有效内存值。
# References: scripts/benchmark/benchmark_reports_to_csv.py:153;scripts/benchmark/benchmark_reports_to_csv.py:154;scripts/benchmark/benchmark_reports_to_csv.py:157;scripts/benchmark/benchmark_reports_to_csv.py:158;scripts/benchmark/benchmark_reports_to_csv.py:160;scripts/benchmark/benchmark_reports_to_csv.py:162

# @unit _one_shot_compile_excluded_from_wall_ms
# Summary: 输出binary模式兼容列的true/false字符串。
#
# @param[in] mode 借用任意模式值。
# @return 小写字符串true或false。
# Constraints and lifecycle: 仅对str输入执行mode.strip().lower()匹配binary；非str输入使用空串，返回字符串false且不调用str(mode)；该列描述既定模式口径，不读取实际编译时间，也不证明某条记录五字段测量契约。保留原docstring。
# References: scripts/benchmark/benchmark_reports_to_csv.py:165;scripts/benchmark/benchmark_reports_to_csv.py:166;scripts/benchmark/benchmark_reports_to_csv.py:170

# @unit _scalar
# Summary: 将输入转换为CSV单元格文本。
#
# @param[in] v 借用任意值。
# @return str；自定义__str__异常可传播。
# Constraints and lifecycle: None/float NaN为空，bool先于int转小写，float用9g，其余str(v)。列表/字典也会得到Python文本，不是JSON验证/编码，不执行公式转义或有限数检查；csv.DictWriter另负责CSV引号。
# References: scripts/benchmark/benchmark_reports_to_csv.py:173;scripts/benchmark/benchmark_reports_to_csv.py:174;scripts/benchmark/benchmark_reports_to_csv.py:176;scripts/benchmark/benchmark_reports_to_csv.py:178;scripts/benchmark/benchmark_reports_to_csv.py:182;scripts/benchmark/benchmark_reports_to_csv.py:184

# @unit _write_benchmark_timings
# Summary: 先构造计时CSV行，再直接写固定列顺序的UTF-8文件。
#
# @param[in] report 借用结构符合writer消费前提的Mapping。
# @param[in] out_path 借用Path，指定可写输出位置；可能创建父目录和覆写文件。
# Returns: None；文件写副作用，IO/结构/转换异常传播。
# Constraints and lifecycle: 调用方须提供可遍历cases、case/impl Mapping及合适summary等报告结构；仅比例可信门控，不是通用坏JSON容错器。保持case/impl输入顺序，不排序；测量字段/统计转录，ratio只筛选后_scalar。输入只读；行转换先于mkdir/open，但文件open(w)截断后IO失败仍可能部分写，目录及旧输出无回滚；上下文退出关闭真实文件，测试用nullcontext替代不关闭StringIO。
# References: scripts/benchmark/benchmark_reports_to_csv.py:187;scripts/benchmark/benchmark_reports_to_csv.py:225;scripts/benchmark/benchmark_reports_to_csv.py:231;scripts/benchmark/benchmark_reports_to_csv.py:237;scripts/benchmark/benchmark_reports_to_csv.py:284;scripts/benchmark/benchmark_reports_to_csv.py:290;scripts/benchmark/benchmark_reports_to_csv.py:291;scripts/benchmark/benchmark_reports_to_csv.py:294

# @unit _write_comparison
# Summary: 建立唯一benchmark case索引，按comparison原顺序写八种runtime比值列。
#
# @param[in] comp 借用comparison报告Mapping。
# @param[in] benchmark_report 借用benchmark报告Mapping。
# @param[in] out_path 借用可写输出Path，文件及目录副作用。
# Returns: None；IO或结构异常传播，真实文件上下文清理。
# Constraints and lifecycle: 输入须适合writer迭代/Mapping操作，relative_to须可get对象；坏身份门控不等同所有结构异常都被捕获。无benchmark或歧义/坏scope时对应比例为空；固定八列，未支持runtime不导出。所有行转换完才写，直接mkdir/open(w)，失败可能留下文件；不修改借用对象、不重算ratio、不排序。
# References: scripts/benchmark/benchmark_reports_to_csv.py:297;scripts/benchmark/benchmark_reports_to_csv.py:317;scripts/benchmark/benchmark_reports_to_csv.py:319;scripts/benchmark/benchmark_reports_to_csv.py:329;scripts/benchmark/benchmark_reports_to_csv.py:332;scripts/benchmark/benchmark_reports_to_csv.py:351;scripts/benchmark/benchmark_reports_to_csv.py:361;scripts/benchmark/benchmark_reports_to_csv.py:362;scripts/benchmark/benchmark_reports_to_csv.py:365

# @unit main
# Summary: 读取report-dir并顺序输出计时及可选comparison CSV。
#
# @return int；正常/缺可选comparison为0，缺必需benchmark文件为1，其它异常传播。
# Constraints and lifecycle: report-dir resolve，benchmark_report.json不存在则stderr/1在输出前；默认输出同目录benchmark_speed_timings.csv与zr_interp_vs_languages.csv，显式路径可相对cwd且不全面检查别名。JSON/open/结构异常不捕获。计时写完才读取comparison，后者缺失仅warn并0，既有comparison CSV不删除；后者损坏/写失败时计时文件已保留。无跨文件事务/原子发布。脚本guard负责整数状态，argparse自身可SystemExit(2)。
# References: scripts/benchmark/benchmark_reports_to_csv.py:368;scripts/benchmark/benchmark_reports_to_csv.py:391;scripts/benchmark/benchmark_reports_to_csv.py:397;scripts/benchmark/benchmark_reports_to_csv.py:401;scripts/benchmark/benchmark_reports_to_csv.py:404;scripts/benchmark/benchmark_reports_to_csv.py:408;scripts/benchmark/benchmark_reports_to_csv.py:410
