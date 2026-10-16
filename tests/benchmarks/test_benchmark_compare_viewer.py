"""Browser-boundary regressions for the actual HTML and bundle injector.

Run with Python -B; Node built-ins suffice. BENCHMARK_VIEWER_NODE can pin Node.
"""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
BENCHMARK_SCRIPTS = ROOT / "scripts" / "benchmark"
sys.path.insert(0, str(BENCHMARK_SCRIPTS))
spec = importlib.util.spec_from_file_location(
    "viewer_actual_aggregate", BENCHMARK_SCRIPTS / "aggregate_benchmark_summary.py"
)
aggregate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(aggregate)
VIEWER = BENCHMARK_SCRIPTS / "benchmark_compare_viewer.html"
HARNESS = Path(__file__).with_name("benchmark_compare_viewer_harness.js")


def benchmark(case="viewer-case", names=("C", "Python")):
    return {"suite": "performance_report", "generated_at_utc": "legacy-time", "cases": [
        {"name": case, "workload_tag": "viewer-workload", "implementations": [
            {"name": name, "status": "PASS", "summary": {"mean_wall_ms": i + 1}}
            for i, name in enumerate(names)]}]}


def comparison(case="comparison-case"):
    return {"suite": "comparison_report", "cases": [
        {"name": case, "relative_to": {"c": 2.5, "dotnet": 1.25}}]}


def summary(**reports):
    return {"schema_version": 3, "reports": reports, "present": list(reports),
            "aggregated_at_utc": "2026-10-16T00:00:00Z", "tests_generated_dir": "viewer-report"}


class BenchmarkCompareViewerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node = os.environ.get("BENCHMARK_VIEWER_NODE") or shutil.which("node")
        if not cls.node:
            raise RuntimeError("Node is required; set BENCHMARK_VIEWER_NODE to its executable")

    def page(self, payloads=(), bundle=None):
        html = VIEWER.read_text(encoding="utf-8")
        if bundle is not None:
            html = aggregate._inject_bundle(html, bundle)
        result = subprocess.run(
            [self.node, str(HARNESS)],
            input=json.dumps({"html": html, "files": [
                p if isinstance(p, str) else json.dumps(p, ensure_ascii=False) for p in payloads]},
                ensure_ascii=False),
            text=True, encoding="utf-8", capture_output=True, timeout=10, check=True,
        )
        return json.loads(result.stdout)

    def assert_no_error(self, state):
        self.assertTrue(state["error"]["hidden"], state["error"]["text"])

    def test_comparison_only_summary_file_and_previous_sections_reset(self):
        result = self.page([summary(benchmark_report=benchmark()),
                            summary(comparison_report=comparison())])
        state = result["states"][-1]
        self.assert_no_error(state)
        self.assertIn("comparison-case", state["zr-vs-container"]["html"])
        self.assertIn("2.5×", state["zr-vs-container"]["html"])
        self.assertNotIn("viewer-case", state["matrix-container"]["html"])
        self.assertEqual("", state["native-matrix-container"]["html"])
        self.assertEqual(["utf-8", "utf-8"], result["encodings"])

    def test_actual_bundle_autoloads_without_file_selection(self):
        result = self.page(bundle=summary(benchmark_report=benchmark()))
        self.assertEqual([], result["encodings"])
        self.assertIn("viewer-case", result["states"][0]["matrix-container"]["html"])
        self.assert_no_error(result["states"][0])

    def test_actual_bundle_decodes_utf8_chinese_and_non_bmp(self):
        payload = summary(benchmark_report=benchmark("中文用例🚀"))
        payload["tests_generated_dir"] = "测试目录/数据😀"
        state = self.page(bundle=payload)["states"][0]
        self.assertIn("中文用例🚀", state["matrix-container"]["html"])
        self.assertIn("测试目录/数据😀", state["meta"]["html"])
        self.assert_no_error(state)

    def test_csharp_dotnet_is_in_native_group(self):
        state = self.page([summary(benchmark_report=benchmark(names=("C#/.NET", "Python")))])["states"][-1]
        self.assert_no_error(state)
        self.assertIn("C#/.NET", state["native-matrix-container"]["html"])
        self.assertNotIn("C#/.NET", state["interpreted-matrix-container"]["html"])

    def test_timestamp_is_escaped_before_html_assignment(self):
        payload = summary(benchmark_report=benchmark())
        payload["aggregated_at_utc"] = '<img src=x onerror="alert(1)">&timestamp'
        state = self.page([payload])["states"][-1]
        self.assert_no_error(state)
        self.assertNotIn("<img", state["meta"]["html"])
        self.assertIn("&lt;img src=x onerror=&quot;alert(1)&quot;&gt;&amp;timestamp", state["meta"]["html"])

    def test_legacy_performance_and_dotnet_alias_control(self):
        state = self.page([benchmark(names=(".NET", "C", "Python"))])["states"][-1]
        self.assert_no_error(state)
        self.assertIn("viewer-case", state["matrix-container"]["html"])
        self.assertIn(".NET", state["native-matrix-container"]["html"])
        self.assertIn("Python", state["interpreted-matrix-container"]["html"])
        self.assertIn("legacy-time", state["meta"]["html"])

    def test_legacy_comparison_control(self):
        state = self.page([comparison()])["states"][-1]
        self.assert_no_error(state)
        self.assertIn("comparison-case", state["zr-vs-container"]["html"])

    def test_template_placeholder_waits_for_file_control(self):
        state = self.page()["states"][0]
        self.assertEqual("", state["error"]["text"])
        self.assertIn("aggregate_benchmark_summary.py --bundle-html", state["meta"]["html"])
        self.assertEqual("", state["matrix-container"]["html"])

    def test_invalid_json_file_control(self):
        state = self.page(["{not-json"])["states"][-1]
        self.assertFalse(state["error"]["hidden"])
        self.assertIn("JSON 解析失败", state["error"]["text"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
