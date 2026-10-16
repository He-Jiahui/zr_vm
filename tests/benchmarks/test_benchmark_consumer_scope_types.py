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
