#!/usr/bin/env python3
"""Malformed JSON scope values must invalidate ratios without aborting consumers."""
from __future__ import annotations

import copy
import csv
import io
import pathlib
import sys
import unittest
from contextlib import nullcontext
from unittest import mock

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "scripts" / "benchmark"))
import aggregate_benchmark_summary as aggregate
import benchmark_reports_to_csv as csv_report


def implementation(name: str = "C", **updates: object) -> dict[str, object]:
    result: dict[str, object] = {
        "name": name, "status": "PASS", "measurement_scope": "persistent_runtime",
        "prepare_scope": "runtime_start_before_measurement", "runtime_reused": True,
        "compiler_reused": False, "jit_state_reused": False, "relative_to_c": 2.5,
    }
    result.update(updates)
    return result


class ScopeTypeTests(unittest.TestCase):
    def malformed_records(self):
        for field in ("measurement_scope", "prepare_scope"):
            for value in ([], {}, ["persistent_runtime"], {"scope": "persistent_runtime"},
                          None, False, 42, "unknown"):
                yield field, value, implementation("ZR interp", **{field: value})

    def test_scope_validators_reject_all_non_string_json_types(self):
        for field, value, record in self.malformed_records():
            with self.subTest(field=field, value=value):
                issues = aggregate._implementation_contract_issue(record, "impl")
                self.assertTrue(any(f"invalid {field}" in issue for issue in issues))
                self.assertFalse(csv_report._implementation_contract_valid(record))

    def test_summary_reports_invalid_scope_and_clears_both_ratio_views(self):
        for field, value, record in self.malformed_records():
            with self.subTest(field=field, value=value):
                benchmark = {"cases": [{"name": "case", "implementations": [implementation(), record]}]}
                comparison = {"cases": [{"name": "case", "measurement_scope": "persistent_runtime",
                                          "relative_to": {"c": 2.5}}]}
                reports = {"benchmark_report.json": benchmark, "comparison_report.json": comparison}
                original = copy.deepcopy(reports)
                with mock.patch.object(aggregate, "_read_json_optional",
                                       side_effect=lambda path: reports.get(path.name)):
                    summary = aggregate._build_summary(REPO_ROOT, "performance")
                self.assertFalse(summary["measurement_contract"]["valid"])
                self.assertIn(field, " ".join(summary["measurement_contract"]["issues"]))
                self.assertIsNone(summary["reports"]["benchmark_report"]["cases"][0]
                                  ["implementations"][1]["relative_to_c"])
                self.assertIsNone(summary["reports"]["comparison_report"]["cases"][0]
                                  ["relative_to"]["c"])
                self.assertEqual(original, reports)

    def test_csv_writers_leave_invalid_ratios_blank(self):
        for field, value, record in self.malformed_records():
            with self.subTest(field=field, value=value):
                benchmark = {"cases": [{"name": "case", "implementations": [implementation(), record]}]}
                comparison = {"cases": [{"name": "case", "measurement_scope": "persistent_runtime",
                                          "relative_to": {"c": 2.5}}]}
                timings = io.StringIO()
                ratios = io.StringIO()
                with mock.patch.object(pathlib.Path, "mkdir"), mock.patch.object(
                        pathlib.Path, "open", return_value=nullcontext(timings)):
                    csv_report._write_benchmark_timings(benchmark, REPO_ROOT / "unused.csv")
                with mock.patch.object(pathlib.Path, "mkdir"), mock.patch.object(
                        pathlib.Path, "open", return_value=nullcontext(ratios)):
                    csv_report._write_comparison(comparison, benchmark, REPO_ROOT / "unused.csv")
                timing_rows = list(csv.DictReader(io.StringIO(timings.getvalue())))
                ratio_rows = list(csv.DictReader(io.StringIO(ratios.getvalue())))
                self.assertEqual("", timing_rows[1]["speed_ratio_vs_c_baseline"])
                self.assertEqual("", ratio_rows[0]["zr_interp_vs_c"])
                self.assertEqual(2.5, record["relative_to_c"])


if __name__ == "__main__":
    unittest.main()

# @unit implementation
# Summary: 创建PASS且五字段有效、ratio=2.5的测试实现记录，再应用调用方覆盖。
#
# @param[in] name 精确实现名，默认C。
# @param[in] updates 任意字段覆盖；嵌套值共享。
# @return 新外层dict[str,object]。
# Constraints and lifecycle: 每次新建外层dict；update是浅合并，传入的list/dict对象仍被借用，并未deepcopy。默认C，malformed generator传ZR interp；只构造夹具，不校验覆盖值也不做I/O。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:20;tests/benchmarks/test_benchmark_consumer_scope_types.py:21;tests/benchmarks/test_benchmark_consumer_scope_types.py:26;tests/benchmarks/test_benchmark_consumer_scope_types.py:35;tests/benchmarks/test_benchmark_consumer_scope_types.py:47;tests/benchmarks/test_benchmark_consumer_scope_types.py:66

# @unit ScopeTypeTests
# Summary: 以三个unittest方法分别检查字段诊断、summary深副本门控和CSV内存输出。
#
# Constraints and lifecycle: 由unittest按test_发现和创建TestCase实例；没有自定义setUp/tearDown。每方法有限16组合，合计48subTest迭代，不是48个注册方法。断言失败由unittest记录，异常/失败的subTest不能代表其后本子例断言执行。上下文patch退出恢复对象；未执行真实CLI与磁盘失败路径。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:30;tests/benchmarks/test_benchmark_consumer_scope_types.py:37;tests/benchmarks/test_benchmark_consumer_scope_types.py:44;tests/benchmarks/test_benchmark_consumer_scope_types.py:63;tests/benchmarks/test_benchmark_consumer_scope_types.py:84;tests/cmake/benchmark-scope-types-tests.cmake:2;tests/cmake/benchmark-scope-types-tests.cmake:4

# @unit ScopeTypeTests.malformed_records
# Summary: 惰性生成两个scope字段各八个有限无效值及相应实现记录。
#
# @param[in] self 借用TestCase实例。
# @return generator依次yield(field,value,record)共16项，非materialized list。
# Constraints and lifecycle: 每次调用得到generator，迭代时才构造夹具；值含空/非空list和dict、None、False、42、unknown字符串。标题不证明所有Python/JSON值都覆盖；未包含所有bool/数值/字符串。每个记录外层新建，value容器按浅更新借用；不修改值。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:31;tests/benchmarks/test_benchmark_consumer_scope_types.py:32;tests/benchmarks/test_benchmark_consumer_scope_types.py:33;tests/benchmarks/test_benchmark_consumer_scope_types.py:35;tests/benchmarks/test_benchmark_consumer_scope_types.py:38;tests/benchmarks/test_benchmark_consumer_scope_types.py:45;tests/benchmarks/test_benchmark_consumer_scope_types.py:64

# @unit ScopeTypeTests.test_scope_validators_reject_all_non_string_json_types
# Summary: 对16个夹具断言聚合诊断含invalid字段、CSV validator返回False。
#
# @param[in,out] self 借用unittest实例；断言记录失败。
# Returns: 正常完成为None；unittest负责断言异常和subTest记账。
# Constraints and lifecycle: first call聚合validator，assertTrue再调用CSV validator并assertFalse；若更早异常则后项未达。只消费generator和内存对象，不运行CLI；不证明任意坏报告结构/比值类型均安全。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:37;tests/benchmarks/test_benchmark_consumer_scope_types.py:38;tests/benchmarks/test_benchmark_consumer_scope_types.py:40;tests/benchmarks/test_benchmark_consumer_scope_types.py:41;tests/benchmarks/test_benchmark_consumer_scope_types.py:42

# @unit ScopeTypeTests.test_summary_reports_invalid_scope_and_clears_both_ratio_views
# Summary: 用mock读取两份内存JSON夹具，检查summary诊断、两种ratio清空及完整输入不变。
#
# @param[in,out] self 借用TestCase。
# Returns: None；断言/异常由unittest处理，patch上下文退出恢复函数。
# Constraints and lifecycle: 每子例合法C+畸形ZR，comparison scope合法且c=2.5；mock _read_json_optional按文件名返回借用reports值。deepcopy原reports后比较，覆盖本夹具全输入不变；没有environment/baseline/真实读取失败场景。_build_summary仍生成UTC与resolve路径元数据；无报告写入。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:44;tests/benchmarks/test_benchmark_consumer_scope_types.py:47;tests/benchmarks/test_benchmark_consumer_scope_types.py:48;tests/benchmarks/test_benchmark_consumer_scope_types.py:51;tests/benchmarks/test_benchmark_consumer_scope_types.py:52;tests/benchmarks/test_benchmark_consumer_scope_types.py:54;tests/benchmarks/test_benchmark_consumer_scope_types.py:55;tests/benchmarks/test_benchmark_consumer_scope_types.py:57;tests/benchmarks/test_benchmark_consumer_scope_types.py:59;tests/benchmarks/test_benchmark_consumer_scope_types.py:61

# @unit ScopeTypeTests.test_summary_reports_invalid_scope_and_clears_both_ratio_views::read_optional_by_name
# Summary: mock reader回调按Path.name选择当前子例的内存JSON。
#
# @param[in] path 被模拟读取的Path；只取name。
# @return reports中对应借用对象或None。
# Constraints and lifecycle: 只在mock.patch.object上下文内作为side_effect调用；闭包借用reports，返回其对象本身，未知名字None。没有读盘/深拷贝；聚合器后续自行复制benchmark/comparison。本夹具baseline_summary=None，A383不达，仅作reader潜在入口参考。context结束恢复原reader；闭包无独立长期资源。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:50;tests/benchmarks/test_benchmark_consumer_scope_types.py:52;tests/benchmarks/test_benchmark_consumer_scope_types.py:53;tests/benchmarks/test_benchmark_consumer_scope_types.py:54;scripts/benchmark/aggregate_benchmark_summary.py:332;scripts/benchmark/aggregate_benchmark_summary.py:383

# @unit ScopeTypeTests.test_csv_writers_leave_invalid_ratios_blank
# Summary: 将两个writer的文件边界换成StringIO并核对有限ratio单元格。
#
# @param[in,out] self 借用TestCase。
# Returns: None；本方法不写真实报告文件。
# Constraints and lifecycle: mock Path.mkdir与Path.open，传入unused.csv只是合成路径；nullcontext保留StringIO供getvalue，真实磁盘无创建。分别读CSV后断言第二计时行ratio与第一comparison c列为空，并原坏记录ratio仍2.5；不构成所有字段深不变或CLI磁盘覆盖证明。patch在各with结束恢复；异常可能跳过后续本子例断言。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:63;tests/benchmarks/test_benchmark_consumer_scope_types.py:66;tests/benchmarks/test_benchmark_consumer_scope_types.py:69;tests/benchmarks/test_benchmark_consumer_scope_types.py:71;tests/benchmarks/test_benchmark_consumer_scope_types.py:73;tests/benchmarks/test_benchmark_consumer_scope_types.py:74;tests/benchmarks/test_benchmark_consumer_scope_types.py:76;tests/benchmarks/test_benchmark_consumer_scope_types.py:77;tests/benchmarks/test_benchmark_consumer_scope_types.py:79;tests/benchmarks/test_benchmark_consumer_scope_types.py:80;tests/benchmarks/test_benchmark_consumer_scope_types.py:81

# @unit module::benchmark_import_path
# Summary: 把repo内benchmark脚本目录置于sys.path首位并导入实际两个消费者。
#
# Constraints and lifecycle: 导入测试模块即修改进程全局sys.path，未自动pop；不得声称import完全无副作用。消费者__main__ guards不触发，因此不写报告；仍导入Task4及其依赖。适用于选定兼容Python和真实repo布局，不是打包隔离保证。
# References: tests/benchmarks/test_benchmark_consumer_scope_types.py:14;tests/benchmarks/test_benchmark_consumer_scope_types.py:15;tests/benchmarks/test_benchmark_consumer_scope_types.py:16;tests/benchmarks/test_benchmark_consumer_scope_types.py:17;tests/benchmarks/test_benchmark_consumer_scope_types.py:84;scripts/benchmark/aggregate_benchmark_summary.py:16;scripts/benchmark/aggregate_benchmark_summary.py:503;scripts/benchmark/benchmark_reports_to_csv.py:413
