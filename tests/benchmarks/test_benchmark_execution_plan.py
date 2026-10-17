#!/usr/bin/env python3
# 固定洗牌向量和序列化结果，保证筛选后的执行计划可复现。
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
BENCHMARK_SCRIPT_ROOT = REPOSITORY_ROOT / "scripts" / "benchmark"
EXECUTION_PLAN_SCRIPT = BENCHMARK_SCRIPT_ROOT / "benchmark_execution_plan.py"
sys.path.insert(0, str(BENCHMARK_SCRIPT_ROOT))

from benchmark_execution_plan import (  # noqa: E402
    EXECUTION_PLAN_SCHEMA_VERSION,
    SHUFFLE_ALGORITHM,
    SHUFFLE_VERSION,
    create_execution_plan,
    serialize_execution_plan,
    shuffle_jobs,
)


def _jobs() -> list[dict[str, str]]:
    return [
        {"case": "a", "implementation": "zr"},
        {"case": "drop-1", "implementation": "python"},
        {"case": "b", "implementation": "zr"},
        {"case": "drop-2", "implementation": "python"},
        {"case": "c", "implementation": "zr"},
        {"case": "d", "implementation": "lua"},
    ]


def _identities(jobs: list[dict[str, str]]) -> list[tuple[str, str]]:
    return [(job["case"], job["implementation"]) for job in jobs]


class BenchmarkExecutionPlanTests(unittest.TestCase):
    def test_same_seed_reproduces_order_without_mutating_input(self) -> None:
        jobs = _jobs()
        original = [dict(job) for job in jobs]

        first = create_execution_plan(jobs, seed=42)
        second = create_execution_plan(jobs, seed=42)

        self.assertEqual(first, second)
        self.assertEqual(original, jobs)
        self.assertEqual(EXECUTION_PLAN_SCHEMA_VERSION, first["schema_version"])
        self.assertEqual(SHUFFLE_ALGORITHM, first["algorithm"])
        self.assertEqual(SHUFFLE_VERSION, first["version"])
        self.assertEqual(42, first["seed"])

    def test_different_seeds_change_nontrivial_order_and_preserve_each_job_once(self) -> None:
        jobs = _jobs()
        seed_zero = create_execution_plan(jobs, seed=0)["jobs"]
        seed_one = create_execution_plan(jobs, seed=1)["jobs"]

        self.assertNotEqual(seed_zero, seed_one)
        self.assertEqual(Counter(_identities(jobs)), Counter(_identities(seed_zero)))
        self.assertEqual(Counter(_identities(jobs)), Counter(_identities(seed_one)))

    def test_filtering_happens_before_shuffle(self) -> None:
        plan = create_execution_plan(
            _jobs(),
            seed=0,
            cases=["a", "b", "c", "d"],
        )

        self.assertEqual(
            ["c", "b", "a", "d"],
            [job["case"] for job in plan["jobs"]],
        )
        self.assertEqual(
            {
                "cases": ["a", "b", "c", "d"],
                "implementations": None,
            },
            plan["filters"],
        )
        self.assertEqual(4, plan["job_count"])

    def test_shuffle_jobs_matches_fixed_fisher_yates_vector(self) -> None:
        jobs = [
            {"case": name, "implementation": "zr"}
            for name in "abcdef"
        ]

        shuffled = shuffle_jobs(jobs, seed=0)

        self.assertEqual("ecfdab", "".join(job["case"] for job in shuffled))

    def test_rejects_malformed_duplicate_and_empty_job_identities(self) -> None:
        invalid_job_lists = (
            None,
            [],
            [{}],
            [{"case": "a"}],
            [{"case": "", "implementation": "zr"}],
            [{"case": "   ", "implementation": "zr"}],
            [{"case": " a", "implementation": "zr"}],
            [{"case": "a", "implementation": ""}],
            [{"case": "a", "implementation": 1}],
            [{"case": "a", "implementation": "zr", "extra": True}],
            [
                {"case": "a", "implementation": "zr"},
                {"case": "a", "implementation": "zr"},
            ],
        )
        for jobs in invalid_job_lists:
            with self.subTest(jobs=jobs):
                with self.assertRaises(ValueError):
                    create_execution_plan(jobs, seed=0)

    def test_rejects_invalid_seed_and_filters(self) -> None:
        for seed in (-1, 1 << 64, True, 1.5):
            with self.subTest(seed=seed):
                with self.assertRaises(ValueError):
                    create_execution_plan(_jobs(), seed=seed)

        invalid_filters = (
            {"cases": []},
            {"cases": [""]},
            {"cases": ["a", "a"]},
            {"cases": "a"},
            {"implementations": [" zr"]},
            {"implementations": [1]},
        )
        for filters in invalid_filters:
            with self.subTest(filters=filters):
                with self.assertRaises(ValueError):
                    create_execution_plan(_jobs(), seed=0, **filters)

        with self.assertRaises(ValueError):
            create_execution_plan(_jobs(), seed=0, cases=["not-present"])

    def test_serialized_plan_is_byte_identical_for_the_same_seed(self) -> None:
        first = serialize_execution_plan(_jobs(), seed=0x12345678)
        second = serialize_execution_plan(_jobs(), seed=0x12345678)

        self.assertEqual(first, second)
        self.assertTrue(first.endswith(b"\n"))
        self.assertNotIn(b"\r\n", first)

    def test_cli_supports_structured_stdin_and_file_io(self) -> None:
        request = {
            "jobs": _jobs(),
            "seed": 0,
            "filters": {"cases": ["a", "b", "c", "d"]},
        }
        request_bytes = json.dumps(request, separators=(",", ":")).encode("ascii")

        first = subprocess.run(
            [sys.executable, str(EXECUTION_PLAN_SCRIPT)],
            input=request_bytes,
            check=False,
            capture_output=True,
        )
        second = subprocess.run(
            [sys.executable, str(EXECUTION_PLAN_SCRIPT)],
            input=request_bytes,
            check=False,
            capture_output=True,
        )
        self.assertEqual(0, first.returncode, first.stderr.decode("utf-8"))
        self.assertEqual(first.stdout, second.stdout)
        self.assertEqual(["c", "b", "a", "d"], [
            job["case"] for job in json.loads(first.stdout)["jobs"]
        ])

        with tempfile.TemporaryDirectory() as temporary_directory:
            input_path = Path(temporary_directory) / "plan-input.json"
            output_path = Path(temporary_directory) / "plan-output.json"
            input_path.write_bytes(request_bytes)
            result = subprocess.run(
                [
                    sys.executable,
                    str(EXECUTION_PLAN_SCRIPT),
                    "--input",
                    str(input_path),
                    "--output",
                    str(output_path),
                ],
                check=False,
                capture_output=True,
            )

            self.assertEqual(0, result.returncode, result.stderr.decode("utf-8"))
            self.assertEqual(first.stdout, output_path.read_bytes())
            self.assertEqual(b"", result.stdout)

    def test_cli_rejects_malformed_duplicate_and_unknown_structures(self) -> None:
        cases = (
            b"not-json",
            b"[]",
            b'{"jobs":[],"seed":0}',
            b'{"jobs":[{"case":"a","implementation":"zr"}],"seed":0,"extra":1}',
            b'{"jobs":[{"case":"a","implementation":"zr"}],"seed":0,"filters":{"unknown":[]}}',
            b'{"jobs":[{"case":"a","implementation":"zr"},{"case":"a","implementation":"zr"}],"seed":0}',
        )
        for request in cases:
            with self.subTest(request=request):
                result = subprocess.run(
                    [sys.executable, str(EXECUTION_PLAN_SCRIPT)],
                    input=request,
                    check=False,
                    capture_output=True,
                )
                self.assertEqual(2, result.returncode)
                self.assertTrue(result.stderr.startswith(b"error: "))
                self.assertEqual(b"", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)

# @unit tests/benchmarks/test_benchmark_execution_plan.py::test module
#
# ## Summary
# 把执行计划的确定顺序、有限非法输入和 CLI 字节 I/O 分成九个 unittest 场景，直接运行时由标准入口发现并派发方法。这里只生成和核对计划，不启动基准作业或完整性能套件。
#
# ## Parameters
# 模块没有函数式参数；标准入口把进程 argv 交给 unittest.main。导入时从路径配置选定的模块绑定六个生产名称，路径配置另有独立合同。
#
# ## Returns
# 模块导入成功后发布测试类型与 helper；直接运行使用 unittest.main 的普通结果和退出协议，不把方法的 None 返回当作程序通过。
#
# ## Constraints and lifecycle
# 普通 import 会先执行路径配置和生产模块导入；导入失败不会回滚 sys.path 改动，预先存在的同名 sys.modules 项也可能被复用。标准 guard 在被其他 loader 导入时不执行 main，调用方仍须正确发现本类。默认 loader 按 test_ 名称及其排序配置选择方法，不能假定源码顺序。没有自定义 setUp/tearDown/load_tests 或手写临时资源总管；I/O 和子进程只在对应方法实际执行时发生。未禁用的普通导入可能写 pycache。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:2 原文件目的
# - tests/benchmarks/test_benchmark_execution_plan.py:19 普通生产 API 导入
# - tests/benchmarks/test_benchmark_execution_plan.py:44 测试类定义
# - tests/benchmarks/test_benchmark_execution_plan.py:219 既有标准 guard，归本入口单元
# - tests/benchmarks/test_benchmark_execution_plan.py:220 unittest 主入口
# - tests/cmake/benchmark-registration.cmake:74 Python 可用条件
# - tests/cmake/benchmark-registration.cmake:81 现有 CTest 名称
# - tests/cmake/benchmark-registration.cmake:83 实际测试脚本命令

# @unit tests/benchmarks/test_benchmark_execution_plan.py::script paths
#
# ## Summary
# 在测试模块载入时从测试文件的真实位置导出仓库和生产脚本路径，并把基准脚本目录放到普通 import 搜索首项，使进程内 API 与子进程 CLI 指向同一预期实现。
#
# ## Fields and invariants
# REPOSITORY_ROOT 是 resolve 后的祖先 Path；BENCHMARK_SCRIPT_ROOT 指向 scripts/benchmark；EXECUTION_PLAN_SCRIPT 是其 benchmark_execution_plan.py 子路径。Path 值只描述路径，不持有文件句柄，也不冻结磁盘内容。
#
# ## Constraints and lifecycle
# 路径计算不验证仓库身份、生产内容或 Python 模块来源。sys.path.insert 是可见的进程级修改，不会在测试结束后自动恢复；已有 sys.modules 缓存可能使普通 import 复用既有对象。子进程使用脚本路径重新载入磁盘版本，而当前进程持有已导入的函数，比较期间两者及借用统计模块须稳定；没有文件锁或多进程一致性快照。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:14 文件位置解析根
# - tests/benchmarks/test_benchmark_execution_plan.py:15 脚本目录
# - tests/benchmarks/test_benchmark_execution_plan.py:16 CLI 路径
# - tests/benchmarks/test_benchmark_execution_plan.py:17 进程级搜索路径修改
# - tests/benchmarks/test_benchmark_execution_plan.py:19 消费导入目录
# - tests/benchmarks/test_benchmark_execution_plan.py:159 首 stdin 子进程
# - tests/benchmarks/test_benchmark_execution_plan.py:165 第二 stdin 子进程
# - tests/benchmarks/test_benchmark_execution_plan.py:183 文件子进程
# - tests/benchmarks/test_benchmark_execution_plan.py:209 负向子进程

# @unit tests/benchmarks/test_benchmark_execution_plan.py::_jobs
#
# ## Summary
# 为多个 API 和 CLI 场景提供相同的六项候选顺序，其中两个 Python 作业供筛选正例排除，其余四项供顺序与身份计数比较。
#
# ## Parameters
# 无显式参数；不读取仓库文件、环境或随机状态。
#
# ## Returns
# @return 返回新的六元素 list，且每个作业 dict 均为本次新建；字符串标量可共享。调用方拥有普通可变容器，不需要释放文件或进程资源。
#
# ## Constraints and lifecycle
# 每次调用互不共享 list/dict 容器，供修改前值对照及重复输入构造。固定字符串不是运行器注册表认证，也不表示这些作业会实际执行；该 helper 不调用生产验证器或断言，不构造通用基准配置。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:29 定义
# - tests/benchmarks/test_benchmark_execution_plan.py:31 首候选
# - tests/benchmarks/test_benchmark_execution_plan.py:32 被筛除 Python 候选
# - tests/benchmarks/test_benchmark_execution_plan.py:34 第二被筛候选
# - tests/benchmarks/test_benchmark_execution_plan.py:46 不修改输入正例
# - tests/benchmarks/test_benchmark_execution_plan.py:70 筛选正例
# - tests/benchmarks/test_benchmark_execution_plan.py:152 CLI 请求组装

# @unit tests/benchmarks/test_benchmark_execution_plan.py::_identities
#
# ## Summary
# 把作业顺序投影为复合身份列表，交给 Counter 比较多重集合，检查固定输入在两个洗牌结果中各出现一次。
#
# ## Parameters
# @param[in] jobs 按顺序可迭代、各元素可用 case 和 implementation 键读取的作业记录；读取期间保持稳定。
#
# ## Returns
# @return 返回新的 list[tuple[str,str]] 投影，顺序与输入迭代一致；字段值按引用读取，不验证其类型、唯一性或作业合法性。
#
# ## Constraints and lifecycle
# 函数不修改输入，没有 I/O 或额外资源。缺键或不可索引对象的异常直接传播；它不是生产验证替身。当前调用使用固定字符串字段，Counter 所需的可哈希性由这些夹具保证，投影本身不对任意输入作此认证。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:40 定义
# - tests/benchmarks/test_benchmark_execution_plan.py:41 字段读取投影
# - tests/benchmarks/test_benchmark_execution_plan.py:65 两次调用比较 seed0
# - tests/benchmarks/test_benchmark_execution_plan.py:66 两次调用比较 seed1

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests
#
# ## Summary
# 以独立 TestCase 方法组织确定顺序、有限非法输入、稳定序列化与三类 CLI I/O 场景。各方法只拥有自己的内存值，文件夹具仅在文件 I/O 正例的局部上下文内存在。
#
# ## Fields and invariants
# 没有手写实例状态、类级可变夹具、构造函数、setUp 或 tearDown。两个模块 helper 独立于 TestCase；九个 test_ 方法经标准 loader 反射取得绑定方法。
#
# ## Constraints and lifecycle
# 默认方法名排序不等于源码顺序，各方法不应依赖前一方法运行。普通方法在未处理断言或异常后退出；含 subTest 的方法在支持它且非 failfast 的 runner 中可记录失败并继续下一情形。failfast、不支持 subTest 的结果对象、KeyboardInterrupt 等会改变边界。唯一 TemporaryDirectory 由其所在 with 管理，没有类级回收器替代它；子进程没有源码内超时或隔离环境。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:44 继承 TestCase
# - tests/benchmarks/test_benchmark_execution_plan.py:45 首个方法
# - tests/benchmarks/test_benchmark_execution_plan.py:116 非法 jobs 子情形
# - tests/benchmarks/test_benchmark_execution_plan.py:122 seed 子情形
# - tests/benchmarks/test_benchmark_execution_plan.py:135 过滤子情形
# - tests/benchmarks/test_benchmark_execution_plan.py:176 唯一临时目录
# - tests/benchmarks/test_benchmark_execution_plan.py:207 CLI 子情形
# - tests/benchmarks/test_benchmark_execution_plan.py:220 默认入口

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_same_seed_reproduces_order_without_mutating_input
#
# ## Summary
# 用同一六作业输入和 seed42 构造两次计划，检查深层值相等、原作业值未变及所选顶层版本字段，固定重复构造的可观察合同。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 两次构造均在断言之前完成；任一调用异常会阻止后续断言，普通断言失败会结束本方法。original 是对固定字符串字段字典的逐项浅复制，足以作当前值对照；不以此证明任意嵌套对象深复制或所有返回容器地址独立。只检查 schema_version、algorithm、version、seed，不检查全部 rng/filter 字段；不使用文件或子进程。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:46 新作业夹具
# - tests/benchmarks/test_benchmark_execution_plan.py:47 当前字段的副本
# - tests/benchmarks/test_benchmark_execution_plan.py:49 第一次构造
# - tests/benchmarks/test_benchmark_execution_plan.py:50 第二次构造
# - tests/benchmarks/test_benchmark_execution_plan.py:52 结果值相等
# - tests/benchmarks/test_benchmark_execution_plan.py:53 原输入值保留
# - tests/benchmarks/test_benchmark_execution_plan.py:54 schema 字段
# - tests/benchmarks/test_benchmark_execution_plan.py:57 seed 字段
# - scripts/benchmark/benchmark_execution_plan.py:157 先验证输入
# - scripts/benchmark/benchmark_execution_plan.py:169 新结果发布

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_different_seeds_change_nontrivial_order_and_preserve_each_job_once
#
# ## Summary
# 比较固定六作业在 seed0 和 seed1 下的两份顺序，并用复合身份 Counter 与输入逐一比较，防止这一固定向量丢失或重复作业。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 先生成两份结果再断言，普通失败结束本方法。Counter 比较关注身份多重集合，不检查额外字段、内存地址或所有计划元数据。结论限当前输入及两个 seed，不能扩成任意不同 seed 必有不同排列或随机质量证明；无文件、子进程和持久随机状态。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:60 固定输入
# - tests/benchmarks/test_benchmark_execution_plan.py:61 seed0 结果
# - tests/benchmarks/test_benchmark_execution_plan.py:62 seed1 结果
# - tests/benchmarks/test_benchmark_execution_plan.py:64 顺序不等
# - tests/benchmarks/test_benchmark_execution_plan.py:65 seed0 Counter 对照
# - tests/benchmarks/test_benchmark_execution_plan.py:66 seed1 Counter 对照
# - scripts/benchmark/benchmark_execution_plan.py:168 生产洗牌入口

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_filtering_happens_before_shuffle
#
# ## Summary
# 对包含两个待排除候选的固定输入应用四个 case 过滤，再检查 seed0 的 c,b,a,d 顺序、回显过滤条件和 job_count，锁定筛选与洗牌的协作次序。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 输入全部来自内存 helper。三个断言依次执行，普通失败阻止后续检查；只检查 case 过滤、implementations 为 None 和计数4，不验证任意过滤组合、实现过滤正例或真实 registry 中的作业存在性。返回结果是普通局部值，无文件和子进程资源。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:69 构造调用
# - tests/benchmarks/test_benchmark_execution_plan.py:72 四 case 过滤
# - tests/benchmarks/test_benchmark_execution_plan.py:75 固定顺序断言
# - tests/benchmarks/test_benchmark_execution_plan.py:79 回显过滤条件
# - tests/benchmarks/test_benchmark_execution_plan.py:86 计数断言
# - scripts/benchmark/benchmark_execution_plan.py:163 先过滤
# - scripts/benchmark/benchmark_execution_plan.py:168 后洗牌

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_shuffle_jobs_matches_fixed_fisher_yates_vector
#
# ## Summary
# 直接调用独立洗牌 API，对六个连续名称和同一实现的输入检查 seed0 产生 ecfdab，给版本化随机源与 Fisher–Yates 的组合提供固定观察点。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 该方法自建六个 dict，不使用 _jobs 或文件夹具。只比较输出 case 串，不在本方法断言输入未变、容器不别名、其他 seed 或跨平台执行；这些不能从向量名称推断。调用或普通断言失败结束方法，局部值由 Python 管理。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:89 局部输入
# - tests/benchmarks/test_benchmark_execution_plan.py:94 独立洗牌调用
# - tests/benchmarks/test_benchmark_execution_plan.py:96 ecfdab 向量
# - scripts/benchmark/benchmark_execution_plan.py:145 API 先验证作业
# - scripts/benchmark/benchmark_execution_plan.py:134 借用 SplitMix64
# - scripts/benchmark/benchmark_execution_plan.py:137 交换索引
# - scripts/benchmark/benchmark_statistics.py:37 随机源更新

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_rejects_malformed_duplicate_and_empty_job_identities
#
# ## Summary
# 将十一种普通非法作业输入逐项送入计划 API，要求各自抛 ValueError，覆盖非列表、空列表、字段与身份不合格以及重复复合身份。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 每项都在 subTest 内包住 assertRaises；支持 subTest 且非 failfast 的普通 runner 可记录失败或错误后继续下一迭代，该情形失败位置之后的语句不再执行。failfast、不支持 subTest 的 runner 或 KeyboardInterrupt 等会改变边界。只要求 ValueError，不断言错误文本、全部任意 Python 对象或非字符串键；这里没有 TemporaryDirectory 或自有文件。有效 seed0 使这些场景针对较早的作业拒绝路径。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:99 十一项输入表
# - tests/benchmarks/test_benchmark_execution_plan.py:115 逐项循环
# - tests/benchmarks/test_benchmark_execution_plan.py:116 局部 subTest
# - tests/benchmarks/test_benchmark_execution_plan.py:117 要求 ValueError
# - tests/benchmarks/test_benchmark_execution_plan.py:118 API 调用
# - scripts/benchmark/benchmark_execution_plan.py:33 非 list 拒绝
# - scripts/benchmark/benchmark_execution_plan.py:35 空 jobs 拒绝
# - scripts/benchmark/benchmark_execution_plan.py:46 字段集合检查
# - scripts/benchmark/benchmark_execution_plan.py:55 case 身份
# - scripts/benchmark/benchmark_execution_plan.py:61 重复身份

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_rejects_invalid_seed_and_filters
#
# ## Summary
# 用有效固定作业隔离四个非法 seed、六个非法过滤输入及一个零匹配过滤，分别要求 ValueError，区分随机源域和筛选域的拒绝入口。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# seed 与过滤两组循环分别使用 subTest，在支持且非 failfast 的 runner 中可记录局部失败后继续；最后零匹配 assertRaises 不在 subTest 内，失败会结束本方法。作业有效；seed 负例在过滤成功后到随机源验证，过滤负例可以更早拒绝。只验证异常类型，不检查 uint64 最大合法值、一般错误优先级或所有过滤形状；不创建文件或子进程。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:121 四 seed
# - tests/benchmarks/test_benchmark_execution_plan.py:122 seed subTest
# - tests/benchmarks/test_benchmark_execution_plan.py:124 seed API 调用
# - tests/benchmarks/test_benchmark_execution_plan.py:126 六过滤样例
# - tests/benchmarks/test_benchmark_execution_plan.py:135 filter subTest
# - tests/benchmarks/test_benchmark_execution_plan.py:137 filter API 调用
# - tests/benchmarks/test_benchmark_execution_plan.py:139 subTest 外异常断言
# - tests/benchmarks/test_benchmark_execution_plan.py:140 零匹配调用
# - scripts/benchmark/benchmark_execution_plan.py:73 过滤形状拒绝
# - scripts/benchmark/benchmark_execution_plan.py:105 空筛选结果
# - scripts/benchmark/benchmark_statistics.py:46 seed 类型拒绝
# - scripts/benchmark/benchmark_statistics.py:48 uint64 范围拒绝

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_serialized_plan_is_byte_identical_for_the_same_seed
#
# ## Summary
# 以同一固定输入和 seed0x12345678 生成两份完整序列化 bytes，检查字节相等、末尾 LF 和没有 CRLF，约束机器消费的可重放字节格式。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 两个调用都先完成后依次断言；异常或普通断言失败结束本方法。比较的是两次当前实现结果，不是独立完整 JSON golden；不逐字段核所有 schema，也不验证编码错误、任意输入或未来算法版本。输出是局部不可变 bytes，没有文件发布或回滚语义。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:143 首次 bytes
# - tests/benchmarks/test_benchmark_execution_plan.py:144 第二次 bytes
# - tests/benchmarks/test_benchmark_execution_plan.py:146 全 bytes 相等
# - tests/benchmarks/test_benchmark_execution_plan.py:147 末 LF
# - tests/benchmarks/test_benchmark_execution_plan.py:148 无 CRLF
# - scripts/benchmark/benchmark_execution_plan.py:196 固定 JSON 渲染
# - scripts/benchmark/benchmark_execution_plan.py:203 UTF-8 bytes 与末 LF
# - scripts/benchmark/benchmark_execution_plan.py:213 先构造后编码

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_cli_supports_structured_stdin_and_file_io
#
# ## Summary
# 把同一结构化请求分别交给两个 stdin CLI 进程，再在自有临时目录中以输入输出文件运行第三个进程，比较固定顺序与文件 bytes，连接纯值计划和 CLI I/O。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 两个 stdin 子进程在首个断言前已同步等待完成；只独立断言 first.returncode 为0，second 的返回码和 stderr 没有单独断言。随后比较 stdout 并解析首个 JSON 的 case 顺序；错误会阻止进入 TemporaryDirectory。文件阶段由 with 在正常退出、断言或异常展开时清理，进入失败不保证清理，清理自身也可失败。输入由父进程写 plan-input.json，输出由第三个 CLI 创建；文件内容比较和 stdout 断言都在上下文内。四处 subprocess.run 共同未给 timeout、env、cwd 或 -B；此方法执行其中三次，普通非零返回不由 check=False 自动抛错。message 参数的 UTF-8 stderr 解码也可先于断言产生异常。父 -B 不作为子进程禁缓存的继承保证；禁缓存须设置可继承环境。没有共享仓库夹具或额外清理钩子。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:151 固定请求
# - tests/benchmarks/test_benchmark_execution_plan.py:156 ASCII 请求 bytes
# - tests/benchmarks/test_benchmark_execution_plan.py:158 首 stdin run
# - tests/benchmarks/test_benchmark_execution_plan.py:164 第二 stdin run
# - tests/benchmarks/test_benchmark_execution_plan.py:170 只独立检查首返回码
# - tests/benchmarks/test_benchmark_execution_plan.py:171 两 stdout 相等
# - tests/benchmarks/test_benchmark_execution_plan.py:172 固定 case 顺序
# - tests/benchmarks/test_benchmark_execution_plan.py:176 唯一目录上下文
# - tests/benchmarks/test_benchmark_execution_plan.py:179 父写输入
# - tests/benchmarks/test_benchmark_execution_plan.py:180 文件 CLI
# - tests/benchmarks/test_benchmark_execution_plan.py:193 文件进程返回码
# - tests/benchmarks/test_benchmark_execution_plan.py:194 读输出比较
# - tests/benchmarks/test_benchmark_execution_plan.py:195 文件进程 stdout 空
# - scripts/benchmark/benchmark_execution_plan.py:307 CLI 先读并解析
# - scripts/benchmark/benchmark_execution_plan.py:314 成功后写出
# - scripts/benchmark/benchmark_execution_plan.py:282 文件输出辅助

# @unit tests/benchmarks/test_benchmark_execution_plan.py::BenchmarkExecutionPlanTests.test_cli_rejects_malformed_duplicate_and_unknown_structures
#
# ## Summary
# 依次给 CLI 六份固定非法请求，要求返回2、stderr 有 error: 前缀且 stdout 为空，区分解析与普通作业验证的已选拒绝入口。
#
# ## Parameters
# @param[in,out] self 由 unittest 为当前方法构造并绑定的 TestCase 实例；断言及子情形结果由其 runner 协议记录。
#
# ## Returns
# 正常完成时返回 None，不提供场景失败码；断言或异常由 unittest 记录，subTest 的局部恢复边界见约束。整个程序的退出状态由 runner 产生。
#
# ## Constraints and lifecycle
# 每份请求在 subTest 内同步等待子进程，再顺序执行三个断言；首个失败会跳过该情形后续断言。在支持 subTest 且非 failfast 的普通 runner 中仍可继续下一请求；其他 runner、failfast 或 KeyboardInterrupt 等可能中止。循环全到达才有六次进程启动，不是六个独立测试方法。没有显式 timeout、env、cwd 或 -B，也没有临时目录。所谓 duplicate 只指重复作业身份，不是重复 JSON 键；不覆盖非法 UTF-8、非标准 JSON 常量、缺必需字段或输出写失败。
#
# ## References
# - tests/benchmarks/test_benchmark_execution_plan.py:198 六请求表
# - tests/benchmarks/test_benchmark_execution_plan.py:206 请求循环
# - tests/benchmarks/test_benchmark_execution_plan.py:207 subTest
# - tests/benchmarks/test_benchmark_execution_plan.py:208 同步子进程
# - tests/benchmarks/test_benchmark_execution_plan.py:214 返回2
# - tests/benchmarks/test_benchmark_execution_plan.py:215 stderr 前缀
# - tests/benchmarks/test_benchmark_execution_plan.py:216 stdout 空
# - scripts/benchmark/benchmark_execution_plan.py:249 非法 JSON 转 ValueError
# - scripts/benchmark/benchmark_execution_plan.py:250 非对象根
# - scripts/benchmark/benchmark_execution_plan.py:253 未知根字段
# - scripts/benchmark/benchmark_execution_plan.py:263 未知过滤字段
# - scripts/benchmark/benchmark_execution_plan.py:315 CLI 捕获 ValueError/OSError
