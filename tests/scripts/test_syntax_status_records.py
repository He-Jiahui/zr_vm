"""验证 55 份语法里程碑状态记录的筛选规则与仓库冻结分布。"""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


def _repository_root_from_arguments() -> Path:
    """保留命令行指定仓库的测试入口，便于在隔离 checkout 重放同一契约。"""
    if "--repository" not in sys.argv:
        return Path(__file__).resolve().parents[2]
    argument_index = sys.argv.index("--repository")
    try:
        root = Path(sys.argv[argument_index + 1]).resolve()
    except IndexError as error:
        raise SystemExit("--repository requires a path") from error
    del sys.argv[argument_index : argument_index + 2]
    return root


REPOSITORY_ROOT = _repository_root_from_arguments()
SCRIPT_ROOT = REPOSITORY_ROOT / "scripts"
sys.path.insert(0, str(SCRIPT_ROOT))

from syntax_status_records import (  # noqa: E402
    EXPECTED_DIRECTORY_COUNTS,
    collect_syntax_status_records,
    validate_syntax_status_records,
)


class SyntaxStatusRecordTests(unittest.TestCase):
    """同时校验合成记录的边界与真实计划目录，防止支持文档被误算为交付里程碑。"""

    @staticmethod
    def _write_record(root: Path, relative_path: str, status: str, time: str) -> None:
        """构造与生产解析器共用的最小 Markdown 状态记录。"""
        path = root / "docs" / "plans" / "syntax" / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            "# Fixture\n\n"
            "## Status\n\n"
            f"- Status: {status}\n"
            f"- Completion time: {time}\n",
            encoding="utf-8",
        )

    def test_selector_uses_numbered_directories_and_excludes_support_records(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self._write_record(root, "01-types/m1.md", "completed", "2026-01-01 01:00 +08:00")
            self._write_record(
                root,
                "02-references/nested/m2.md",
                "completed_with_known_baseline_markers",
                "2026-01-01 02:00 +08:00",
            )
            self._write_record(
                root,
                "01-types/m3-implementation-plan.md",
                "completed",
                "2026-01-01 03:00 +08:00",
            )
            self._write_record(
                root,
                "05-property-unified-ast/m5-task4-property-import-bootstrap.md",
                "completed",
                "2026-01-01 04:00 +08:00",
            )
            self._write_record(root, "support/m4.md", "completed", "2026-01-01 05:00 +08:00")
            self._write_record(root, "top-level.md", "completed", "2026-01-01 06:00 +08:00")

            report = collect_syntax_status_records(root)

        self.assertEqual(
            ("01-types/m1.md", "02-references/nested/m2.md"),
            tuple(record.relative_path for record in report.records),
        )
        self.assertEqual({"01": 1, "02": 1}, report.directory_counts)
        self.assertEqual(2, report.complete_count)
        self.assertEqual(0, len(report.missing_status))
        self.assertEqual(0, len(report.non_complete))
        self.assertEqual(0, len(report.missing_time))

    def test_status_parser_accepts_chinese_and_qualified_completion(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            path = root / "docs" / "plans" / "syntax" / "01-types" / "m1.md"
            path.parent.mkdir(parents=True)
            path.write_text(
                "# Fixture\n\n"
                "- \u5b8c\u6210\u65f6\u95f4\uff1a2026-01-01 01:00 +08:00\n"
                "- \u72b6\u6001\uff1a\u5df2\u5b8c\u6210\uff08M1 gate\uff09\n",
                encoding="utf-8",
            )

            report = collect_syntax_status_records(root)

        self.assertEqual(1, report.complete_count)
        self.assertEqual(0, len(report.missing_status))
        self.assertEqual(0, len(report.non_complete))
        self.assertEqual(0, len(report.missing_time))

    def test_validation_rejects_non_complete_or_drifted_record_sets(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self._write_record(
                root,
                "01-types/m1.md",
                "in_progress",
                "2026-01-01 01:00 +08:00",
            )
            report = collect_syntax_status_records(root)
            issues = validate_syntax_status_records(report)

        self.assertEqual(("01-types/m1.md",), report.non_complete)
        self.assertTrue(any("expected 55 records" in issue for issue in issues))
        self.assertTrue(any("directory distribution differs" in issue for issue in issues))
        self.assertTrue(any("non-complete status" in issue for issue in issues))

    def test_repository_matches_frozen_55_record_contract(self) -> None:
        """仓库级门禁：新增或改写计划状态时，须有意更新 55 条基线及目录分布。"""
        report = collect_syntax_status_records(REPOSITORY_ROOT)
        issues = validate_syntax_status_records(report)

        self.assertEqual(55, len(report.records))
        self.assertEqual(55, report.complete_count)
        self.assertEqual(EXPECTED_DIRECTORY_COUNTS, report.directory_counts)
        self.assertEqual((), issues)


if __name__ == "__main__":
    unittest.main(verbosity=2)

# @unit module entry and roots [config]
# ## Summary
# 在 unittest 载入测试模块时先确定被测仓库，再从该仓库的 scripts 目录绑定状态记录 API，使合成记录与真实 55 条记录使用同一实现和期望分布。这里组织测试依赖，不执行生产 CLI 或收集记录。
#
# ## Fields and value model
# REPOSITORY_ROOT 保存参数 helper 返回的已解析路径；SCRIPT_ROOT 指向其 scripts 子目录。三个 from-import 名称分别借用生产常量字典、收集函数和校验函数；常量字典没有深复制。
#
# ## Constraints and lifecycle
# 初始化在类定义和方法派发前同步完成。根目录参数会从共享 sys.argv 删除，sys.path 首项也会改变；之后导入失败不会回滚这些进程级改变。普通 import 使用 Python 的 sys.modules 缓存，不能仅凭 sys.path 首项证明既有同名模块的来源；独立运行应提供预期的模块绑定和稳定的目录/常量。未加 -B 或等效设置的普通导入可能写入字节码缓存。
#
# ## References
# tests/scripts/test_syntax_status_records.py:24 模块调用根参数 helper
# tests/scripts/test_syntax_status_records.py:26 发布 scripts 搜索优先级
# tests/scripts/test_syntax_status_records.py:28 普通导入生产 API
# tests/scripts/test_syntax_status_records.py:126 真实仓库方法消费根
# tests/scripts/test_syntax_status_records.py:131 比较借用的期望分布
# scripts/syntax_status_records.py:13 生产期望分布原定义

# @unit _repository_root_from_arguments [function]
# ## Summary
# 在模块配置前消费专用 --repository 选项，避免它交给 unittest 参数解析；没有该选项时以测试文件位置选择默认仓库。
#
# ## Parameters
# 无显式参数；读取进程共享的 sys.argv 和本模块 __file__。需要标准可修改 argv 列表，选项值按普通路径解释。
#
# ## Returns
# @return 返回 resolve 后的 Path 值；不创建目录、不验证仓库内容或存在性。首次 --repository 后没有值时抛出带说明的 SystemExit，不返回 Path；路径解析的其他异常原样传播。
#
# ## Constraints and lifecycle
# 只处理第一个精确匹配的 --repository 及其后一项；成功解析后才删除这两项。没有值或 resolve 先失败时不执行删除。重复选项和其他参数仍留给后续 loader；不支持 --repository=... 的专门解析，也不提供路径认证或并发 argv 修改保护。
#
# ## References
# tests/scripts/test_syntax_status_records.py:13 无选项分支
# tests/scripts/test_syntax_status_records.py:17 有值时解析路径
# tests/scripts/test_syntax_status_records.py:19 缺值的 SystemExit
# tests/scripts/test_syntax_status_records.py:20 成功解析后改 argv
# tests/scripts/test_syntax_status_records.py:24 唯一仓内直接调用

# @unit SyntaxStatusRecordTests [type]
# ## Summary
# 把状态记录选择、中文完成字段、冻结分布的负向诊断与真实仓库 55 条集合分成四个 unittest 场景。合成场景检验有限规则，真实场景检验当时选定文档集合，两者不能互相替代。
#
# ## Fields and value model
# 继承 unittest.TestCase 的派发与断言协议；本类没有手写 __init__、setUp、tearDown 或长期夹具字段。四个 test_ 方法由 loader 发现，_write_record 是静态夹具写入器，不参与 test_ 方法选择。
#
# ## Constraints and lifecycle
# 默认 loader 依据 test_ 前缀和可调用属性发现方法并使用其排序配置，不能假定按源码声明顺序运行。实例经继承的标准机制获得方法名；静态 helper 通过 self 访问时不隐式绑定 self。三个合成方法各自进入 TemporaryDirectory，退出后才检查值报告；没有类级清理器替代上下文。断言失败或 I/O/清理异常由 unittest 区分 failure/error；本模块不使用 Unity longjmp。
#
# ## References
# tests/scripts/test_syntax_status_records.py:35 TestCase 子类定义
# tests/scripts/test_syntax_status_records.py:38 静态夹具方法描述符
# tests/scripts/test_syntax_status_records.py:51 选择器场景
# tests/scripts/test_syntax_status_records.py:88 字段解析场景
# tests/scripts/test_syntax_status_records.py:107 负向校验场景
# tests/scripts/test_syntax_status_records.py:124 真实仓库场景
# tests/scripts/test_syntax_status_records.py:136 直接入口委托 unittest

# @unit _write_record [function]
# ## Summary
# 为选择规则和负向校验场景写入人工 Markdown 状态记录，保持字段名称与生产解析器一致；这里只生成测试数据，不调用收集或校验 API。
#
# ## Parameters
# @param[in] root 调用方管理的夹具根 Path；方法在其 docs/plans/syntax 下写入，参数对象本身不转移。
# @param[in] relative_path 受信任的人工记录路径。现有调用均为固定相对字面量；方法不检查绝对路径、.. 或根目录包含关系。
# @param[in] status 直接插入 Status 列表项的人工文本；不验证完成类别或转义嵌入换行。
# @param[in] time 直接插入 Completion time 列表项的人工文本；不解析日期或检查格式。
#
# ## Returns
# 无 C 风格状态码，正常完成隐式返回 None；结果通过新建父目录和 UTF-8 Markdown 文件交付。mkdir、编码或写入异常向调用方传播，可能已留下目录、截断文件或部分内容，不提供原子替换和回滚。
#
# ## Constraints and lifecycle
# @staticmethod 保证通过测试实例调用时仍只接收四个显式参数。现有七个调用都位于成功进入的 TemporaryDirectory 上下文中，该上下文拥有目录清理；本 helper 不删除文件或关闭调用方资源。Path.write_text 自行管理其文件句柄。需要写入目标由当前场景独占，方法没有锁或不可信路径隔离。
#
# ## References
# tests/scripts/test_syntax_status_records.py:38 静态描述符
# tests/scripts/test_syntax_status_records.py:41 根与相对路径连接
# tests/scripts/test_syntax_status_records.py:43 直接 UTF-8 文件写入
# tests/scripts/test_syntax_status_records.py:54 选择场景首个调用
# tests/scripts/test_syntax_status_records.py:110 负向场景调用
# scripts/syntax_status_records.py:32 生产状态字段规则
# scripts/syntax_status_records.py:36 生产时间字段规则

# @unit test_selector_uses_numbered_directories_and_excludes_support_records [function]
# ## Summary
# 以同一个临时根里的六份人工记录隔离选择器的边界：保留两个编号目录记录，排除实现计划、指定支撑记录、support 目录和顶层记录，防止完成文本本身把支撑文档误算成里程碑。
#
# ## Parameters
# @param[in] self unittest 提供的当前 TestCase 实例；借用其静态夹具方法与断言 API，没有持有临时根的实例字段。
#
# ## Returns
# 正常完成隐式返回 None；每个断言通过表示本组有限路径和汇总值符合预期。AssertionError 停止当前方法并由 unittest 记录 failure；创建、写入、读取或清理异常可在断言前作为 error 传播。
#
# ## Constraints and lifecycle
# 本用例在自己的 TemporaryDirectory 创建 root，生产收集函数来自已初始化的模块导入。六个写入都成功后才收集。with 退出后才断言，报告中的路径/状态是值快照，不依赖已删除文件继续存在；上下文清理失败会阻止这些断言。只覆盖 01/02 及给定排除例，不证明全部目录名、符号链接或畸形路径规则。
#
# ## References
# tests/scripts/test_syntax_status_records.py:52 本用例临时根
# tests/scripts/test_syntax_status_records.py:54 正向编号记录
# tests/scripts/test_syntax_status_records.py:63 实现计划排除例
# tests/scripts/test_syntax_status_records.py:69 固定支撑记录排除例
# tests/scripts/test_syntax_status_records.py:73 support 目录例
# tests/scripts/test_syntax_status_records.py:74 顶层例
# tests/scripts/test_syntax_status_records.py:76 实际收集调用
# tests/scripts/test_syntax_status_records.py:79 仅保留两条的精确路径
# tests/scripts/test_syntax_status_records.py:82 目录计数
# scripts/syntax_status_records.py:102 生产选择器
# tests/scripts/test_syntax_status_records.py:136 标准 loader 间接派发

# @unit test_status_parser_accepts_chinese_and_qualified_completion [function]
# ## Summary
# 用一份中文列表字段记录验证解析器识别全角冒号、完成时间字段及带 M1 gate 限定说明的“已完成”状态，防止把字段标签语言和完成说明误作未完成。
#
# ## Parameters
# @param[in] self unittest 当前实例，仅用于断言；生产函数绑定来自模块级普通导入。
#
# ## Returns
# 正常完成隐式返回 None；四个断言分别检查完成数和三个问题分区为空。断言失败由 unittest 记录 failure；夹具写入、收集或上下文清理异常可先使方法成为 error，后续断言不再到达。
#
# ## Constraints and lifecycle
# root 和唯一 Markdown 都在本用例自己的 TemporaryDirectory 中创建，生产收集函数来自全局导入。UTF-8 数据字符串仅作为 Markdown 夹具，不执行代码。with 正常退出并清理后才断言值报告。只覆盖给定中文标签、全角冒号和括号限定文本，不证明全部语言/时间格式，也没有直接比较保留的 status 或 completion_time 字符串。
#
# ## References
# tests/scripts/test_syntax_status_records.py:89 本用例临时根
# tests/scripts/test_syntax_status_records.py:91 唯一人工文件
# tests/scripts/test_syntax_status_records.py:95 中文完成时间字段
# tests/scripts/test_syntax_status_records.py:96 中文限定完成状态
# tests/scripts/test_syntax_status_records.py:100 实际收集调用
# tests/scripts/test_syntax_status_records.py:102 完成数断言
# scripts/syntax_status_records.py:32 状态匹配规则
# scripts/syntax_status_records.py:36 时间匹配规则
# scripts/syntax_status_records.py:54 完成判定属性
# tests/scripts/test_syntax_status_records.py:136 标准 loader 间接派发

# @unit test_validation_rejects_non_complete_or_drifted_record_sets [function]
# ## Summary
# 以仅含一个 in_progress 记录的临时集合同时触发数量、目录分布及未完成状态三类问题，确认校验器保留诊断而非静默接纳新的冻结集合。
#
# ## Parameters
# @param[in] self unittest 当前实例，用于构造受信任夹具和检查问题文本；不转移资源所有权。
#
# ## Returns
# 正常完成隐式返回 None；先精确检查 non_complete 路径，再分别要求三个诊断子串各至少出现在一条问题中。断言失败由 unittest 记录 failure；写入、收集、校验或目录清理异常可在断言前传播为 error。
#
# ## Constraints and lifecycle
# 本用例独立创建 TemporaryDirectory 的 root，生产函数和期望常量来自已初始化导入。issues 与 report 在临时目录退出后按值检查。本例不要求问题的完整字节、顺序或精确数量，也不构造缺 Status、缺 Completion time 或独立只有某一个错误的情形；不得把三个 any 子串检查扩大为每个诊断分支已单独隔离。
#
# ## References
# tests/scripts/test_syntax_status_records.py:108 本用例临时根
# tests/scripts/test_syntax_status_records.py:110 唯一人工记录调用
# tests/scripts/test_syntax_status_records.py:113 未完成状态文本
# tests/scripts/test_syntax_status_records.py:116 收集调用
# tests/scripts/test_syntax_status_records.py:117 校验调用
# tests/scripts/test_syntax_status_records.py:119 non_complete 精确路径
# tests/scripts/test_syntax_status_records.py:120 数量子串
# tests/scripts/test_syntax_status_records.py:121 分布子串
# tests/scripts/test_syntax_status_records.py:122 状态子串
# scripts/syntax_status_records.py:169 生产校验器
# tests/scripts/test_syntax_status_records.py:136 标准 loader 间接派发

# @unit test_repository_matches_frozen_55_record_contract [function]
# ## Summary
# 读取选定仓库的真实状态记录，通过记录数、完成数、期望目录分布和空问题元组四个条件核对冻结的 55 条合同；这是一项有限文档集合门禁，不是构建、语言功能或整个仓库完成性证明。
#
# ## Parameters
# @param[in] self unittest 当前实例；REPOSITORY_ROOT、收集/校验函数及 EXPECTED_DIRECTORY_COUNTS 均来自已初始化的模块配置。
#
# ## Returns
# 正常完成隐式返回 None；四个断言逐项确认这次收集形成的值报告。前面的读取/解析/校验异常会阻止断言，首个失败断言也会阻止后续断言；unittest 记录对应 failure/error。
#
# ## Constraints and lifecycle
# 不创建临时目录或子进程，不写文档。调用期间需要生产选择规则、期望分布字典及被选 Markdown 的路径成员和内容保持稳定；收集没有锁，也不是文件系统原子快照。当前规则只进入首字符为数字的直接子目录并排除实现计划和指定支撑记录，不代表检查所有 docs/plans 或所有 Markdown。测试不读取 JSON golden、不调用生产 CLI，也不证明当前计划的实际工程交付状态。
#
# ## References
# tests/scripts/test_syntax_status_records.py:124 真实仓库方法定义
# tests/scripts/test_syntax_status_records.py:126 实际收集调用
# tests/scripts/test_syntax_status_records.py:127 实际校验调用
# tests/scripts/test_syntax_status_records.py:129 记录数断言
# tests/scripts/test_syntax_status_records.py:130 完成数断言
# tests/scripts/test_syntax_status_records.py:131 借用期望分布比较
# tests/scripts/test_syntax_status_records.py:132 空问题元组
# scripts/syntax_status_records.py:102 真实路径选择范围
# scripts/syntax_status_records.py:130 收集值快照
# scripts/syntax_status_records.py:169 校验合同
# tests/scripts/test_syntax_status_records.py:136 标准 loader 间接派发
