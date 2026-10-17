"""Embedded migration findings must point into the host file."""
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from syntax_migration_inventory import SourceKind, SourcePosition, SourceRange, _scan_path


class EmbeddedPositionTests(unittest.TestCase):
    def scan(self, text):
        with mock.patch.object(Path, "read_text", return_value=text):
            return _scan_path(Path("embedded.c"), file="embedded.c",
                              source_kind=SourceKind.EMBEDDED_ZR_FIXTURE)

    def assert_host_span(self, text, finding, spelling):
        offset = text.index(spelling)
        line = text.count("\n", 0, offset) + 1
        column = offset - text.rfind("\n", 0, offset)
        self.assertEqual(SourceRange(SourcePosition(line, column),
                                     SourcePosition(line, column + len(spelling))),
                         finding.source_range)

    def test_decoded_newline_keeps_both_host_columns(self):
        text = r'const char *source = "%foo\n%await";'
        findings = self.scan(text)
        self.assertEqual(2, len(findings))
        for finding, spelling in zip(findings, ("%foo", "%await")):
            self.assert_host_span(text, finding, spelling)

    def test_decoded_comment_boundary_still_uses_host_coordinates(self):
        text = r'const char *source = "%foo\n// ignore %hidden\n%await";'
        findings = self.scan(text)
        self.assertEqual(2, len(findings))
        self.assert_host_span(text, findings[1], "%await")

    def test_each_adjacent_literal_preserves_its_own_host_range(self):
        text = 'const char *source =\n' + r'  "%foo\n"' + '\n' + r'  "\n%await";'
        findings = self.scan(text)
        self.assertEqual(2, len(findings))
        for finding, spelling in zip(findings, ("%foo", "%await")):
            self.assert_host_span(text, finding, spelling)

    def test_real_newline_and_shortened_escape_map_independently(self):
        text = 'const char *source = `prefix\\n\n\\t%await`;'
        findings = self.scan(text)
        self.assertEqual(1, len(findings))
        self.assert_host_span(text, findings[0], "%await")


if __name__ == "__main__":
    unittest.main()

# @unit import_bootstrap [block]
# ## Summary
# 在测试模块导入时，把由本文件位置推导的仓库 scripts 目录放到 sys.path 首位，再导入真实迁移扫描器和坐标记录。
# ## Parameters
# 模块 __file__ 的实际位置和已安装的 Python 标准库；不读取待测宿主 fixture 文件。
# ## Returns
# 成功后本模块持有四个导入名字，进程 sys.path 保留新增的首项；导入失败按 Python 异常传播。
# ## Constraints and lifecycle
# 这是模块级进程状态修改，直接脚本执行和 unittest 导入发现都会执行；没有恢复 sys.path 的局部清理。测试应按原布局运行，不能把脚本复制到另一位置后仍假定相同仓库根。
# bootstrap 不创建宿主 fixture 或日志；现有 CTest 以 -B 启动 Python，抑制导入字节码缓存。不能把这一导入路径操作当作隔离其他并行导入的机制。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:2
# tests/scripts/test_syntax_migration_embedded_positions.py:7
# tests/scripts/test_syntax_migration_embedded_positions.py:8
# scripts/syntax_migration_inventory.py:15
# tests/cmake/syntax-embedded-positions-tests.cmake:4

# @unit EmbeddedPositionTests [type]
# ## Summary
# 集中四个 unittest 场景，验证有限解码之后的 finding 范围仍使用人工宿主字符串的字符行列。
# ## Fields
# 继承 unittest.TestCase，拥有两个手写支持方法和四个 test_ 方法；没有自定义 setUp、tearDown、实例池或文件 fixture。
# scan 通过有作用域的 Path.read_text 替换接入真实 _scan_path；assert_host_span 按人工字符串的首个拼写位置构造预期 SourceRange。
# ## Constraints and lifecycle
# 方法由 unittest 按 test_ 命名发现和调度，不是 Unity RUN_TEST，也没有仓内显式函数指针表。每个 case 的断言失败记为失败后，标准 runner 依其配置继续或停止。
# 四个输入只是 Python str 中的 C/反引号样例，没有编译 C、运行 JavaScript 或执行 Zr 程序；不证明完整宿主语法或真实设备执行。
# 局部字符串、finding 列表由 Python 引用管理；没有自有 teardown 负责全局资源。支持方法的 mock 在离开 with 时恢复，包括异常退出。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:11
# tests/scripts/test_syntax_migration_embedded_positions.py:12
# tests/scripts/test_syntax_migration_embedded_positions.py:17
# tests/scripts/test_syntax_migration_embedded_positions.py:25
# tests/scripts/test_syntax_migration_embedded_positions.py:32
# tests/scripts/test_syntax_migration_embedded_positions.py:38
# tests/scripts/test_syntax_migration_embedded_positions.py:45
# tests/scripts/test_syntax_migration_embedded_positions.py:53

# @unit EmbeddedPositionTests.scan [function]
# ## Summary
# 在当前支持调用的 with 作用域内，用人工字符串替代 Path.read_text，再调用真实 embedded 来源分支，返回其 findings。
# ## Parameters
# @param[in] self 当前 unittest 测试上下文。
# @param[in] text 作为整个虚拟 embedded.c 内容的 Python str。
# ## Returns
# @return 返回 _scan_path 产生的 finding 列表；扫描或 mock 的 Python 异常直接交给 unittest。
# ## Constraints and lifecycle
# Path.read_text 的类方法替换在进程内生效，应避免与不相关的并行 Path 读取交错；退出 with 时正常或异常均恢复原方法。
# 虚拟 Path 和 file 标签均为 embedded.c，不创建该文件；source_kind 固定为 EMBEDDED_ZR_FIXTURE，未开启 repository 模式的 require_embedded_source_context 筛选。
# 返回列表和坐标对象按 Python 引用持有，无本地 Free 或磁盘资源；本方法不自行断言分类、顺序或有效宿主语法。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:12
# tests/scripts/test_syntax_migration_embedded_positions.py:13
# tests/scripts/test_syntax_migration_embedded_positions.py:14
# tests/scripts/test_syntax_migration_embedded_positions.py:27
# tests/scripts/test_syntax_migration_embedded_positions.py:34
# tests/scripts/test_syntax_migration_embedded_positions.py:40
# tests/scripts/test_syntax_migration_embedded_positions.py:47
# scripts/syntax_migration_inventory.py:845
# scripts/syntax_migration_inventory.py:852
# scripts/syntax_migration_inventory.py:855
# scripts/syntax_migration_inventory.py:872

# @unit EmbeddedPositionTests.assert_host_span [function]
# ## Summary
# 按宿主 str 中 spelling 的首个出现位置计算一基行列及同一行的尾后终点，比较 finding 的完整 SourceRange。
# ## Parameters
# @param[in] self 当前 unittest 断言上下文。
# @param[in] text 与 finding 对应的原宿主 str。
# @param[in] finding 具有 source_range 字段的扫描结果。
# @param[in] spelling 在 text 中存在且本场景没有换行的待比较拼写。
# ## Returns
# 比较相等时正常返回 None；不等时抛 AssertionError 供 runner 记录，拼写不存在时 text.index 抛 ValueError。
# ## Constraints and lifecycle
# 只选择 text.index 的首个匹配；终点列通过 len(spelling) 相加，因此不是通用的多行范围或同拼写后续出现位置验证器。
# 不修改 text、finding 或坐标对象，不持有文件或需显式回收的资源；失败不会保证同一 case 后续断言继续执行。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:17
# tests/scripts/test_syntax_migration_embedded_positions.py:18
# tests/scripts/test_syntax_migration_embedded_positions.py:19
# tests/scripts/test_syntax_migration_embedded_positions.py:20
# tests/scripts/test_syntax_migration_embedded_positions.py:21
# tests/scripts/test_syntax_migration_embedded_positions.py:30
# tests/scripts/test_syntax_migration_embedded_positions.py:36
# tests/scripts/test_syntax_migration_embedded_positions.py:43
# tests/scripts/test_syntax_migration_embedded_positions.py:49

# @unit EmbeddedPositionTests.test_decoded_newline_keeps_both_host_columns [function]
# ## Summary
# 用只有一个宿主物理行、但在引号中含反斜杠加 n 的 C 风格字符串，核对两个 decoded finding 各自仍落在该宿主行的原列。
# ## Parameters
# @param[in] self 由 unittest 为当前场景提供的 TestCase 实例。
# ## Returns
# 本 Python 测试函数没有显式返回语句；断言通过时正常返回 None，AssertionError 或其他异常交给 unittest 记录。直接脚本的最终退出状态由 unittest.main 决定。
# ## Constraints and lifecycle
# 人工输入含 %foo、反斜杠加 n、%await；先断言恰好两个 finding，再按列表顺序分别调用 assert_host_span 比较两个拼写的起点及尾后终点。
# 第二个标记的 decoded 终点通过片段尾后哨兵投到宿主闭引号位置；这里不检查分类、迁移理由或 C 编译结果。
# scan 的 Path.read_text 替换在其 with 退出时恢复，包括扫描异常。后续断言只持有 Python 字符串、列表与记录；断言失败会跳过本 case 余下语句，不会终止整个 Python 测试进程的正常 runner 调度。
# 不覆盖非 ASCII、非法行列、未闭合引号或所有转义；结果列表的长度断言失败后，后面的范围断言不会执行。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:25
# tests/scripts/test_syntax_migration_embedded_positions.py:26
# tests/scripts/test_syntax_migration_embedded_positions.py:27
# tests/scripts/test_syntax_migration_embedded_positions.py:28
# tests/scripts/test_syntax_migration_embedded_positions.py:30
# tests/scripts/test_syntax_migration_embedded_positions.py:12
# tests/scripts/test_syntax_migration_embedded_positions.py:17
# tests/scripts/test_syntax_migration_embedded_positions.py:53
# scripts/syntax_migration_inventory.py:859
# scripts/syntax_migration_inventory.py:879
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883

# @unit EmbeddedPositionTests.test_decoded_comment_boundary_still_uses_host_coordinates [function]
# ## Summary
# 让 decoded 换行终止 Zr 行注释，再确认后续标记的 finding 使用宿主字符串中的原行列。
# ## Parameters
# @param[in] self 由 unittest 为当前场景提供的 TestCase 实例。
# ## Returns
# 本 Python 测试函数没有显式返回语句；断言通过时正常返回 None，AssertionError 或其他异常交给 unittest 记录。直接脚本的最终退出状态由 unittest.main 决定。
# ## Constraints and lifecycle
# 人工宿主字符串中的两段反斜杠加 n 在 decoded 文本中形成换行；中间的 // ignore %hidden 属于下游 Zr 行注释，不是引号外宿主注释。
# 只断言 finding 总数为二，并检查第二项对应 %await 的完整宿主范围；不单独核对第一项范围，也不按 legacy_form 字段检查隐藏标记。
# scan 的 Path.read_text 替换在其 with 退出时恢复，包括扫描异常。后续断言只持有 Python 字符串、列表与记录；断言失败会跳过本 case 余下语句，不会终止整个 Python 测试进程的正常 runner 调度。
# 数量与第二项范围是本 case 的有限证据，不扩大成全部注释规则、所有结果分类或第一项坐标已逐一比较。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:32
# tests/scripts/test_syntax_migration_embedded_positions.py:33
# tests/scripts/test_syntax_migration_embedded_positions.py:34
# tests/scripts/test_syntax_migration_embedded_positions.py:35
# tests/scripts/test_syntax_migration_embedded_positions.py:36
# tests/scripts/test_syntax_migration_embedded_positions.py:12
# tests/scripts/test_syntax_migration_embedded_positions.py:17
# tests/scripts/test_syntax_migration_embedded_positions.py:53
# scripts/syntax_migration_inventory.py:859
# scripts/syntax_migration_inventory.py:879
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883

# @unit EmbeddedPositionTests.test_each_adjacent_literal_preserves_its_own_host_range [function]
# ## Summary
# 用分处宿主不同行的两个相邻引号片段，核对各片段 decoded finding 的范围仍指向各自宿主拼写。
# ## Parameters
# @param[in] self 由 unittest 为当前场景提供的 TestCase 实例。
# ## Returns
# 本 Python 测试函数没有显式返回语句；断言通过时正常返回 None，AssertionError 或其他异常交给 unittest 记录。直接脚本的最终退出状态由 unittest.main 决定。
# ## Constraints and lifecycle
# 人工输入含真实宿主换行，第一片段以解码换行结束，第二片段以解码换行开始；断言两个 finding，并分别比较 %foo 和 %await 的完整宿主范围。
# 生成器分别 yield 两个片段，_scan_path 对每个 payload 独立扫描；本 case 未开启 repository 上下文过滤，也没有把两个片段先合成为一段 Zr 源。
# scan 的 Path.read_text 替换在其 with 退出时恢复，包括扫描异常。后续断言只持有 Python 字符串、列表与记录；断言失败会跳过本 case 余下语句，不会终止整个 Python 测试进程的正常 runner 调度。
# 不证明跨片段的注释或词法状态拼接、完整 C 相邻字面量规则，以及 require_embedded_source_context=True 的连续片段筛选分支。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:38
# tests/scripts/test_syntax_migration_embedded_positions.py:39
# tests/scripts/test_syntax_migration_embedded_positions.py:40
# tests/scripts/test_syntax_migration_embedded_positions.py:41
# tests/scripts/test_syntax_migration_embedded_positions.py:43
# tests/scripts/test_syntax_migration_embedded_positions.py:12
# tests/scripts/test_syntax_migration_embedded_positions.py:17
# tests/scripts/test_syntax_migration_embedded_positions.py:53
# scripts/syntax_migration_inventory.py:859
# scripts/syntax_migration_inventory.py:879
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883

# @unit EmbeddedPositionTests.test_real_newline_and_shortened_escape_map_independently [function]
# ## Summary
# 在反引号人工宿主片段中混合解码换行、真实宿主换行和缩短的转义，核对后续标记的原宿主范围。
# ## Parameters
# @param[in] self 由 unittest 为当前场景提供的 TestCase 实例。
# ## Returns
# 本 Python 测试函数没有显式返回语句；断言通过时正常返回 None，AssertionError 或其他异常交给 unittest 记录。直接脚本的最终退出状态由 unittest.main 决定。
# ## Constraints and lifecycle
# fixture 同时包含反斜杠加 n、一个真实换行、反斜杠加 t 与 %await；有限规则把后者解为字母 t，该转义产生的字符映射到反斜杠所在的宿主索引。
# 断言恰好一个 finding，再比较 %await 的完整宿主 SourceRange；不执行 JavaScript 或把反斜杠加 t 宣称为制表符解码。
# scan 的 Path.read_text 替换在其 with 退出时恢复，包括扫描异常。后续断言只持有 Python 字符串、列表与记录；断言失败会跳过本 case 余下语句，不会终止整个 Python 测试进程的正常 runner 调度。
# 没有直接断言每个解码字符的内容，也不覆盖完整 JavaScript 反引号插值、Unicode 转义、字节/UTF-16 坐标或未闭合输入。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:45
# tests/scripts/test_syntax_migration_embedded_positions.py:46
# tests/scripts/test_syntax_migration_embedded_positions.py:47
# tests/scripts/test_syntax_migration_embedded_positions.py:48
# tests/scripts/test_syntax_migration_embedded_positions.py:49
# tests/scripts/test_syntax_migration_embedded_positions.py:12
# tests/scripts/test_syntax_migration_embedded_positions.py:17
# tests/scripts/test_syntax_migration_embedded_positions.py:53
# scripts/syntax_migration_inventory.py:859
# scripts/syntax_migration_inventory.py:879
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883

# @unit __main__ guard [block]
# ## Summary
# 仅在直接执行本测试脚本时启动 unittest 默认测试程序，让 loader 发现当前模块中的四个 test_ 方法并由 runner 汇总结果。
# ## Parameters
# Python 的 __name__ 和 unittest.main 的标准命令行参数；导入模块用于外部发现时不自动启动此入口。
# ## Returns
# unittest.main 默认通过 SystemExit 交回进程状态；测试失败由 runner 记录并影响退出状态，不是这四个方法各自返回整数失败码。
# ## Constraints and lifecycle
# 模块级导入/bootstrap 在 guard 前已经执行；guard 本身不创建 fixture、配置构建或调用迁移工具的 CLI main。
# 没有自有全局 fixture、Free 或终止清理步骤；局部 mock 的恢复由 scan 的 with 保证。各 case 发现顺序采用 unittest 的规则，不把源码排列当作手写直调顺序。
# 现有 CTest 传入 -B 和脚本绝对路径；外部 unittest 导入发现时不会自动启动此 guard。
# ## References
# tests/scripts/test_syntax_migration_embedded_positions.py:7
# tests/scripts/test_syntax_migration_embedded_positions.py:11
# tests/scripts/test_syntax_migration_embedded_positions.py:52
# tests/scripts/test_syntax_migration_embedded_positions.py:53
# tests/cmake/syntax-embedded-positions-tests.cmake:2
# tests/cmake/syntax-embedded-positions-tests.cmake:3
# tests/cmake/syntax-embedded-positions-tests.cmake:4
