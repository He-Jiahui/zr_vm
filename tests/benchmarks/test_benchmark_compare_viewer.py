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

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::module::aggregate_import_protocol [block]
# ## Summary
# 在测试模块装载时定位并执行真实聚合器，使 bundle 场景消费生产注入器。
# ## Constraints and lifecycle
# 先把 BENCHMARK_SCRIPTS 插到本进程 sys.path 首位，再按 viewer_actual_aggregate 名创建模块并调用 loader.exec_module。这里没有把该私有名登记到 sys.modules，也不恢复 sys.path 或常规依赖导入的缓存；聚合器内部的绝对导入仍依赖标准 sys.modules 和该搜索路径。调用方须以可信、稳定 checkout 在受控新进程装载，不能把这种命名方式当成完整模块隔离。无导入异常包装；spec/loader 不可用、依赖导入或执行异常会中止测试模块装载。纯装载不调用聚合 CLI，也不生成 bundle 文件；禁写 pyc 需由解释器 -B 保证，-B 本身不排除读取旧缓存。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:18
# - tests/benchmarks/test_benchmark_compare_viewer.py:19
# - tests/benchmarks/test_benchmark_compare_viewer.py:22
# - tests/benchmarks/test_benchmark_compare_viewer.py:23
# - tests/benchmarks/test_benchmark_compare_viewer.py:55
# - scripts/benchmark/aggregate_benchmark_summary.py:16
# - scripts/benchmark/benchmark_task4_contract.py:12
# - tests/cmake/benchmark-viewer-tests.cmake:5
# - tests/cmake/benchmark-viewer-tests.cmake:6
# - tests/benchmarks/test_benchmark_compare_viewer.py:133

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::spec [state]
# ## Summary
# 保存真实聚合器的文件模块规格，供模块构造与执行共用。
# ## Constraints and lifecycle
# spec_from_file_location 使用固定 viewer_actual_aggregate 名和当前 checkout 脚本路径。此记录不是已登记到 sys.modules 的模块实例，也不拥有测试输出目录；它的 loader 在23行被直接使用。当前路径应对应可加载 Python 文件，代码不为 None spec/loader 提供额外回退；失败传播为模块装载错误。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:19
# - tests/benchmarks/test_benchmark_compare_viewer.py:20
# - tests/benchmarks/test_benchmark_compare_viewer.py:21
# - tests/benchmarks/test_benchmark_compare_viewer.py:22
# - tests/benchmarks/test_benchmark_compare_viewer.py:23

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::aggregate [state]
# ## Summary
# 保留按文件装载的聚合器模块，使 page 调用真实 _inject_bundle。
# ## Constraints and lifecycle
# module_from_spec 创建模块后，23行执行其正文；此测试没有自行写入 sys.modules['viewer_actual_aggregate']。生命周期跟随测试模块全局，page 仅借用其注入函数；常规依赖缓存和 sys.path 不是独立命名空间。不得在用例执行期间替换模块或其注入器；没有显式卸载/恢复钩子。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:22
# - tests/benchmarks/test_benchmark_compare_viewer.py:23
# - tests/benchmarks/test_benchmark_compare_viewer.py:55
# - scripts/benchmark/aggregate_benchmark_summary.py:399
# - scripts/benchmark/aggregate_benchmark_summary.py:406

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::benchmark [function]
# ## Summary
# 构造 legacy performance_report 的最小页面输入，用确定性数值区分实现行。
# ## Parameters
# @param[in] case 写入 case.name 的夹具值，当前场景使用字符串。
# @param[in] names 按枚举顺序生成实现记录的可迭代名字集合；函数不校验名称域。
# ## Returns
# @return 返回新建 performance_report 容器；输入 case/name 值可仍被其字段引用。枚举或分配等异常直接传播。
# ## Constraints and lifecycle
# 每次新建字典/列表，mean_wall_ms 只是 enumerate 顺序生成的 i+1，status='PASS' 是输入夹具字段，不是执行 benchmark 的结果。不验证 case/name 类型或测量合法性；容器里保留调用方提供的值而非深拷贝其对象。当前用例传入字符串和有序名字序列。调用方持有返回模型直至 page 序列化，无文件或 native 资源。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:28
# - tests/benchmarks/test_benchmark_compare_viewer.py:29
# - tests/benchmarks/test_benchmark_compare_viewer.py:31
# - tests/benchmarks/test_benchmark_compare_viewer.py:32
# - tests/benchmarks/test_benchmark_compare_viewer.py:69
# - tests/benchmarks/test_benchmark_compare_viewer.py:80
# - tests/benchmarks/test_benchmark_compare_viewer.py:86
# - tests/benchmarks/test_benchmark_compare_viewer.py:94
# - tests/benchmarks/test_benchmark_compare_viewer.py:100
# - tests/benchmarks/test_benchmark_compare_viewer.py:108
# - scripts/benchmark/benchmark_compare_viewer.html:186

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::comparison [function]
# ## Summary
# 构造 legacy comparison_report，给相对比率表提供固定可识别内容。
# ## Parameters
# @param[in] case 写入唯一 case.name 的夹具值，当前场景为字符串。
# ## Returns
# @return 返回新建 comparison_report 字典；构造异常直接传播。
# ## Constraints and lifecycle
# 新建外层报告字典、cases 列表及 relative_to 字典，固定 c=2.5、dotnet=1.25 仅用于展示断言，不执行真实比率计算或契约校验。case 值原样写入而非深拷贝；无文件句柄、生成目录或浏览器对象。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:35
# - tests/benchmarks/test_benchmark_compare_viewer.py:36
# - tests/benchmarks/test_benchmark_compare_viewer.py:37
# - tests/benchmarks/test_benchmark_compare_viewer.py:70
# - tests/benchmarks/test_benchmark_compare_viewer.py:116
# - scripts/benchmark/benchmark_compare_viewer.html:194
# - scripts/benchmark/benchmark_compare_viewer.html:438

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::summary [function]
# ## Summary
# 把指定报告包装成聚合摘要夹具，让同一页面入口接受单份或组合报告。
# ## Parameters
# @param[in] reports 以报告名为关键字传入的模型对象；其引用被放进返回摘要的 reports。
# ## Returns
# @return 返回新的摘要外层字典和 present 列表，嵌套报告仍与输入共享；构造异常直接传播。
# ## Constraints and lifecycle
# reports 是本次关键字参数字典；present 按关键字插入顺序列名，嵌套报告对象保留原引用，不做深拷贝、字段合法性或测量身份校验。固定 schema/time/目录字段是展示输入，不能当真实产物或运行见证；调用方可在 page 前修改该模型。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:40
# - tests/benchmarks/test_benchmark_compare_viewer.py:41
# - tests/benchmarks/test_benchmark_compare_viewer.py:42
# - tests/benchmarks/test_benchmark_compare_viewer.py:69
# - tests/benchmarks/test_benchmark_compare_viewer.py:70
# - tests/benchmarks/test_benchmark_compare_viewer.py:80
# - tests/benchmarks/test_benchmark_compare_viewer.py:86
# - tests/benchmarks/test_benchmark_compare_viewer.py:94
# - tests/benchmarks/test_benchmark_compare_viewer.py:100
# - scripts/benchmark/benchmark_compare_viewer.html:182

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests [class]
# ## Summary
# 通过真实 HTML、生产 bundle 注入器和 Node 浏览器边界替身，验证页面输入兼容与有限输出文本。
# ## Constraints and lifecycle
# unittest 默认方法发现调度9个 test_ 方法，setUpClass 只设置共享 Node 命令路径；没有 setUp/tearDown 文件夹具。page 每次建立独立 Node 进程，通过 stdin/stdout 交换内存 JSON，返回快照不借用已退出进程中的 DOM。测试仅断言替身元素的字符串/标志与编码参数，不模拟浏览器 HTML parser、真实异步 FileReader、布局、事件执行或完整安全边界。装载会修改本进程 sys.path 并导入聚合器依赖，应使用受控新进程及稳定模块/文件；共享类属性不得并发改写。类级装载失败使用例无法进入，断言失败与其他异常由 unittest 按 runner 策略记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:45
# - tests/benchmarks/test_benchmark_compare_viewer.py:47
# - tests/benchmarks/test_benchmark_compare_viewer.py:52
# - tests/benchmarks/test_benchmark_compare_viewer.py:68
# - tests/benchmarks/test_benchmark_compare_viewer.py:126
# - tests/benchmarks/test_benchmark_compare_viewer.py:132
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/benchmarks/benchmark_compare_viewer_harness.js:19
# - tests/benchmarks/benchmark_compare_viewer_harness.js:34
# - tests/benchmarks/benchmark_compare_viewer_harness.js:45
# - tests/cmake/benchmark-viewer-tests.cmake:4
# - tests/cmake/benchmark-viewer-tests.cmake:8
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.setUpClass [function]
# ## Summary
# 在类级启动阶段选择所有页面用例共用的 Node 可执行命令。
# ## Parameters
# @param[in,out] cls unittest 调度的测试类；本函数在其上发布 node 命令字符串。
# ## Returns
# 正常完成无返回值；找不到非空 Node 命令时抛 RuntimeError，unittest 记录 setUpClass 错误而不进入这些 case。
# ## Constraints and lifecycle
# 优先采用非空 BENCHMARK_VIEWER_NODE，后备 shutil.which('node')；两者均为空则抛 RuntimeError，属于类级 setup error，不是 skip。非空覆盖值不在此执行或验证，失效路径会在 page 启动时报错。仅写 cls.node，不创建文件/夹具，也不设置卸载或恢复钩子；执行用例期间应保持该共享属性稳定。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:47
# - tests/benchmarks/test_benchmark_compare_viewer.py:46
# - tests/benchmarks/test_benchmark_compare_viewer.py:48
# - tests/benchmarks/test_benchmark_compare_viewer.py:49
# - tests/benchmarks/test_benchmark_compare_viewer.py:50
# - tests/benchmarks/test_benchmark_compare_viewer.py:57
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:8
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.page [function]
# ## Summary
# 把文件输入序列或内存 bundle 交给真实页面脚本，并取回 Node harness 的输出快照。
# ## Parameters
# @param[in] self 由 unittest 创建的实例；读取类级 node 路径，不发布持久页面状态。
# @param[in] payloads 文件选择事件的有序输入；字符串作为原始文件文本，其他值须可 JSON 序列化。
# @param[in] bundle 非 None 时供真实注入器编码的模型；None 表示直接执行未注入模板。
# ## Returns
# @return 返回 stdout 解析出的 states/encodings 模型；当前 harness 返回独立 JSON 快照。任何读取、注入、序列化、进程或解析错误直接抛给调用方。
# ## Constraints and lifecycle
# 先只读真实 VIEWER；bundle 非 None 时调用生产 _inject_bundle，在内存完成 UTF-8/base64 注入，不调用聚合 CLI 或写生成页。payloads 中 str 原样传输，其他对象以 ensure_ascii=False 编码，再随 HTML 一起通过 UTF-8 stdin 传入 Node；stdout 必须是 harness JSON。subprocess.run 的 timeout=10 只约束该子进程调用的等待/通信，不是整个 Python case 的硬时限；超时/启动失败/非零退出/序列化或输出解析错误均直接传播，函数不降格为空快照。harness 对每段初始 inline script 使用 vm.runInContext(timeout=2000)，之后由宿主调用的 change/FileReader 同步回调不在该2秒调用范围内，仍处在外层子进程等待之中。当前 harness 仅读 stdin/执行受信仓内模板并写 stdout，无文件夹具、下载或真实浏览器；不要把 vm 当任意不可信页面的安全沙箱。返回 JSON 由父 Python 持有，不再借用子进程对象；管道/子进程生命周期由 subprocess.run 管理。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:52
# - tests/benchmarks/test_benchmark_compare_viewer.py:53
# - tests/benchmarks/test_benchmark_compare_viewer.py:54
# - tests/benchmarks/test_benchmark_compare_viewer.py:55
# - tests/benchmarks/test_benchmark_compare_viewer.py:56
# - tests/benchmarks/test_benchmark_compare_viewer.py:57
# - tests/benchmarks/test_benchmark_compare_viewer.py:58
# - tests/benchmarks/test_benchmark_compare_viewer.py:59
# - tests/benchmarks/test_benchmark_compare_viewer.py:61
# - tests/benchmarks/test_benchmark_compare_viewer.py:63
# - scripts/benchmark/aggregate_benchmark_summary.py:399
# - scripts/benchmark/aggregate_benchmark_summary.py:406
# - tests/benchmarks/benchmark_compare_viewer_harness.js:7
# - tests/benchmarks/benchmark_compare_viewer_harness.js:20
# - tests/benchmarks/benchmark_compare_viewer_harness.js:23
# - tests/benchmarks/benchmark_compare_viewer_harness.js:26
# - tests/benchmarks/benchmark_compare_viewer_harness.js:34
# - tests/benchmarks/benchmark_compare_viewer_harness.js:43
# - tests/benchmarks/benchmark_compare_viewer_harness.js:45
# - tests/benchmarks/benchmark_compare_viewer_harness.js:48
# - scripts/benchmark/benchmark_compare_viewer.html:465
# - scripts/benchmark/benchmark_compare_viewer.html:469
# - scripts/benchmark/benchmark_compare_viewer.html:482
# - tests/benchmarks/test_benchmark_compare_viewer.py:69
# - tests/benchmarks/test_benchmark_compare_viewer.py:80
# - tests/benchmarks/test_benchmark_compare_viewer.py:88
# - tests/benchmarks/test_benchmark_compare_viewer.py:94
# - tests/benchmarks/test_benchmark_compare_viewer.py:102
# - tests/benchmarks/test_benchmark_compare_viewer.py:108
# - tests/benchmarks/test_benchmark_compare_viewer.py:116
# - tests/benchmarks/test_benchmark_compare_viewer.py:121
# - tests/benchmarks/test_benchmark_compare_viewer.py:127

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.assert_no_error [function]
# ## Summary
# 在已取回的单个页面快照上复用错误区隐藏断言，并保留错误文本作为失败说明。
# ## Parameters
# @param[in] self 当前测试实例，用于报告断言结果。
# @param[in] state page 返回的某次快照；必须含 error 字段和预期子字段。
# ## Returns
# 无返回值；hidden 不为真时抛 AssertionError，缺失/错误结构等异常直接传播。
# ## Constraints and lifecycle
# 要求 state 含 error.hidden 与 error.text；只检查 hidden 真值，不独立要求 text 为空，也不证明真实浏览器未执行事件。失败抛 unittest 断言异常；调用时 page 已返回，函数不拥有 Node 进程、文件或页面借用。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:65
# - tests/benchmarks/test_benchmark_compare_viewer.py:66
# - tests/benchmarks/test_benchmark_compare_viewer.py:72
# - tests/benchmarks/test_benchmark_compare_viewer.py:83
# - tests/benchmarks/test_benchmark_compare_viewer.py:91
# - tests/benchmarks/test_benchmark_compare_viewer.py:95
# - tests/benchmarks/test_benchmark_compare_viewer.py:103
# - tests/benchmarks/test_benchmark_compare_viewer.py:109
# - tests/benchmarks/test_benchmark_compare_viewer.py:117
# - tests/benchmarks/benchmark_compare_viewer_harness.js:36
# - tests/benchmarks/benchmark_compare_viewer_harness.js:38

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_comparison_only_summary_file_and_previous_sections_reset [function]
# ## Summary
# 先装 benchmark 摘要再装 comparison-only 摘要，检验第二次文件输入展示比率并消除有限旧内容。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 当前模型通过同一个 Node 进程依次触发两次 change。只直接断言最终错误区隐藏、相对表含 comparison-case/2.5×、全量矩阵不含 viewer-case、native 分类区为空以及两次读取参数均为 utf-8；没有单独断言 interpreted/bar 两区的清空，也不证明真实浏览器 FileReader 时序。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:68
# - tests/benchmarks/test_benchmark_compare_viewer.py:69
# - tests/benchmarks/test_benchmark_compare_viewer.py:70
# - tests/benchmarks/test_benchmark_compare_viewer.py:71
# - tests/benchmarks/test_benchmark_compare_viewer.py:72
# - tests/benchmarks/test_benchmark_compare_viewer.py:73
# - tests/benchmarks/test_benchmark_compare_viewer.py:74
# - tests/benchmarks/test_benchmark_compare_viewer.py:75
# - tests/benchmarks/test_benchmark_compare_viewer.py:76
# - tests/benchmarks/test_benchmark_compare_viewer.py:77
# - tests/benchmarks/benchmark_compare_viewer_harness.js:42
# - tests/benchmarks/benchmark_compare_viewer_harness.js:45
# - scripts/benchmark/benchmark_compare_viewer.html:324
# - scripts/benchmark/benchmark_compare_viewer.html:340
# - scripts/benchmark/benchmark_compare_viewer.html:343
# - scripts/benchmark/benchmark_compare_viewer.html:348
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_actual_bundle_autoloads_without_file_selection [function]
# ## Summary
# 用真实注入器生成内存 bundle，检验页面初始执行能够自动展示 benchmark 内容。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 以 bundle 参数执行未触发文件选择的初始快照；实际断言 encodings 为空、初始矩阵含 viewer-case 与错误区隐藏。不以该正例证明任意模板标记布局、错误 base64 或全部输入格式。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:79
# - tests/benchmarks/test_benchmark_compare_viewer.py:80
# - tests/benchmarks/test_benchmark_compare_viewer.py:81
# - tests/benchmarks/test_benchmark_compare_viewer.py:82
# - tests/benchmarks/test_benchmark_compare_viewer.py:83
# - scripts/benchmark/aggregate_benchmark_summary.py:399
# - scripts/benchmark/aggregate_benchmark_summary.py:402
# - scripts/benchmark/aggregate_benchmark_summary.py:406
# - tests/benchmarks/benchmark_compare_viewer_harness.js:41
# - scripts/benchmark/benchmark_compare_viewer.html:206
# - scripts/benchmark/benchmark_compare_viewer.html:211
# - scripts/benchmark/benchmark_compare_viewer.html:216
# - scripts/benchmark/benchmark_compare_viewer.html:485
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_actual_bundle_decodes_utf8_chinese_and_non_bmp [function]
# ## Summary
# 在真实 bundle 自动加载路径保留中文、emoji 与非 BMP 字符，区分字节映射和 UTF-8 解码。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 模型把中文用例🚀写入 case，把测试目录/数据😀写入摘要目录；只断言两个完整字符串出现在矩阵/meta 且错误区隐藏。harness 的 atob 以 latin1 模拟字节映射，页面再严格 UTF-8 解码；没有覆盖畸形 UTF-8、所有 Unicode 组合或真实浏览器字形。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:85
# - tests/benchmarks/test_benchmark_compare_viewer.py:86
# - tests/benchmarks/test_benchmark_compare_viewer.py:87
# - tests/benchmarks/test_benchmark_compare_viewer.py:88
# - tests/benchmarks/test_benchmark_compare_viewer.py:89
# - tests/benchmarks/test_benchmark_compare_viewer.py:90
# - tests/benchmarks/test_benchmark_compare_viewer.py:91
# - scripts/benchmark/aggregate_benchmark_summary.py:400
# - tests/benchmarks/benchmark_compare_viewer_harness.js:30
# - scripts/benchmark/benchmark_compare_viewer.html:213
# - scripts/benchmark/benchmark_compare_viewer.html:214
# - scripts/benchmark/benchmark_compare_viewer.html:331
# - scripts/benchmark/benchmark_compare_viewer.html:332
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_csharp_dotnet_is_in_native_group [function]
# ## Summary
# 在文件输入路径检查 C#/.NET 名字进入 native 表并从 interpreted 表排除。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 输入仅含 C#/.NET 和 Python，断言错误区隐藏、native HTML 含该完整别名、interpreted HTML 不含该别名；不直接调用私有分类器，不外推任意实现名或完整分类矩阵。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:93
# - tests/benchmarks/test_benchmark_compare_viewer.py:94
# - tests/benchmarks/test_benchmark_compare_viewer.py:95
# - tests/benchmarks/test_benchmark_compare_viewer.py:96
# - tests/benchmarks/test_benchmark_compare_viewer.py:97
# - scripts/benchmark/benchmark_compare_viewer.html:233
# - scripts/benchmark/benchmark_compare_viewer.html:248
# - scripts/benchmark/benchmark_compare_viewer.html:258
# - scripts/benchmark/benchmark_compare_viewer.html:337
# - scripts/benchmark/benchmark_compare_viewer.html:338
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_timestamp_is_escaped_before_html_assignment [function]
# ## Summary
# 给摘要时间戳注入含 HTML 标记、引号和 ampersand 的文本，检查 meta 插值的转义结果。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 只在 meta 输出上断言没有 <img 子串并含完整预期 &lt;/&quot;/&gt;/&amp; 文本，同时检查错误区隐藏。DOM 替身只保存 innerHTML 字符串，不解析 HTML 或执行 onerror，不能据此声称触发过攻击事件、全页面安全审计或任意输入净化。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:99
# - tests/benchmarks/test_benchmark_compare_viewer.py:100
# - tests/benchmarks/test_benchmark_compare_viewer.py:101
# - tests/benchmarks/test_benchmark_compare_viewer.py:102
# - tests/benchmarks/test_benchmark_compare_viewer.py:103
# - tests/benchmarks/test_benchmark_compare_viewer.py:104
# - tests/benchmarks/test_benchmark_compare_viewer.py:105
# - tests/benchmarks/benchmark_compare_viewer_harness.js:11
# - tests/benchmarks/benchmark_compare_viewer_harness.js:38
# - scripts/benchmark/benchmark_compare_viewer.html:330
# - scripts/benchmark/benchmark_compare_viewer.html:331
# - scripts/benchmark/benchmark_compare_viewer.html:354
# - scripts/benchmark/benchmark_compare_viewer.html:359
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_legacy_performance_and_dotnet_alias_control [function]
# ## Summary
# 保留 legacy performance_report 的文件输入控制，验证 .NET 别名与基础分组展示。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 当前输入为 .NET/C/Python；直接断言 viewer-case 仍在矩阵、.NET 在 native、Python 在 interpreted、legacy-time 在 meta 且错误区隐藏。C 是同一夹具成员，但本 case 没有单独断言其分类或所有耗时/比例。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:107
# - tests/benchmarks/test_benchmark_compare_viewer.py:108
# - tests/benchmarks/test_benchmark_compare_viewer.py:109
# - tests/benchmarks/test_benchmark_compare_viewer.py:110
# - tests/benchmarks/test_benchmark_compare_viewer.py:111
# - tests/benchmarks/test_benchmark_compare_viewer.py:112
# - tests/benchmarks/test_benchmark_compare_viewer.py:113
# - scripts/benchmark/benchmark_compare_viewer.html:186
# - scripts/benchmark/benchmark_compare_viewer.html:191
# - scripts/benchmark/benchmark_compare_viewer.html:249
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_legacy_comparison_control [function]
# ## Summary
# 保留 legacy comparison_report 的文件输入控制，确认包装后仍能显示相对表。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 只断言最终错误区隐藏与相对表出现 comparison-case，不单独验证所有语言列、数值或无 benchmark 时的全部区域。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:115
# - tests/benchmarks/test_benchmark_compare_viewer.py:116
# - tests/benchmarks/test_benchmark_compare_viewer.py:117
# - tests/benchmarks/test_benchmark_compare_viewer.py:118
# - scripts/benchmark/benchmark_compare_viewer.html:194
# - scripts/benchmark/benchmark_compare_viewer.html:199
# - scripts/benchmark/benchmark_compare_viewer.html:347
# - scripts/benchmark/benchmark_compare_viewer.html:438
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_template_placeholder_waits_for_file_control [function]
# ## Summary
# 直接执行未注入的原始模板，确认其保留等待输入提示而不伪装为可解析 bundle。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 不传 bundle 或文件序列，检查初始 error.text 为空、meta 含现有 --bundle-html 提示、矩阵 HTML 为空。未独立断言 error.hidden，也不执行文件选择、验证输出文件或读取真实构建目录。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:120
# - tests/benchmarks/test_benchmark_compare_viewer.py:121
# - tests/benchmarks/test_benchmark_compare_viewer.py:122
# - tests/benchmarks/test_benchmark_compare_viewer.py:123
# - tests/benchmarks/test_benchmark_compare_viewer.py:124
# - tests/benchmarks/benchmark_compare_viewer_harness.js:41
# - scripts/benchmark/benchmark_compare_viewer.html:128
# - scripts/benchmark/benchmark_compare_viewer.html:211
# - scripts/benchmark/benchmark_compare_viewer.html:485
# - scripts/benchmark/benchmark_compare_viewer.html:487
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6

# @unit tests/benchmarks/test_benchmark_compare_viewer.py::BenchmarkCompareViewerTests.test_invalid_json_file_control [function]
# ## Summary
# 把语法损坏的文件文本送入实际 change/onload 路径，检查页面暴露 JSON 解析错误。
# ## Parameters
# @param[in] self unittest 为此 case 创建的实例，用于调用页面 helper 与断言。
# ## Returns
# 无返回值；断言失败由 unittest 记为 failure，未捕获的进程/解析等异常记为 error；后续调度遵循 runner 配置。
# ## Constraints and lifecycle
# 传入 {not-json，只断言错误区可见且错误文本含 JSON 解析失败；不检查先前区域是否清除，也不泛化为全部 schema/base64/Unicode/文件读取错误。 page 正常返回时对应 Node 进程已结束，后续断言只持有父进程 JSON；无本地文件夹具或专用清理钩子，失败交由 unittest 记录。
# ## References
# - tests/benchmarks/test_benchmark_compare_viewer.py:126
# - tests/benchmarks/test_benchmark_compare_viewer.py:127
# - tests/benchmarks/test_benchmark_compare_viewer.py:128
# - tests/benchmarks/test_benchmark_compare_viewer.py:129
# - tests/benchmarks/benchmark_compare_viewer_harness.js:23
# - tests/benchmarks/benchmark_compare_viewer_harness.js:45
# - scripts/benchmark/benchmark_compare_viewer.html:469
# - scripts/benchmark/benchmark_compare_viewer.html:471
# - scripts/benchmark/benchmark_compare_viewer.html:478
# - scripts/benchmark/benchmark_compare_viewer.html:479
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/cmake/benchmark-viewer-tests.cmake:6
