"""Focused safety tests for scripts/layout/apply_moves.py.

Only finite synthetic files are created, under the task's registered test
artifact root. This suite never copies production source files.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import stat
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


# @unit tests/tooling/test_layout_moves.py::REPO [state]
#
# ## Summary
# 从当前测试文件定位实际checkout，只用于装载待测实现。
#
# ## Constraints and lifecycle
# 不把 REPO 作为可移动夹具根；真实迁移 main 在 run_tool 收到 self.root。不复制源码。
#
# ## References
# - tests/tooling/test_layout_moves.py:33
# - tests/tooling/test_layout_moves.py:46
# - tests/tooling/test_layout_moves.py:243
REPO = Path(__file__).resolve().parents[2]
# @unit tests/tooling/test_layout_moves.py::TOOL [state]
#
# ## Summary
# 固定要装载的真实迁移脚本路径。
#
# ## Constraints and lifecycle
# 仅供 spec_from_file_location 读取；本测试不改 TOOL。此路径与原CLI入口不同于受控人工仓根。
#
# ## References
# - tests/tooling/test_layout_moves.py:46
# - tests/tooling/test_layout_moves.py:76
# - tests/tooling/test_layout_moves.py:94
TOOL = REPO / "scripts" / "layout" / "apply_moves.py"
# @unit tests/tooling/test_layout_moves.py::module::dynamic_loader [block]
#
# ## Summary
# 在测试发现前按真实路径建立并执行待测模块，不运行其main guard。
#
# ## Constraints and lifecycle
# module_from_spec对象在exec前写sys.modules以满足dataclass；异常不恢复之前同名项。exec执行生产顶层根选择/正则/装饰器，不是纯AST读取；实际测试时需隔离缓存和根环境。
#
# ## References
# - tests/tooling/test_layout_moves.py:76
# - tests/tooling/test_layout_moves.py:77
# - tests/tooling/test_layout_moves.py:92
# - tests/tooling/test_layout_moves.py:93
# - tests/tooling/test_layout_moves.py:94
# - tests/tooling/test_layout_moves.py:115
# @unit tests/tooling/test_layout_moves.py::SPEC [state]
#
# ## Summary
# 保存待测实现的动态装载描述，在执行前验证 loader 存在。
#
# ## Constraints and lifecycle
# assert 在模块导入时执行，失败则测试尚未注册，不能依赖TestCase清理。SPEC.name 作为 sys.modules 键；并不表示 main 已执行。
#
# ## References
# - tests/tooling/test_layout_moves.py:76
# - tests/tooling/test_layout_moves.py:77
# - tests/tooling/test_layout_moves.py:92
# - tests/tooling/test_layout_moves.py:93
# - tests/tooling/test_layout_moves.py:94
SPEC = importlib.util.spec_from_file_location("apply_moves", TOOL)
assert SPEC and SPEC.loader
# @unit tests/tooling/test_layout_moves.py::migration [state]
#
# ## Summary
# 保存从真实脚本装载的模块对象，供所有用例与夹具根共享。
#
# ## Constraints and lifecycle
# sys.modules 先发布再 exec_module，支持 dataclass；导入失败不会自动恢复旧绑定。所有用例共用其lru/global状态，tearDown不卸载。Python -B 控制字节码写入但不等于所有宿主无I/O。
#
# ## References
# - tests/tooling/test_layout_moves.py:92
# - tests/tooling/test_layout_moves.py:93
# - tests/tooling/test_layout_moves.py:94
# - tests/tooling/test_layout_moves.py:115
# - tests/tooling/test_layout_moves.py:243
migration = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = migration
SPEC.loader.exec_module(migration)

# The sandbox must stay under the tool's own registered fixture root, since
# --root is rejected outside it.  Both sides read the same portable constant.
# @unit tests/tooling/test_layout_moves.py::FIXTURE_ROOT [state]
#
# ## Summary
# 借用生产模块选定的登记夹具父根，确保 main 接受用例 --root。
#
# ## Constraints and lifecycle
# 运行前同时设置生产两个环境根以定向到本任务独占 E 子树；TEMP/TMP本身不替换Windows默认。测试只拥有本次case随机子目录和明确atomic-tmp等产物，不能清理共享旧根。
#
# ## References
# - tests/tooling/test_layout_moves.py:96
# - tests/tooling/test_layout_moves.py:97
# - tests/tooling/test_layout_moves.py:115
# - tests/tooling/test_layout_moves.py:165
# - tests/tooling/test_layout_moves.py:166
# - tests/tooling/test_layout_moves.py:1492
# - tests/tooling/test_layout_moves.py:1581
# - tests/tooling/test_layout_moves.py:2064
FIXTURE_ROOT = migration.REGISTERED_FIXTURE_ROOT


# @unit tests/tooling/test_layout_moves.py::LayoutMoveTests [class]
#
# ## Summary
# 组织36个有限人工夹具回归，验证迁移工具的允许路径/文本/FileAPI边界。
#
# ## Fields and invariants
# Fields: 每次 setUp 建 self.temp/self.root，mapping 与 fixture helpers 只写该树；FIXTURE_ROOT 是共享登记父根。test* 方法由 unittest 反射发现，setUp/tearDown 为回调；本类不启动外部CMake或Git。模块级 dynamic loader/caches 和 atomic-tmp 兄弟目录不归普通 tearDown 全量回收。断言失败通常结束当前方法，subTest 局部记录可继续；不能把源码遍历计数当运行trace。
#
# ## Constraints and lifecycle
# 类型本身不授予路径、文件树或夹具的所有权；构造和使用受以上约束。
#
# ## References
# - tests/tooling/test_layout_moves.py:33
# - tests/tooling/test_layout_moves.py:76
# - tests/tooling/test_layout_moves.py:93
# - tests/tooling/test_layout_moves.py:94
# - tests/tooling/test_layout_moves.py:115
# - tests/tooling/test_layout_moves.py:142
# - tests/tooling/test_layout_moves.py:164
# - tests/tooling/test_layout_moves.py:191
# - tests/tooling/test_layout_moves.py:240
# - tests/tooling/test_layout_moves.py:718
# - tests/tooling/test_layout_moves.py:2064
# - tests/tooling/test_layout_moves.py:2071
class LayoutMoveTests(unittest.TestCase):
    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.setUp [function]
    #
    # ## Summary
    # 在共享登记父根下为当前用例分配独占随机树。
    #
    # ## Parameters
    # - self: 当前 TestCase；成功后持有 temp/root。
    #
    # ## Returns
    # 完成返回 None；mkdir/TemporaryDirectory 失败向 unittest 传播。
    #
    # ## Constraints and lifecycle
    # 先 mkdir 父根，再创建 case- 随机目录，最后建立映射表父目录。setUp 失败不会调用本类 tearDown；这里未 addCleanup 注册主 temp，不能承诺所有失败都显式回收。父根必须事先独立登记且属于本次运行，不能清理其它会话目录。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:164
    # - tests/tooling/test_layout_moves.py:165
    # - tests/tooling/test_layout_moves.py:166
    # - tests/tooling/test_layout_moves.py:167
    # - tests/tooling/test_layout_moves.py:168
    # - tests/tooling/test_layout_moves.py:191
    def setUp(self) -> None:
        FIXTURE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case-", dir=FIXTURE_ROOT)
        self.root = Path(self.temp.name)
        (self.root / "scripts/layout").mkdir(parents=True)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.tearDown [function]
    #
    # ## Summary
    # 在成功完成 setUp 的当前用例结束后清理随机夹具树。
    #
    # ## Parameters
    # - self: 持有本用例 TemporaryDirectory 的 TestCase。
    #
    # ## Returns
    # 正常清理返回 None；cleanup 错误可成为测试错误。
    #
    # ## Constraints and lifecycle
    # unittest 在测试断言失败后通常仍调用 tearDown，但不会在 setUp 失败时调用。只清 self.temp，不移除共享 FIXTURE_ROOT、模块缓存或固定 atomic-tmp 兄弟目录；进程中止不在保证内。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:164
    # - tests/tooling/test_layout_moves.py:166
    # - tests/tooling/test_layout_moves.py:191
    # - tests/tooling/test_layout_moves.py:192
    # - tests/tooling/test_layout_moves.py:2059
    # - tests/tooling/test_layout_moves.py:2068
    def tearDown(self) -> None:
        self.temp.cleanup()

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.mapping [function]
    #
    # ## Summary
    # 为当前夹具写入迁移表，供真实 main 读取。
    #
    # ## Parameters
    # - self: 已经成功 setUp 的当前用例。
    # - rows: 写入的 TSV 文本，刻意畸形输入也可由测试传入。
    #
    # ## Returns
    # 写完返回 None；文件错误传播。
    #
    # ## Constraints and lifecycle
    # 只写 self.root/scripts/layout/layout_map.tsv，父目录由 setUp 提供；不在此做迁移验证或更新仓库表。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:168
    # - tests/tooling/test_layout_moves.py:214
    # - tests/tooling/test_layout_moves.py:215
    # - tests/tooling/test_layout_moves.py:390
    def mapping(self, rows: str) -> None:
        (self.root / "scripts/layout/layout_map.tsv").write_text(rows, encoding="utf-8")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.run_tool [function]
    #
    # ## Summary
    # 以内存 argv 调用真实 main，并合并捕获本次 stdout/stderr。
    #
    # ## Parameters
    # - self: 提供受控 fixture root 的 TestCase。
    # - args: 附加 CLI 字符串；--root 固定为当前随机夹具。
    #
    # ## Returns
    # main 普通返回时得到 (code,combined_output)；SystemExit 或未包装异常直接传播。
    #
    # ## Constraints and lifecycle
    # 不启动子进程、不复制源。redirect_stdout/stderr 是进程全局临时替换，退出 context 会恢复但不适合并发执行这些用例。--apply 会真实改动夹具文件；不能把此 helper 当纯预览。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:240
    # - tests/tooling/test_layout_moves.py:241
    # - tests/tooling/test_layout_moves.py:242
    # - tests/tooling/test_layout_moves.py:243
    # - tests/tooling/test_layout_moves.py:244
    # - tests/tooling/test_layout_moves.py:432
    # - tests/tooling/test_layout_moves.py:1141
    def run_tool(self, *args: str) -> tuple[int, str]:
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            code = migration.main(["--root", str(self.root), *args])
        return code, output.getvalue()

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.fixture_module [function]
    #
    # ## Summary
    # 建立有限 Demo C/header 人工源，包含 BOM、CRLF 和旧路径注释。
    #
    # ## Parameters
    # - self: 拥有独占夹具树的 TestCase。
    #
    # ## Returns
    # 返回 source/header 两个 Path；I/O失败传播。
    #
    # ## Constraints and lifecycle
    # 写入仅本 case 的人工常量；不读取或复制产品源码。调用同一 case 中第二次时 mkdir 默认行为可能拒绝已存在模块目录；调用方按当前一次初始化协议使用。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:268
    # - tests/tooling/test_layout_moves.py:269
    # - tests/tooling/test_layout_moves.py:270
    # - tests/tooling/test_layout_moves.py:273
    # - tests/tooling/test_layout_moves.py:274
    # - tests/tooling/test_layout_moves.py:275
    # - tests/tooling/test_layout_moves.py:388
    def fixture_module(self) -> tuple[Path, Path]:
        module = self.root / "zr_vm_demo/src/zr_vm_demo"
        module.mkdir(parents=True)
        source = module / "demo.c"
        header = module / "demo_internal.h"
        source.write_bytes(b"\xef\xbb\xbf#include \"demo_internal.h\"\r\n/* zr_vm_demo/src/zr_vm_demo/demo.c */\r\n")
        header.write_bytes(b"#define DEMO 1\n")
        return source, header

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.write_file_api [function]
    #
    # ## Summary
    # 为当前人工 source 写最小 codemodel 和逐组 include roots。
    #
    # ## Parameters
    # - self: 拥有 source 所属根的 TestCase。
    # - build: 本用例可写的人工构建目录；不是实际 CMake 输出。
    # - source: 须能相对 self.root 的人工源 Path。
    # - variants: 每个变体为有序 Path 元组，生成一个 sourceIndexes=[0] 的 compile group。
    #
    # ## Returns
    # 写完返回 None；文件/相对化/序列化错误传播。
    #
    # ## Constraints and lifecycle
    # 只发布 index-test/cm/target 三份 JSON，不执行 CMake，不认证完整 File API schema。目录与 JSON 由本测试拥有；source 根在 paths.source 明确写入。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:304
    # - tests/tooling/test_layout_moves.py:306
    # - tests/tooling/test_layout_moves.py:308
    # - tests/tooling/test_layout_moves.py:311
    # - tests/tooling/test_layout_moves.py:315
    # - tests/tooling/test_layout_moves.py:317
    # - tests/tooling/test_layout_moves.py:319
    # - tests/tooling/test_layout_moves.py:320
    # - tests/tooling/test_layout_moves.py:1497
    def write_file_api(self, build: Path, source: Path,
                       variants: tuple[tuple[Path, ...], ...]) -> None:
        reply = build / ".cmake/api/v1/reply"
        reply.mkdir(parents=True, exist_ok=True)
        (reply / "index-test.json").write_text(json.dumps({
            "objects": [{"kind": "codemodel", "jsonFile": "cm.json"}],
        }), encoding="utf-8")
        (reply / "cm.json").write_text(json.dumps({
            "paths": {"source": str(self.root)},
            "configurations": [{"name": "Debug", "targets": [{"jsonFile": "target.json"}]}],
        }), encoding="utf-8")
        groups = [{
            "includes": [{"path": str(include_root)} for include_root in roots],
            "sourceIndexes": [0],
        } for roots in variants]
        (reply / "target.json").write_text(json.dumps({
            "sources": [{"path": source.relative_to(self.root).as_posix()}],
            "compileGroups": groups,
        }), encoding="utf-8")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_rejects_unsafe_and_duplicate_map_paths [function]
    #
    # ## Summary
    # 验证四个畸形表输入会被拒绝，避免由场景名外推具体守门分支。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。四轮分别写父逃逸、同址 third_party、带改名的重复源、单次改名表。 四个 assertRaises 只要求 MigrationError；首例到 safe_relative 点组件检查，同址例先被 old==new 拒绝，后两例先被 basename 保留检查拒绝，未覆盖 duplicate-source 集合检查。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:349
    # - tests/tooling/test_layout_moves.py:351
    # - tests/tooling/test_layout_moves.py:357
    # - tests/tooling/test_layout_moves.py:358
    # - tests/tooling/test_layout_moves.py:359
    # - scripts/layout/apply_moves.py:334
    # - scripts/layout/apply_moves.py:339
    # - scripts/layout/apply_moves.py:388
    # - scripts/layout/apply_moves.py:390
    # - scripts/layout/apply_moves.py:401
    def test_rejects_unsafe_and_duplicate_map_paths(self) -> None:
        table = self.root / "unsafe.tsv"
        for body in (
            "../escape.c\tescape.c\n",
            "src/third_party/a.c\tsrc/third_party/a.c\n",
            "src/a.c\tsrc/b.c\nsrc/a.c\tsrc/c.c\n",
            "src/a.c\tsrc/group/renamed.c\n",
        ):
            table.write_text(body, encoding="utf-8")
            with self.assertRaises(migration.MigrationError):
                migration.load_moves(table)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_preview_does_not_mutate_dirty_bytes_or_paths [function]
    #
    # ## Summary
    # 证明普通预览只报告迁移意图并保留人工脏内容。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。写 Demo 源与单文件迁移，保存原 bytes 后调用 --dry-run。 若全部断言到达，返回0、含 preview、源 bytes 相同且新目的不存在；不证明带 --plan 的预览无 JSON 写入。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:387
    # - tests/tooling/test_layout_moves.py:389
    # - tests/tooling/test_layout_moves.py:391
    # - tests/tooling/test_layout_moves.py:392
    # - tests/tooling/test_layout_moves.py:393
    # - tests/tooling/test_layout_moves.py:394
    # - tests/tooling/test_layout_moves.py:395
    # - scripts/layout/apply_moves.py:2701
    # - scripts/layout/apply_moves.py:2702
    # - scripts/layout/apply_moves.py:2704
    # - scripts/layout/apply_moves.py:2737
    def test_preview_does_not_mutate_dirty_bytes_or_paths(self) -> None:
        source, _ = self.fixture_module()
        original = source.read_bytes()
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        code, output = self.run_tool("--dry-run")
        self.assertEqual(code, 0, output)
        self.assertIn("preview:", output)
        self.assertEqual(source.read_bytes(), original)
        self.assertFalse((self.root / "zr_vm_demo/src/zr_vm_demo/group/demo.c").exists())

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_rewrites_repository_paths_after_source_root_separators [function]
    #
    # ## Summary
    # 检验仓路径前有 CMake根和宿主反斜杠前缀时的字节改写。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。人工 consumer.cmake 含两种完整旧路径，运行 --apply。 比较完整 consumer bytes 为两种预期新路径；不运行 CMake 或 Windows路径解析器。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:423
    # - tests/tooling/test_layout_moves.py:427
    # - tests/tooling/test_layout_moves.py:432
    # - tests/tooling/test_layout_moves.py:433
    # - tests/tooling/test_layout_moves.py:434
    # - tests/tooling/test_layout_moves.py:436
    # - tests/tooling/test_layout_moves.py:437
    # - scripts/layout/apply_moves.py:1261
    # - scripts/layout/apply_moves.py:1267
    # - scripts/layout/apply_moves.py:1280
    # - scripts/layout/apply_moves.py:1969
    def test_rewrites_repository_paths_after_source_root_separators(self) -> None:
        source, _ = self.fixture_module()
        consumer = self.root / "tests/consumer.cmake"
        consumer.parent.mkdir(parents=True)
        consumer.write_bytes(
            b"${CMAKE_SOURCE_DIR}/zr_vm_demo/src/zr_vm_demo/demo.c\n"
            b"E:\\Git\\zr_vm\\zr_vm_demo\\src\\zr_vm_demo\\demo.c\n"
        )
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        code, output = self.run_tool("--apply")
        self.assertEqual(code, 0, output)
        self.assertEqual(
            consumer.read_bytes(),
            b"${CMAKE_SOURCE_DIR}/zr_vm_demo/src/zr_vm_demo/group/demo.c\n"
            b"E:\\Git\\zr_vm\\zr_vm_demo\\src\\zr_vm_demo\\group\\demo.c\n",
        )

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_path_rewriter_keeps_mapped_full_destination_idempotent [function]
    #
    # ## Summary
    # 验证新目标保护 trie 避免再次迁移已有完整目的。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。构造跨嵌套 AOT 到 Parser 的单文件/目录规则，不建对应源文件。 旧完整路径改为新名；新完整名、加 CMAKE_SOURCE_DIR 前缀的新名、新目录末斜杠均保持。只验证这些字节向量。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:466
    # - tests/tooling/test_layout_moves.py:475
    # - tests/tooling/test_layout_moves.py:479
    # - tests/tooling/test_layout_moves.py:480
    # - tests/tooling/test_layout_moves.py:481
    # - tests/tooling/test_layout_moves.py:484
    # - tests/tooling/test_layout_moves.py:485
    # - scripts/layout/apply_moves.py:1076
    # - scripts/layout/apply_moves.py:1083
    # - scripts/layout/apply_moves.py:1093
    # - scripts/layout/apply_moves.py:1269
    def test_path_rewriter_keeps_mapped_full_destination_idempotent(self) -> None:
        move = migration.Move(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h",
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h",
        )
        dirs = ((
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot",
            "zr_vm_parser/src/zr_vm_parser/backend_aot",
        ),)
        rewriter = migration.path_rewriter(
            str(self.root.resolve()), ((move.old, move.new),), dirs)
        old_path = move.old.encode()
        new_path = move.new.encode()
        self.assertEqual(rewriter.replace(old_path), new_path)
        self.assertEqual(rewriter.replace(new_path), new_path)
        self.assertEqual(
            rewriter.replace(b"${CMAKE_SOURCE_DIR}/" + new_path),
            b"${CMAKE_SOURCE_DIR}/" + new_path)
        new_dir = b"zr_vm_parser/src/zr_vm_parser/backend_aot/"
        self.assertEqual(rewriter.replace(new_dir), new_dir)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_existing_unmapped_nested_path_is_not_rewritten_by_suffix_alias [function]
    #
    # ## Summary
    # 保留真实存在但未映射的嵌套文件 token，同时改写精确源名。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。建立 nested 外来样式人工文件和真正映射文件，分别输入相对/绝对 nested token。 两个 nested token 保持，旧完整源变为目的，nested 文件内容不变；不证明所有带空格/Unicode/链接路径。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:515
    # - tests/tooling/test_layout_moves.py:521
    # - tests/tooling/test_layout_moves.py:528
    # - tests/tooling/test_layout_moves.py:532
    # - tests/tooling/test_layout_moves.py:533
    # - tests/tooling/test_layout_moves.py:535
    # - tests/tooling/test_layout_moves.py:539
    # - scripts/layout/apply_moves.py:1320
    # - scripts/layout/apply_moves.py:1326
    # - scripts/layout/apply_moves.py:1357
    # - scripts/layout/apply_moves.py:1413
    # - scripts/layout/apply_moves.py:1416
    # - scripts/layout/apply_moves.py:1431
    def test_existing_unmapped_nested_path_is_not_rewritten_by_suffix_alias(self) -> None:
        move = migration.Move(
            "zr_vm_library/src/zr_vm_library/aot_runtime.c",
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c",
        )
        nested = "zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c"
        existing = self.root / nested
        existing.parent.mkdir(parents=True)
        existing.write_bytes(b"/* foreign nested source */\n")
        mapped_source = self.root / move.old
        mapped_source.parent.mkdir(parents=True, exist_ok=True)
        mapped_source.write_bytes(b"/* mapped source */\n")

        self.assertEqual(
            migration.replace_path_tokens(nested.encode(), [move], self.root, ()),
            nested.encode(),
        )
        absolute = str(existing.resolve()).encode()
        self.assertEqual(
            migration.replace_path_tokens(absolute, [move], self.root, ()), absolute)
        self.assertEqual(
            migration.replace_path_tokens(move.old.encode(), [move], self.root, ()),
            move.new.encode(),
        )
        self.assertEqual(existing.read_bytes(), b"/* foreign nested source */\n")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_path_rewriter_still_rewrites_short_legacy_directory_aliases [function]
    #
    # ## Summary
    # 验证目标保护规则不禁止允许的旧短别名和目录后缀。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。使用与前例相同结构的文件/目录映射。 短 src/... header 与旧目录下 x.c 被改成预期完整目的。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:564
    # - tests/tooling/test_layout_moves.py:573
    # - tests/tooling/test_layout_moves.py:575
    # - tests/tooling/test_layout_moves.py:578
    # - scripts/layout/apply_moves.py:1076
    # - scripts/layout/apply_moves.py:1086
    # - scripts/layout/apply_moves.py:1229
    # - scripts/layout/apply_moves.py:1278
    def test_path_rewriter_still_rewrites_short_legacy_directory_aliases(self) -> None:
        move = migration.Move(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h",
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h",
        )
        dirs = ((
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot",
            "zr_vm_parser/src/zr_vm_parser/backend_aot",
        ),)
        rewriter = migration.path_rewriter(
            str(self.root.resolve()), ((move.old, move.new),), dirs)
        self.assertEqual(
            rewriter.replace(b"src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h"),
            move.new.encode())
        self.assertEqual(
            rewriter.replace(b"zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/x.c"),
            b"zr_vm_parser/src/zr_vm_parser/backend_aot/x.c")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_precomputed_rewriter_matches_standalone_rewrite_bytes [function]
    #
    # ## Summary
    # 确保预计算规则与一次性计算在同输入上产生相同结果。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。人工头文件、quoted include、完整路径文本及显式 context；分别传/不传 compiled/candidates。 只断言 prepared==ordinary，不独立断言每个输出字段或所有输入等价。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:607
    # - tests/tooling/test_layout_moves.py:615
    # - tests/tooling/test_layout_moves.py:618
    # - tests/tooling/test_layout_moves.py:622
    # - tests/tooling/test_layout_moves.py:624
    # - tests/tooling/test_layout_moves.py:627
    # - scripts/layout/apply_moves.py:1515
    # - scripts/layout/apply_moves.py:1522
    # - scripts/layout/apply_moves.py:1524
    # - scripts/layout/apply_moves.py:1548
    def test_precomputed_rewriter_matches_standalone_rewrite_bytes(self) -> None:
        move = migration.Move(
            "zr_vm_demo/src/zr_vm_demo/demo_internal.h",
            "zr_vm_demo/src/zr_vm_demo/group/demo_internal.h",
        )
        old_header = self.root / move.old
        old_header.parent.mkdir(parents=True)
        old_header.write_bytes(b"/* fixture */\n")
        data = (b'#include "demo_internal.h"\n'
                b'${CMAKE_SOURCE_DIR}/zr_vm_demo/src/zr_vm_demo/demo_internal.h\n')
        pairs = ((move.old, move.new),)
        dir_pairs = migration.directory_aliases(str(self.root.resolve()), pairs)
        compiled = migration.path_rewriter(str(self.root.resolve()), pairs, dir_pairs)
        candidates = migration.include_candidate_names([move])
        context = {"tests/consumer.c": (old_header.parent,)}
        ordinary = migration.rewrite_bytes(data, "tests/consumer.c", [move],
                                            {move.old: move.new}, self.root, dir_pairs, context)
        prepared = migration.rewrite_bytes(data, "tests/consumer.c", [move],
                                           {move.old: move.new}, self.root, dir_pairs, context,
                                           compiled_rewriter=compiled, candidates=candidates)
        self.assertEqual(prepared, ordinary)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_full_backslash_public_path_keeps_full_private_destination [function]
    #
    # ## Summary
    # 区分完整公共路径与短公共 include 降级后的目的拼写。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。构造公共头到私有 aot 子目录的 Move 并验证 aliases。 完整反斜杠路径保留完整私有目的；短公共名改成私有 suffix。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:655
    # - tests/tooling/test_layout_moves.py:656
    # - tests/tooling/test_layout_moves.py:660
    # - tests/tooling/test_layout_moves.py:661
    # - tests/tooling/test_layout_moves.py:663
    # - tests/tooling/test_layout_moves.py:667
    # - scripts/layout/apply_moves.py:950
    # - scripts/layout/apply_moves.py:951
    # - scripts/layout/apply_moves.py:953
    # - scripts/layout/apply_moves.py:956
    # - scripts/layout/apply_moves.py:958
    def test_full_backslash_public_path_keeps_full_private_destination(self) -> None:
        move = migration.Move(
            "zr_vm_demo/include/zr_vm_demo/aot_ir_owned.h",
            "zr_vm_demo/src/zr_vm_demo/aot/aot_ir_owned.h",
        )
        migration.validate_move_aliases([move])
        rewriter = migration.path_rewriter(str(self.root.resolve()), ((move.old, move.new),), ())
        full = b"zr_vm_demo\\include\\zr_vm_demo\\aot_ir_owned.h"
        self.assertEqual(
            rewriter.replace(full),
            b"zr_vm_demo\\src\\zr_vm_demo\\aot\\aot_ir_owned.h",
        )
        self.assertEqual(
            rewriter.replace(b"zr_vm_demo/aot_ir_owned.h"),
            b"aot/aot_ir_owned.h",
        )

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_mapped_full_and_local_private_include_aliases_are_context_resolved [function]
    #
    # ## Summary
    # 验证多种完整/本地私有 include 根据原上下文解析后重写。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。四种 Core include 拼写在 subTest 中处理，再处理独立 Parser 局部私有名。 每个 subTest 断言新的 function 私有拼写；默认非 failfast 时，subTest 失败可记录后继续后续拼写；启用 failfast 时可中止方法。最终 Parser 完整 bytes 断言也只在到达时证明该向量。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。 此方法的 subTest 在默认非 failfast 时允许单个拼写失败后继续；启用 failfast 时可中止方法，其它普通 assert 失败通常中断当前方法。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:699
    # - tests/tooling/test_layout_moves.py:712
    # - tests/tooling/test_layout_moves.py:718
    # - tests/tooling/test_layout_moves.py:719
    # - tests/tooling/test_layout_moves.py:723
    # - tests/tooling/test_layout_moves.py:735
    # - tests/tooling/test_layout_moves.py:739
    # - scripts/layout/apply_moves.py:737
    # - scripts/layout/apply_moves.py:763
    # - scripts/layout/apply_moves.py:856
    # - scripts/layout/apply_moves.py:872
    # - scripts/layout/apply_moves.py:1586
    def test_mapped_full_and_local_private_include_aliases_are_context_resolved(self) -> None:
        move = migration.Move(
            "zr_vm_core/src/zr_vm_core/function_precall_internal.h",
            "zr_vm_core/src/zr_vm_core/function/function_precall_internal.h",
        )
        header = self.root / move.old
        header.parent.mkdir(parents=True)
        header.write_bytes(b"/* private fixture */\n")
        consumer = self.root / "tests/core/test_frame_slot_layout_lookup.c"
        consumer.parent.mkdir(parents=True)
        roots = (self.root, self.root / "zr_vm_core")
        context = {"tests/core/test_frame_slot_layout_lookup.c": roots}
        move_map = {move.old: move.new}
        for spelling in (
            "zr_vm_core/src/zr_vm_core/function_precall_internal.h",
            "src/zr_vm_core/function_precall_internal.h",
            "zr_vm_core\\src\\zr_vm_core\\function_precall_internal.h",
            "src\\zr_vm_core\\function_precall_internal.h",
        ):
            with self.subTest(spelling=spelling):
                rewritten = migration.rewrite_bytes(
                    f'#include "{spelling}"\n'.encode(),
                    "tests/core/test_frame_slot_layout_lookup.c", [move], move_map,
                    self.root, context=context)
                self.assertIn(
                    b'#include "function\\function_precall_internal.h"' if "\\" in spelling
                    else b'#include "function/function_precall_internal.h"', rewritten)
        parser_move = migration.Move(
            "zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.h",
            "zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_compare.h",
        )
        parser_header = self.root / parser_move.old
        parser_header.parent.mkdir(parents=True)
        parser_header.write_bytes(b"/* parser private fixture */\n")
        parser_consumer = "tests/parser/test_ssa_compare_metadata_guards.c"
        parser_context = {parser_consumer: (self.root / "zr_vm_parser/src/zr_vm_parser",)}
        rewritten = migration.rewrite_bytes(
            b'#include "compiler/compiler_semantic_compare.h"\n', parser_consumer,
            [parser_move], {parser_move.old: parser_move.new}, self.root,
            context=parser_context)
        self.assertEqual(rewritten, b'#include "compiler/semantic/compiler_semantic_compare.h"\n')

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_dormant_fallback_order_precedes_module_heuristics [function]
    #
    # ## Summary
    # 证明显式 dormant 变体优先于模块启发式根。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。三个不同根都有 choice.h，fallback 按 first/second 排序。 结果为第一个 fallback 文件身份；不合并所有根来制造歧义。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:766
    # - tests/tooling/test_layout_moves.py:773
    # - tests/tooling/test_layout_moves.py:776
    # - tests/tooling/test_layout_moves.py:777
    # - tests/tooling/test_layout_moves.py:778
    # - scripts/layout/apply_moves.py:744
    # - scripts/layout/apply_moves.py:745
    # - scripts/layout/apply_moves.py:750
    # - scripts/layout/apply_moves.py:755
    # - scripts/layout/apply_moves.py:766
    def test_dormant_fallback_order_precedes_module_heuristics(self) -> None:
        including = "zr_vm_demo/src/zr_vm_demo/nested/consumer.c"
        source = self.root / including
        source.parent.mkdir(parents=True)
        first = self.root / "tests/harness"
        second = self.root / "tests/extra"
        heuristic = self.root / "zr_vm_demo/src/zr_vm_demo"
        for directory in (first, second, heuristic):
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "choice.h").write_bytes((str(directory) + "\n").encode())
        fallback = {including: ((first, second),)}
        resolved = migration.resolve_include(self.root, including, "choice.h", {}, {}, fallback)
        self.assertEqual(resolved, ("tests/harness/choice.h", "tests/harness/choice.h"))

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_nested_historical_paths_are_untouched_by_apply_and_check [function]
    #
    # ## Summary
    # 保留嵌套历史 docs 和 acceptance 中的旧路径样本。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。建立两份历史常量文件，依次预览/应用/检查 Demo 映射。 三阶段返回0且历史 bytes 均相同；只覆盖给出的两种路径。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:807
    # - tests/tooling/test_layout_moves.py:810
    # - tests/tooling/test_layout_moves.py:817
    # - tests/tooling/test_layout_moves.py:819
    # - tests/tooling/test_layout_moves.py:822
    # - tests/tooling/test_layout_moves.py:825
    # - tests/tooling/test_layout_moves.py:827
    # - scripts/layout/apply_moves.py:435
    # - scripts/layout/apply_moves.py:438
    # - scripts/layout/apply_moves.py:439
    # - scripts/layout/apply_moves.py:505
    # - scripts/layout/apply_moves.py:2693
    def test_nested_historical_paths_are_untouched_by_apply_and_check(self) -> None:
        source, _ = self.fixture_module()
        old = b"zr_vm_demo/src/zr_vm_demo/demo.c\n"
        history = (
            self.root / "docs/superpowers/plans/old-plan.md",
            self.root / "zr_vm_aot/tests/acceptance/old-case.c",
        )
        for path in history:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(old)
        before = [path.read_bytes() for path in history]
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        code, output = self.run_tool("--dry-run")
        self.assertEqual(code, 0, output)
        self.assertEqual([path.read_bytes() for path in history], before)
        code, output = self.run_tool("--apply")
        self.assertEqual(code, 0, output)
        self.assertEqual([path.read_bytes() for path in history], before)
        code, output = self.run_tool("--check")
        self.assertEqual(code, 0, output)
        self.assertEqual([path.read_bytes() for path in history], before)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_docs_at_any_depth_are_untouched_by_default_preview_apply_and_check [function]
    #
    # ## Summary
    # 验证顶层/产品嵌套/测试夹具 docs 的一致排除。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。三份 docs 常量内容与 Demo 映射，依次调用三阶段。 各阶段返回0、docs dict 全同且最终新源存在；不把 excluded scans 结论外推所有仓内数据。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:858
    # - tests/tooling/test_layout_moves.py:861
    # - tests/tooling/test_layout_moves.py:869
    # - tests/tooling/test_layout_moves.py:872
    # - tests/tooling/test_layout_moves.py:875
    # - tests/tooling/test_layout_moves.py:876
    # - tests/tooling/test_layout_moves.py:878
    # - tests/tooling/test_layout_moves.py:879
    # - tests/tooling/test_layout_moves.py:881
    # - tests/tooling/test_layout_moves.py:882
    # - scripts/layout/apply_moves.py:435
    # - scripts/layout/apply_moves.py:438
    # - scripts/layout/apply_moves.py:441
    # - scripts/layout/apply_moves.py:505
    def test_docs_at_any_depth_are_untouched_by_default_preview_apply_and_check(self) -> None:
        source, _ = self.fixture_module()
        old = b"zr_vm_demo/src/zr_vm_demo/demo.c\n"
        documentation = (
            self.root / "docs/overview.md",
            self.root / "zr_vm_demo/docs/api/reference.md",
            self.root / "tests/fixtures/docs/expected.txt",
        )
        for path in documentation:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(old)
        before = {path: path.read_bytes() for path in documentation}
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")

        code, output = self.run_tool("--dry-run")
        self.assertEqual(code, 0, output)
        self.assertIn("preview:", output)
        self.assertEqual({path: path.read_bytes() for path in documentation}, before)
        code, output = self.run_tool("--apply")
        self.assertEqual(code, 0, output)
        self.assertEqual({path: path.read_bytes() for path in documentation}, before)
        code, output = self.run_tool("--check")
        self.assertEqual(code, 0, output)
        self.assertEqual({path: path.read_bytes() for path in documentation}, before)
        self.assertTrue((self.root / "zr_vm_demo/src/zr_vm_demo/group/demo.c").is_file())

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_move_table_cannot_target_docs_or_itself [function]
    #
    # ## Summary
    # 校验表中部分保护目标被拒绝，并区分真正触发的原因。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。三轮分别 docs 改名、同basename移入docs、源移向layout_map.tsv。 第一/第三因 basename 不保留在保护区检查前拒绝；第二确到 docs 保护规则。只 assertRaises，不证明每轮到达用例名暗示的保护分支。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:908
    # - tests/tooling/test_layout_moves.py:910
    # - tests/tooling/test_layout_moves.py:915
    # - tests/tooling/test_layout_moves.py:916
    # - tests/tooling/test_layout_moves.py:917
    # - scripts/layout/apply_moves.py:390
    # - scripts/layout/apply_moves.py:391
    # - scripts/layout/apply_moves.py:396
    # - scripts/layout/apply_moves.py:397
    def test_move_table_cannot_target_docs_or_itself(self) -> None:
        table = self.root / "unsafe.tsv"
        for body in (
            "zr_vm_demo/docs/api.md\tzr_vm_demo/docs/api-new.md\n",
            "zr_vm_demo/src/zr_vm_demo/demo.c\tdocs/demo.c\n",
            "zr_vm_demo/src/zr_vm_demo/demo.c\tscripts/layout/layout_map.tsv\n",
        ):
            table.write_text(body, encoding="utf-8")
            with self.assertRaises(migration.MigrationError):
                migration.load_moves(table)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_empty_private_directory_suffix_does_not_create_module_short_alias [function]
    #
    # ## Summary
    # 避免把整个私有根虚构成更短模块前缀别名。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。传空文件对及从私有根到重复嵌套根的目录对。 给定 module-qualified token 保持，不验证其磁盘存在性。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:942
    # - tests/tooling/test_layout_moves.py:943
    # - tests/tooling/test_layout_moves.py:945
    # - tests/tooling/test_layout_moves.py:947
    # - tests/tooling/test_layout_moves.py:948
    # - scripts/layout/apply_moves.py:1076
    # - scripts/layout/apply_moves.py:1086
    # - scripts/layout/apply_moves.py:1088
    def test_empty_private_directory_suffix_does_not_create_module_short_alias(self) -> None:
        old_root = "zr_vm_parser/src/zr_vm_parser"
        nested_root = old_root + "/zr_vm_parser/src/zr_vm_parser"
        rewriter = migration.path_rewriter(
            str(self.root.resolve()), (), ((old_root, nested_root),))
        token = b"zr_vm_parser/compiler/compiler_internal.h"
        self.assertEqual(rewriter.replace(token), token)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_aot_directory_alias_preserves_parent_path_without_duplicate_module [function]
    #
    # ## Summary
    # 保留路径前的父相对组件，同时只改旧 AOT 子串。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。输入 CMAKE_SOURCE_DIR/Parser/../AOT backend_aot 文本。 完整 bytes 等于预期，随后 normpath 为正确 Parser backend路径；normpath 是纯词法，不运行CMake。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:975
    # - tests/tooling/test_layout_moves.py:979
    # - tests/tooling/test_layout_moves.py:981
    # - tests/tooling/test_layout_moves.py:982
    # - tests/tooling/test_layout_moves.py:986
    # - tests/tooling/test_layout_moves.py:988
    # - scripts/layout/apply_moves.py:1261
    # - scripts/layout/apply_moves.py:1267
    # - scripts/layout/apply_moves.py:1278
    # - scripts/layout/apply_moves.py:1320
    def test_aot_directory_alias_preserves_parent_path_without_duplicate_module(self) -> None:
        old = "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot"
        new = "zr_vm_parser/src/zr_vm_parser/backend_aot"
        move = migration.Move(old + "/backend_aot.c", new + "/backend_aot.c")
        raw = (b'${CMAKE_SOURCE_DIR}/zr_vm_parser/../'
               b'zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot\n')
        rewritten = migration.replace_path_tokens(raw, [move], self.root, ((old, new),))
        self.assertEqual(
            rewritten,
            b'${CMAKE_SOURCE_DIR}/zr_vm_parser/../'
            b'zr_vm_parser/src/zr_vm_parser/backend_aot\n')
        normalized = migration.os.path.normpath(
            rewritten.decode().strip().replace("${CMAKE_SOURCE_DIR}/", ""))
        self.assertEqual(normalized.replace("\\", "/"), "zr_vm_parser/src/zr_vm_parser/backend_aot")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_legacy_include_resolves_before_nested_directory_alias_rewrite [function]
    #
    # ## Summary
    # 证明旧父相对 include 先解析，避免被普通目录别名污染。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。建立未移动 Parser 私有头与 ../../ include；另传无关文件 Move 和会重复嵌套的目录别名。 include 变成 compiler/...；注释中真实未映射完整路径保留；错误重复路径不存在。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1017
    # - tests/tooling/test_layout_moves.py:1018
    # - tests/tooling/test_layout_moves.py:1023
    # - tests/tooling/test_layout_moves.py:1034
    # - tests/tooling/test_layout_moves.py:1039
    # - tests/tooling/test_layout_moves.py:1040
    # - tests/tooling/test_layout_moves.py:1042
    # - scripts/layout/apply_moves.py:1575
    # - scripts/layout/apply_moves.py:1581
    # - scripts/layout/apply_moves.py:1586
    # - scripts/layout/apply_moves.py:1593
    # - scripts/layout/apply_moves.py:1597
    def test_legacy_include_resolves_before_nested_directory_alias_rewrite(self) -> None:
        header = self.root / "zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"
        header.parent.mkdir(parents=True)
        header.write_bytes(b"/* unmoved parser private header */\n")
        consumer = self.root / "tests/compileTime/test_compile_time_execution.c"
        consumer.parent.mkdir(parents=True)
        original = (
            b'#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"\n'
            b'/* zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h */\n'
        )
        consumer.write_bytes(original)
        old_root = "zr_vm_parser/src/zr_vm_parser"
        nested_root = old_root + "/zr_vm_parser/src/zr_vm_parser"
        move = migration.Move(
            old_root + "/backend_aot/aot_source.c",
            old_root + "/backend_aot/group/aot_source.c",
        )
        rewritten = migration.rewrite_bytes(
            original, "tests/compileTime/test_compile_time_execution.c", [move],
            {move.old: move.new}, self.root,
            ((old_root, nested_root),),
        )
        self.assertIn(b'#include "compiler/compiler_internal.h"', rewritten)
        self.assertIn(
            (old_root + "/compiler/compiler_internal.h").encode(), rewritten)
        self.assertNotIn(
            b'#include "zr_vm_parser/src/zr_vm_parser/zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"',
            rewritten,
        )

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_rewrites_only_fully_mapped_directory_prefixes [function]
    #
    # ## Summary
    # 检验目录整体别名必须覆盖所有可枚举后代。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。目录含 a.c/b.h 且均同结构移动，再追加 stay.inc。 首次 CMake 路径进入改写字典并有 group；追加后该 CMake 路径不在字典。未模拟不可读目录/并发新增。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1077
    # - tests/tooling/test_layout_moves.py:1080
    # - tests/tooling/test_layout_moves.py:1085
    # - tests/tooling/test_layout_moves.py:1089
    # - tests/tooling/test_layout_moves.py:1090
    # - tests/tooling/test_layout_moves.py:1091
    # - tests/tooling/test_layout_moves.py:1092
    # - tests/tooling/test_layout_moves.py:1096
    # - tests/tooling/test_layout_moves.py:1097
    # - tests/tooling/test_layout_moves.py:1098
    # - scripts/layout/apply_moves.py:1014
    # - scripts/layout/apply_moves.py:1037
    # - scripts/layout/apply_moves.py:1038
    # - scripts/layout/apply_moves.py:1039
    # - scripts/layout/apply_moves.py:1042
    def test_rewrites_only_fully_mapped_directory_prefixes(self) -> None:
        old_dir = self.root / "zr_vm_demo/src/zr_vm_demo/backend_aot"
        old_dir.mkdir(parents=True)
        for name in ("a.c", "b.h"):
            (old_dir / name).write_bytes(b"/* fixture */\n")
        cmake = self.root / "tests/paths.cmake"
        cmake.parent.mkdir(parents=True)
        cmake.write_text("${CMAKE_SOURCE_DIR}/zr_vm_demo/src/zr_vm_demo/backend_aot\n", encoding="utf-8")
        self.mapping(
            "zr_vm_demo/src/zr_vm_demo/backend_aot/a.c\tzr_vm_demo/src/zr_vm_demo/backend_aot/group/a.c\n"
            "zr_vm_demo/src/zr_vm_demo/backend_aot/b.h\tzr_vm_demo/src/zr_vm_demo/backend_aot/group/b.h\n"
        )
        moves = migration.load_moves(self.root / "scripts/layout/layout_map.tsv")
        rewrites = migration.scan_rewrites(self.root, moves)
        self.assertIn("tests/paths.cmake", rewrites)
        self.assertIn(b"backend_aot/group", rewrites["tests/paths.cmake"][1])

        # An unmapped descendant invalidates the directory alias, while exact
        # mapped file paths remain independently eligible for rewriting.
        (old_dir / "stay.inc").write_bytes(b"/* retained */\n")
        rewrites = migration.scan_rewrites(self.root, moves)
        self.assertNotIn("tests/paths.cmake", rewrites)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_apply_rebases_includes_and_preserves_bom_and_line_endings [function]
    #
    # ## Summary
    # 在真实夹具 rename/改写后比较 BOM、CRLF 与头 bytes。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。源/头一起移入 group，源含 BOM、CRLF 与旧路径注释。 目的源完整 bytes 符合两处替换、目的头同原文、旧两路径不存在；再预览返回 already moved。断言若早失败，后续重入未必到达。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1132
    # - tests/tooling/test_layout_moves.py:1134
    # - tests/tooling/test_layout_moves.py:1135
    # - tests/tooling/test_layout_moves.py:1137
    # - tests/tooling/test_layout_moves.py:1141
    # - tests/tooling/test_layout_moves.py:1145
    # - tests/tooling/test_layout_moves.py:1146
    # - tests/tooling/test_layout_moves.py:1147
    # - tests/tooling/test_layout_moves.py:1148
    # - tests/tooling/test_layout_moves.py:1149
    # - tests/tooling/test_layout_moves.py:1151
    # - scripts/layout/apply_moves.py:1515
    # - scripts/layout/apply_moves.py:1593
    # - scripts/layout/apply_moves.py:1969
    # - scripts/layout/apply_moves.py:1999
    # - scripts/layout/apply_moves.py:2009
    # - scripts/layout/apply_moves.py:2709
    def test_apply_rebases_includes_and_preserves_bom_and_line_endings(self) -> None:
        source, header = self.fixture_module()
        header_bytes = header.read_bytes()
        expected = source.read_bytes().replace(b'"demo_internal.h"', b'"group/demo_internal.h"')
        expected = expected.replace(b"zr_vm_demo/src/zr_vm_demo/demo.c", b"zr_vm_demo/src/zr_vm_demo/group/demo.c")
        self.mapping(
            "zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n"
            "zr_vm_demo/src/zr_vm_demo/demo_internal.h\tzr_vm_demo/src/zr_vm_demo/group/demo_internal.h\n"
        )
        code, output = self.run_tool("--apply")
        self.assertEqual(code, 0, output)
        new_source = self.root / "zr_vm_demo/src/zr_vm_demo/group/demo.c"
        new_header = self.root / "zr_vm_demo/src/zr_vm_demo/group/demo_internal.h"
        self.assertEqual(new_source.read_bytes(), expected)
        self.assertEqual(new_header.read_bytes(), header_bytes)
        self.assertFalse(source.exists())
        self.assertFalse(header.exists())
        code, output = self.run_tool("--dry-run")
        self.assertEqual(code, 0, output)
        self.assertIn("already moved", output)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_target_collision_refuses_without_mutation [function]
    #
    # ## Summary
    # 在任何改名之前拒绝已有目的，保留两边内容。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。目的写 keep，保存源 bytes 后调用 --apply。 main 返回2且诊断含实际断言文本 destination already exists；源/已有目的 bytes 均保留且未发生覆盖，首次拒绝为 preflight target.exists，不是回滚分支。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1179
    # - tests/tooling/test_layout_moves.py:1183
    # - tests/tooling/test_layout_moves.py:1186
    # - tests/tooling/test_layout_moves.py:1187
    # - tests/tooling/test_layout_moves.py:1188
    # - tests/tooling/test_layout_moves.py:1189
    # - tests/tooling/test_layout_moves.py:1190
    # - scripts/layout/apply_moves.py:1737
    # - scripts/layout/apply_moves.py:1738
    # - scripts/layout/apply_moves.py:2701
    # - scripts/layout/apply_moves.py:2745
    def test_target_collision_refuses_without_mutation(self) -> None:
        source, _ = self.fixture_module()
        target = self.root / "zr_vm_demo/src/zr_vm_demo/group/demo.c"
        target.parent.mkdir()
        target.write_bytes(b"keep")
        original = source.read_bytes()
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        code, output = self.run_tool("--apply")
        self.assertEqual(code, 2)
        self.assertIn("destination already exists", output)
        self.assertEqual(source.read_bytes(), original)
        self.assertEqual(target.read_bytes(), b"keep")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_pinned_plan_rejects_stale_rewrite_input [function]
    #
    # ## Summary
    # 拒绝预览计划之后已改变的源快照。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。先 --plan 写夹具JSON，再附加源注释后 --apply --plan。 返回2且含 stale，源仍在/新目的不存在。证明计划结构不等被拒绝，不逐字段断言哪一个 hash 首先不同。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1220
    # - tests/tooling/test_layout_moves.py:1223
    # - tests/tooling/test_layout_moves.py:1224
    # - tests/tooling/test_layout_moves.py:1226
    # - tests/tooling/test_layout_moves.py:1227
    # - tests/tooling/test_layout_moves.py:1228
    # - tests/tooling/test_layout_moves.py:1229
    # - tests/tooling/test_layout_moves.py:1230
    # - tests/tooling/test_layout_moves.py:1231
    # - scripts/layout/apply_moves.py:1779
    # - scripts/layout/apply_moves.py:1818
    # - scripts/layout/apply_moves.py:1823
    # - scripts/layout/apply_moves.py:2718
    def test_pinned_plan_rejects_stale_rewrite_input(self) -> None:
        source, _ = self.fixture_module()
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        plan = self.root / "plan.json"
        code, output = self.run_tool("--plan", str(plan))
        self.assertEqual(code, 0, output)
        source.write_bytes(source.read_bytes() + b"/* changed */\n")
        code, output = self.run_tool("--apply", "--plan", str(plan))
        self.assertEqual(code, 2)
        self.assertIn("stale", output)
        self.assertTrue(source.exists())
        self.assertFalse((self.root / "zr_vm_demo/src/zr_vm_demo/group/demo.c").exists())

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_pinned_plan_accepts_exact_already_applied_state [function]
    #
    # ## Summary
    # 验证已完全迁移树可凭原计划重入而不再改名。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。写计划、首次应用，再以同一计划应用。 首次后旧源不存在，第二次返回0且 already migrated；前面的普通断言失败时，后续路径不会由该次方法执行到。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1262
    # - tests/tooling/test_layout_moves.py:1265
    # - tests/tooling/test_layout_moves.py:1266
    # - tests/tooling/test_layout_moves.py:1268
    # - tests/tooling/test_layout_moves.py:1269
    # - tests/tooling/test_layout_moves.py:1270
    # - tests/tooling/test_layout_moves.py:1271
    # - tests/tooling/test_layout_moves.py:1272
    # - tests/tooling/test_layout_moves.py:1273
    # - scripts/layout/apply_moves.py:1742
    # - scripts/layout/apply_moves.py:1856
    # - scripts/layout/apply_moves.py:2709
    # - scripts/layout/apply_moves.py:2712
    # - scripts/layout/apply_moves.py:2714
    def test_pinned_plan_accepts_exact_already_applied_state(self) -> None:
        source, _ = self.fixture_module()
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        plan = self.root / "plan.json"
        code, output = self.run_tool("--plan", str(plan))
        self.assertEqual(code, 0, output)
        code, output = self.run_tool("--apply", "--plan", str(plan))
        self.assertEqual(code, 0, output)
        self.assertFalse(source.exists())
        code, output = self.run_tool("--apply", "--plan", str(plan))
        self.assertEqual(code, 0, output)
        self.assertIn("already migrated", output)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_ambiguous_cross_module_private_include_is_refused [function]
    #
    # ## Summary
    # 拒绝两个实际变体解析为不同私有头。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。两个模块都有 common.h，消费者context给两个独立根变体。 只要求跨 compile contexts 的 MigrationError；不会执行 C编译或证明启发式多命中分支。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1301
    # - tests/tooling/test_layout_moves.py:1302
    # - tests/tooling/test_layout_moves.py:1303
    # - tests/tooling/test_layout_moves.py:1313
    # - tests/tooling/test_layout_moves.py:1314
    # - tests/tooling/test_layout_moves.py:1315
    # - scripts/layout/apply_moves.py:744
    # - scripts/layout/apply_moves.py:752
    # - scripts/layout/apply_moves.py:763
    # - scripts/layout/apply_moves.py:768
    # - scripts/layout/apply_moves.py:769
    def test_ambiguous_cross_module_private_include_is_refused(self) -> None:
        include_a = self.root / "zr_vm_one/src/zr_vm_one/common.h"
        include_b = self.root / "zr_vm_two/src/zr_vm_two/common.h"
        include_a.parent.mkdir(parents=True)
        include_b.parent.mkdir(parents=True)
        include_a.write_bytes(b"#define ONE 1\n")
        include_b.write_bytes(b"#define TWO 2\n")
        source, _ = self.fixture_module()
        consumer = self.root / "tests/consumer.c"
        consumer.parent.mkdir(parents=True)
        consumer.write_bytes(b'#include "common.h"\n')
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        context = {"tests/consumer.c": ((include_a.parent,), (include_b.parent,))}
        with self.assertRaisesRegex(migration.MigrationError, "resolves differently across compile contexts"):
            migration.resolve_include(self.root, "tests/consumer.c", "common.h", {}, context)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_nested_bundle_public_include_uses_its_corresponding_module_root [function]
    #
    # ## Summary
    # 在无编译上下文时让保留嵌套模块公共名使用相邻根。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。嵌套与顶层库都有同名公共头。 结果为嵌套头旧/新同身份；仅该启发式向量。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1344
    # - tests/tooling/test_layout_moves.py:1345
    # - tests/tooling/test_layout_moves.py:1346
    # - tests/tooling/test_layout_moves.py:1347
    # - tests/tooling/test_layout_moves.py:1353
    # - tests/tooling/test_layout_moves.py:1354
    # - tests/tooling/test_layout_moves.py:1355
    # - scripts/layout/apply_moves.py:641
    # - scripts/layout/apply_moves.py:658
    # - scripts/layout/apply_moves.py:664
    # - scripts/layout/apply_moves.py:665
    # - scripts/layout/apply_moves.py:737
    def test_nested_bundle_public_include_uses_its_corresponding_module_root(self) -> None:
        including = "zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c"
        nested = self.root / "zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h"
        repository = self.root / "zr_vm_library/include/zr_vm_library/aot_runtime.h"
        nested.parent.mkdir(parents=True)
        repository.parent.mkdir(parents=True)
        nested.write_bytes(b"/* bundled header */\n")
        repository.write_bytes(b"/* production header */\n")

        self.assertEqual(
            migration.resolve_include(self.root, including, "zr_vm_library/aot_runtime.h", {}, {}),
            ("zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h",
             "zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h"),
        )

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_nested_bundle_does_not_override_ambiguous_actual_compile_contexts [function]
    #
    # ## Summary
    # 保证嵌套启发式不能覆盖真实多变体歧义。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。同名嵌套/顶层公共头与两个显式compile variants。 assertRaisesRegex 观察跨变体歧义，不把嵌套优先用到真实上下文。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1383
    # - tests/tooling/test_layout_moves.py:1391
    # - tests/tooling/test_layout_moves.py:1393
    # - tests/tooling/test_layout_moves.py:1395
    # - tests/tooling/test_layout_moves.py:1396
    # - scripts/layout/apply_moves.py:744
    # - scripts/layout/apply_moves.py:746
    # - scripts/layout/apply_moves.py:750
    # - scripts/layout/apply_moves.py:769
    def test_nested_bundle_does_not_override_ambiguous_actual_compile_contexts(self) -> None:
        including = "zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c"
        nested = self.root / "zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h"
        repository = self.root / "zr_vm_library/include/zr_vm_library/aot_runtime.h"
        nested.parent.mkdir(parents=True)
        repository.parent.mkdir(parents=True)
        nested.write_bytes(b"/* bundled header */\n")
        repository.write_bytes(b"/* production header */\n")
        context = {including: ((nested.parents[1],), (repository.parents[1],))}

        with self.assertRaisesRegex(migration.MigrationError,
                                    "resolves differently across compile contexts"):
            migration.resolve_include(
                self.root, including, "zr_vm_library/aot_runtime.h", {}, context)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_file_api_include_context_is_per_source [function]
    #
    # ## Summary
    # 读取人工 File API 后按源绑定 include 根并用于解析。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。写三JSON，仅一个 consumer 与一个compile group/sourceIndexes=[0]。 context 恰为该根元组，quoted common.h 解析到人工头；不认证完整实际CMake图。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1427
    # - tests/tooling/test_layout_moves.py:1434
    # - tests/tooling/test_layout_moves.py:1436
    # - tests/tooling/test_layout_moves.py:1439
    # - tests/tooling/test_layout_moves.py:1443
    # - tests/tooling/test_layout_moves.py:1449
    # - tests/tooling/test_layout_moves.py:1450
    # - tests/tooling/test_layout_moves.py:1452
    # - tests/tooling/test_layout_moves.py:1453
    # - scripts/layout/apply_moves.py:2437
    # - scripts/layout/apply_moves.py:2445
    # - scripts/layout/apply_moves.py:2492
    # - scripts/layout/apply_moves.py:2497
    # - scripts/layout/apply_moves.py:2508
    def test_file_api_include_context_is_per_source(self) -> None:
        header = self.root / "zr_vm_one/src/zr_vm_one/common.h"
        header.parent.mkdir(parents=True)
        header.write_bytes(b"#define ONE 1\n")
        consumer = self.root / "tests/consumer.c"
        consumer.parent.mkdir(parents=True)
        consumer.write_bytes(b'#include "common.h"\n')
        build = self.root / "build"
        reply = build / ".cmake/api/v1/reply"
        reply.mkdir(parents=True)
        (reply / "index-test.json").write_text(
            '{"objects":[{"kind":"codemodel","jsonFile":"codemodel.json"}]}', encoding="utf-8"
        )
        (reply / "codemodel.json").write_text(
            '{"paths":{"source":"' + str(self.root).replace("\\", "\\\\") + '"},'
            '"configurations":[{"targets":[{"jsonFile":"target.json"}]}]}', encoding="utf-8"
        )
        (reply / "target.json").write_text(
            '{"sources":[{"path":"tests/consumer.c"}],'
            '"compileGroups":[{"includes":[{"path":"zr_vm_one/src/zr_vm_one","isSystem":false}],'
            '"sourceIndexes":[0]}]}', encoding="utf-8"
        )
        context = migration.load_cmake_context(build, self.root)
        self.assertEqual(context["tests/consumer.c"], ((header.parent.resolve(),),))
        self.assertEqual(
            migration.resolve_include(self.root, "tests/consumer.c", "common.h", {}, context),
            ("zr_vm_one/src/zr_vm_one/common.h", "zr_vm_one/src/zr_vm_one/common.h"),
        )

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_file_api_generated_include_is_kept_and_never_read [function]
    #
    # ## Summary
    # 区分精确生成头的路径身份与内容读取。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。addCleanup 管理独立 build- 临时树，生成头含无效UTF字节，patch Path.read_bytes拦精确生成Path。 context 保留生成根，unresolved问题为空，补丁退出后生成内容 hash 未变。只阻断 read_bytes 等值Path，不代表无任何stat/别名读取。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1487
    # - tests/tooling/test_layout_moves.py:1492
    # - tests/tooling/test_layout_moves.py:1493
    # - tests/tooling/test_layout_moves.py:1496
    # - tests/tooling/test_layout_moves.py:1497
    # - tests/tooling/test_layout_moves.py:1502
    # - tests/tooling/test_layout_moves.py:1506
    # - tests/tooling/test_layout_moves.py:1529
    # - tests/tooling/test_layout_moves.py:1534
    # - tests/tooling/test_layout_moves.py:1536
    # - tests/tooling/test_layout_moves.py:1537
    # - scripts/layout/apply_moves.py:2487
    # - scripts/layout/apply_moves.py:2140
    # - scripts/layout/apply_moves.py:2141
    # - scripts/layout/apply_moves.py:2142
    # - scripts/layout/apply_moves.py:2144
    def test_file_api_generated_include_is_kept_and_never_read(self) -> None:
        source = self.root / "tests/generated_consumer.c"
        source.parent.mkdir(parents=True)
        source.write_bytes(b'#include "zr_vm_ffi_fixture_path.h"\n')
        build_temp = tempfile.TemporaryDirectory(prefix="build-", dir=FIXTURE_ROOT)
        self.addCleanup(build_temp.cleanup)
        build = Path(build_temp.name)
        generated_dir = build / "tests_generated"
        generated_dir.mkdir()
        generated_header = generated_dir / "zr_vm_ffi_fixture_path.h"
        invalid_bytes = b"\xff\xfe\x00not valid text\n"
        generated_header.write_bytes(invalid_bytes)
        original_hash = migration.sha256(invalid_bytes)
        self.write_file_api(build, source, ((generated_dir,),))

        context = migration.load_cmake_context(build, self.root)
        self.assertEqual(
            migration.context_variants(context, "tests/generated_consumer.c"),
            ((generated_dir,),),
        )
        original_read_bytes = Path.read_bytes

        # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_file_api_generated_include_is_kept_and_never_read.deny_generated_read [function]
        #
        # ## Summary
        # 拦截精确生成头 read_bytes 调用，以区分路径分类与内容读取。
        #
        # ## Parameters
        # - path: Path.read_bytes 接收的实际 Path。
        #
        # ## Returns
        # 精确命中生成头时抛 AssertionError；其它路径调用保存的原 read_bytes 并返回 bytes。
        #
        # ## Constraints and lifecycle
        # 只在 patch.object context 内替换类方法；上下文退出恢复原方法，包括内部异常路径。比较是 Path 等值，不是所有别名/链接身份认证。此函数不阻断 lstat/stat 或其它读取API。
        #
        # ## References
        # - tests/tooling/test_layout_moves.py:1529
        # - tests/tooling/test_layout_moves.py:1530
        # - tests/tooling/test_layout_moves.py:1532
        # - tests/tooling/test_layout_moves.py:1534
        # - tests/tooling/test_layout_moves.py:1537
        def deny_generated_read(path: Path) -> bytes:
            if path == generated_header:
                raise AssertionError("generated header contents must never be read")
            return original_read_bytes(path)

        with patch.object(Path, "read_bytes", deny_generated_read):
            problems = migration.unresolved_includes(self.root, context, build=build)
        self.assertEqual(problems, [])
        self.assertEqual(migration.sha256(original_read_bytes(generated_header)), original_hash)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_file_api_ordered_repository_and_generated_variants_must_agree [function]
    #
    # ## Summary
    # 拒绝真实变体分别命中仓内/生成身份，随后验证源目录一致遮蔽。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。一个repo include根和一个generated根，各有同名头；之后在包含源目录新增同名头。 先保留两个有序variants，问题恰一且含 unsafe/contexts；源目录有头后问题为空。只覆盖精确生成头名称。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1571
    # - tests/tooling/test_layout_moves.py:1581
    # - tests/tooling/test_layout_moves.py:1582
    # - tests/tooling/test_layout_moves.py:1588
    # - tests/tooling/test_layout_moves.py:1589
    # - tests/tooling/test_layout_moves.py:1590
    # - tests/tooling/test_layout_moves.py:1591
    # - tests/tooling/test_layout_moves.py:1594
    # - tests/tooling/test_layout_moves.py:1598
    # - tests/tooling/test_layout_moves.py:1599
    # - tests/tooling/test_layout_moves.py:1600
    # - scripts/layout/apply_moves.py:742
    # - scripts/layout/apply_moves.py:752
    # - scripts/layout/apply_moves.py:2140
    # - scripts/layout/apply_moves.py:2143
    # - scripts/layout/apply_moves.py:2144
    # - scripts/layout/apply_moves.py:2146
    def test_file_api_ordered_repository_and_generated_variants_must_agree(self) -> None:
        source = self.root / "tests/core/generated_consumer.c"
        source.parent.mkdir(parents=True)
        source.write_bytes(b'#include "zr_vm_ffi_fixture_path.h"\n')
        repo_include = self.root / "tests/generated-repo-include"
        repo_include.mkdir(parents=True)
        repo_header = repo_include / "zr_vm_ffi_fixture_path.h"
        repo_header.write_bytes(b"/* repository variant */\n")

        build_temp = tempfile.TemporaryDirectory(prefix="build-", dir=FIXTURE_ROOT)
        self.addCleanup(build_temp.cleanup)
        build = Path(build_temp.name)
        generated_dir = build / "tests_generated"
        generated_dir.mkdir()
        (generated_dir / "zr_vm_ffi_fixture_path.h").write_bytes(b"\xffgenerated\x00\n")
        self.write_file_api(build, source, ((repo_include,), (generated_dir,)))

        context = migration.load_cmake_context(build, self.root)
        variants = migration.context_variants(context, "tests/core/generated_consumer.c")
        self.assertEqual(variants, ((repo_include,), (generated_dir,)))
        problems = migration.unresolved_includes(self.root, context, build=build)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("unsafe or inconsistent", problems[0])
        self.assertIn("across compile contexts", problems[0])

        # The including source's own directory is searched before either
        # ordered compile variant, so it consistently shadows both roots.
        source_header = source.parent / "zr_vm_ffi_fixture_path.h"
        source_header.write_bytes(b"/* source-directory winner */\n")
        self.assertEqual(migration.unresolved_includes(self.root, context, build=build), [])

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_file_api_from_another_checkout_is_rejected [function]
    #
    # ## Summary
    # 在读取目标上下文前拒绝 codemodel 指向其它源根。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。人工 paths.source 指向 self.root/another-checkout。 assertRaisesRegex different source root；首拒为根身份比较。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1626
    # - tests/tooling/test_layout_moves.py:1635
    # - tests/tooling/test_layout_moves.py:1639
    # - tests/tooling/test_layout_moves.py:1644
    # - tests/tooling/test_layout_moves.py:1645
    # - scripts/layout/apply_moves.py:2464
    # - scripts/layout/apply_moves.py:2467
    # - scripts/layout/apply_moves.py:2468
    # - scripts/layout/apply_moves.py:2469
    def test_file_api_from_another_checkout_is_rejected(self) -> None:
        source = self.root / "tests/consumer.c"
        source.parent.mkdir(parents=True)
        source.write_bytes(b"int consumer;\n")
        build = self.root / "build"
        reply = build / ".cmake/api/v1/reply"
        reply.mkdir(parents=True)
        (reply / "index-test.json").write_text(json.dumps({
            "objects": [{"kind": "codemodel", "jsonFile": "cm.json"}],
        }), encoding="utf-8")
        (reply / "cm.json").write_text(json.dumps({
            "paths": {"source": str(self.root / "another-checkout")},
            "configurations": [{"name": "Debug", "targets": [{"jsonFile": "target.json"}]}],
        }), encoding="utf-8")
        (reply / "target.json").write_text(json.dumps({
            "sources": [{"path": str(source)}],
            "compileGroups": [{"includes": [{"path": str(source.parent)}], "sourceIndexes": [0]}],
        }), encoding="utf-8")
        with self.assertRaisesRegex(migration.MigrationError, "different source root"):
            migration.load_cmake_context(build, self.root)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_file_api_groups_bind_only_indexed_sources_and_select_config [function]
    #
    # ## Summary
    # 保持配置选择与sourceIndexes逐组绑定，不把所有根并集。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。人工Release/Debug两配置；Debug两源各自一根。 选择Debug后两个context各对应自己的根；再选缺失配置抛 unavailable。后一个断言须在前面完成后到达。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1677
    # - tests/tooling/test_layout_moves.py:1689
    # - tests/tooling/test_layout_moves.py:1695
    # - tests/tooling/test_layout_moves.py:1699
    # - tests/tooling/test_layout_moves.py:1700
    # - tests/tooling/test_layout_moves.py:1701
    # - tests/tooling/test_layout_moves.py:1702
    # - tests/tooling/test_layout_moves.py:1703
    # - tests/tooling/test_layout_moves.py:1704
    # - scripts/layout/apply_moves.py:2455
    # - scripts/layout/apply_moves.py:2456
    # - scripts/layout/apply_moves.py:2461
    # - scripts/layout/apply_moves.py:2492
    # - scripts/layout/apply_moves.py:2495
    # - scripts/layout/apply_moves.py:2497
    def test_file_api_groups_bind_only_indexed_sources_and_select_config(self) -> None:
        one = self.root / "tests/one.c"
        two = self.root / "tests/two.c"
        one.parent.mkdir(parents=True)
        one.write_bytes(b"int one;\n")
        two.write_bytes(b"int two;\n")
        build = self.root / "build"
        reply = build / ".cmake/api/v1/reply"
        reply.mkdir(parents=True)
        (reply / "index-test.json").write_text(
            '{"objects":[{"kind":"codemodel","jsonFile":"cm.json"}]}', encoding="utf-8")
        (reply / "cm.json").write_text(
            '{"paths":{"source":"' + str(self.root).replace("\\", "\\\\") + '"},'
            '"configurations":[{"name":"Release","targets":[{"jsonFile":"release.json"}]},'
            '{"name":"Debug","targets":[{"jsonFile":"target.json"}]}]}', encoding="utf-8")
        (reply / "release.json").write_text('{"sources":[],"compileGroups":[]}', encoding="utf-8")
        roots = ["tests/inc-one", "tests/inc-two"]
        for name in roots:
            (self.root / name).mkdir(parents=True)
        (reply / "target.json").write_text(
            '{"sources":[{"path":"tests/one.c"},{"path":"tests/two.c"}],'
            '"compileGroups":[{"includes":[{"path":"tests/inc-one"}],"sourceIndexes":[0]},'
            '{"includes":[{"path":"tests/inc-two"}],"sourceIndexes":[1]}]}', encoding="utf-8")
        context = migration.load_cmake_context(build, self.root, "Debug")
        self.assertEqual(context["tests/one.c"], (((self.root / roots[0]).resolve(),),))
        self.assertEqual(context["tests/two.c"], (((self.root / roots[1]).resolve(),),))
        with self.assertRaisesRegex(migration.MigrationError, "unavailable"):
            migration.load_cmake_context(build, self.root, "RelWithDebInfo")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_context_variants_propagate_without_unioning_header_roots [function]
    #
    # ## Summary
    # 把两源不同根变体传播到共享头，保留可检测的歧义。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。one/two都include shared.h，而两根各有 private.h。 共享头的variants集合等于两元组；进一步解析 private.h 抛跨变体歧义。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1735
    # - tests/tooling/test_layout_moves.py:1743
    # - tests/tooling/test_layout_moves.py:1746
    # - tests/tooling/test_layout_moves.py:1747
    # - tests/tooling/test_layout_moves.py:1751
    # - tests/tooling/test_layout_moves.py:1752
    # - tests/tooling/test_layout_moves.py:1753
    # - tests/tooling/test_layout_moves.py:1755
    # - scripts/layout/apply_moves.py:2545
    # - scripts/layout/apply_moves.py:2549
    # - scripts/layout/apply_moves.py:2571
    # - scripts/layout/apply_moves.py:2577
    # - scripts/layout/apply_moves.py:2579
    # - scripts/layout/apply_moves.py:769
    def test_context_variants_propagate_without_unioning_header_roots(self) -> None:
        root_a = self.root / "tests/include-a"
        root_b = self.root / "tests/include-b"
        root_a.mkdir(parents=True)
        root_b.mkdir(parents=True)
        (root_a / "private.h").write_bytes(b"/* a */\n")
        (root_b / "private.h").write_bytes(b"/* b */\n")
        for name in ("one.c", "two.c"):
            source = self.root / "tests" / name
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_bytes(b'#include "shared.h"\n')
        shared = self.root / "tests/shared.h"
        shared.write_bytes(b'#include "private.h"\n')
        context = {
            "tests/one.c": ((root_a,),),
            "tests/two.c": ((root_b,),),
        }
        propagated = migration.propagate_include_context(self.root, context)
        self.assertEqual(set(propagated["tests/shared.h"]), {(root_a,), (root_b,)})
        with self.assertRaisesRegex(migration.MigrationError, "resolves differently across compile contexts"):
            migration.resolve_include(self.root, "tests/shared.h", "private.h", {}, propagated)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_protected_compile_sources_are_omitted_from_file_api_context [function]
    #
    # ## Summary
    # 在上下文读取与传播两阶段排除 vendored 编译源。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。同一compile group绑定普通源和third_party Unity人工源。 普通源在context、保护源不在，传播后仍不在；未编译Unity或读取实际第三方源。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1783
    # - tests/tooling/test_layout_moves.py:1784
    # - tests/tooling/test_layout_moves.py:1794
    # - tests/tooling/test_layout_moves.py:1799
    # - tests/tooling/test_layout_moves.py:1800
    # - tests/tooling/test_layout_moves.py:1801
    # - tests/tooling/test_layout_moves.py:1802
    # - tests/tooling/test_layout_moves.py:1803
    # - tests/tooling/test_layout_moves.py:1804
    # - scripts/layout/apply_moves.py:2506
    # - scripts/layout/apply_moves.py:2554
    def test_protected_compile_sources_are_omitted_from_file_api_context(self) -> None:
        protected = self.root / "tests/third_party/zr_unity/unity.c"
        protected.parent.mkdir(parents=True)
        protected.write_bytes(b"#include \"missing-protected.h\"\n")
        ordinary = self.root / "tests/normal.c"
        ordinary.write_bytes(b"int normal;\n")
        build = self.root / "build"
        reply = build / ".cmake/api/v1/reply"
        reply.mkdir(parents=True)
        (reply / "index-test.json").write_text(
            '{"objects":[{"kind":"codemodel","jsonFile":"cm.json"}]}', encoding="utf-8")
        (reply / "cm.json").write_text(
            '{"paths":{"source":"' + str(self.root).replace("\\", "\\\\") + '"},'
            '"configurations":[{"name":"Debug","targets":[{"jsonFile":"target.json"}]}]}', encoding="utf-8")
        (reply / "target.json").write_text(
            '{"sources":[{"path":"tests/normal.c"},{"path":"tests/third_party/zr_unity/unity.c"}],'
            '"compileGroups":[{"includes":[{"path":"tests"}],"sourceIndexes":[0,1]}]}', encoding="utf-8")
        context = migration.load_cmake_context(build, self.root)
        self.assertIn("tests/normal.c", context)
        self.assertNotIn("tests/third_party/zr_unity/unity.c", context)
        propagated = migration.propagate_include_context(self.root, context)
        self.assertNotIn("tests/third_party/zr_unity/unity.c", propagated)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_protected_vendor_include_resolves_but_is_not_rewritten_or_scanned [function]
    #
    # ## Summary
    # 允许保护头作解析身份，但保持其内容和路径不参与改写。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。人工consumer引用人工vendor Unity头，并显式给其根。 解析得到vendor身份；rewritten_include为None、问题空、scan字典无vendor、vendor bytes未变。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1834
    # - tests/tooling/test_layout_moves.py:1838
    # - tests/tooling/test_layout_moves.py:1841
    # - tests/tooling/test_layout_moves.py:1843
    # - tests/tooling/test_layout_moves.py:1844
    # - tests/tooling/test_layout_moves.py:1845
    # - tests/tooling/test_layout_moves.py:1846
    # - tests/tooling/test_layout_moves.py:1848
    # - tests/tooling/test_layout_moves.py:1849
    # - scripts/layout/apply_moves.py:815
    # - scripts/layout/apply_moves.py:867
    # - scripts/layout/apply_moves.py:1649
    # - scripts/layout/apply_moves.py:2124
    def test_protected_vendor_include_resolves_but_is_not_rewritten_or_scanned(self) -> None:
        consumer = self.root / "tests/vendor_consumer.c"
        consumer.parent.mkdir(parents=True, exist_ok=True)
        consumer.write_bytes(b'#include "unity.h"\n')
        vendor = self.root / "tests/third_party/zr_unity/unity.h"
        vendor.parent.mkdir(parents=True)
        vendor.write_bytes(b"/* protected vendor fixture */\n")
        context = {"tests/vendor_consumer.c": (vendor.parent,)}

        resolved = migration.resolve_include(self.root, "tests/vendor_consumer.c", "unity.h", {}, context)
        self.assertEqual(resolved, ("tests/third_party/zr_unity/unity.h", "tests/third_party/zr_unity/unity.h"))
        self.assertEqual(migration.rewritten_include("unity.h", "tests/vendor_consumer.c", {}, self.root, context), None)
        self.assertEqual(migration.unresolved_includes(self.root, context), [])
        rewrites = migration.scan_rewrites(self.root, [], context)
        self.assertNotIn("tests/third_party/zr_unity/unity.h", rewrites)
        self.assertEqual(vendor.read_bytes(), b"/* protected vendor fixture */\n")

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_dormant_cmake_fragment_supplies_exact_fallback_context [function]
    #
    # ## Summary
    # 从有限字面wrapper恢复dormant根，应用私有头移动后再次恢复验证。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。人工CMake片段+共享helper+Unity根，真实run_tool只在夹具执行。 断言恢复根含harness/vendor，旧Unity解析，真实context传播无dormant源；手工rewrite含function/...；应用后新头存在/源改写，再恢复新include解析并--check返回0。所有后段仅if-reached。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1888
    # - tests/tooling/test_layout_moves.py:1898
    # - tests/tooling/test_layout_moves.py:1909
    # - tests/tooling/test_layout_moves.py:1921
    # - tests/tooling/test_layout_moves.py:1925
    # - tests/tooling/test_layout_moves.py:1929
    # - tests/tooling/test_layout_moves.py:1934
    # - tests/tooling/test_layout_moves.py:1942
    # - tests/tooling/test_layout_moves.py:1947
    # - tests/tooling/test_layout_moves.py:1949
    # - tests/tooling/test_layout_moves.py:1951
    # - tests/tooling/test_layout_moves.py:1953
    # - tests/tooling/test_layout_moves.py:1958
    # - tests/tooling/test_layout_moves.py:1960
    # - tests/tooling/test_layout_moves.py:1962
    # - scripts/layout/apply_moves.py:2190
    # - scripts/layout/apply_moves.py:2200
    # - scripts/layout/apply_moves.py:2306
    # - scripts/layout/apply_moves.py:2320
    # - scripts/layout/apply_moves.py:2342
    # - scripts/layout/apply_moves.py:2658
    # - scripts/layout/apply_moves.py:2659
    def test_dormant_cmake_fragment_supplies_exact_fallback_context(self) -> None:
        source = self.root / "tests/core/test_argument_staging.c"
        source.parent.mkdir(parents=True)
        source.write_bytes(
            b'#if defined(ZR_ARGUMENT_STAGING_TEST_CANDIDATE)\n'
            b'#include "function_argument_staging.h"\n#endif\n'
            b'#include "runtime_support.h"\n#include "unity.h"\n'
        )
        old_header = self.root / "zr_vm_core/src/zr_vm_core/function_argument_staging.h"
        old_header.parent.mkdir(parents=True)
        old_header.write_bytes(b"/* mapped fixture header */\n")
        fragment = self.root / "tests/cmake/ssa-argument-staging-tests.cmake"
        fragment.parent.mkdir(parents=True)
        fragment.write_text(
            "zr_vm_add_unity_test_target(zr_vm_ssa_argument_staging_test\n"
            "    ${CMAKE_SOURCE_DIR}/tests/core/test_argument_staging.c)\n"
            "target_include_directories(zr_vm_ssa_argument_staging_test PRIVATE\n"
            "    ${CMAKE_SOURCE_DIR}/zr_vm_core/include\n"
            "    ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)\n",
            encoding="utf-8",
        )
        common = self.root / "tests/CMakeLists.txt"
        common.write_text(
            "function(zr_vm_apply_common_test_settings target_name)\n"
            "  target_include_directories(${target_name} PRIVATE\n"
            "    ${CMAKE_SOURCE_DIR}/tests\n"
            "    ${CMAKE_SOURCE_DIR}/tests/harness\n"
            "    ${CMAKE_SOURCE_DIR}/tests/third_party/zr_unity/Unity/src\n"
            "  )\nendfunction()\n"
            "function(zr_vm_add_unity_test_target target_name)\n"
            "  zr_vm_apply_common_test_settings(${target_name})\nendfunction()\n",
            encoding="utf-8",
        )
        (self.root / "tests/harness").mkdir(parents=True)
        (self.root / "tests/harness/runtime_support.h").write_bytes(b"/* helper */\n")
        unity = self.root / "tests/third_party/zr_unity/Unity/src/unity.h"
        unity.parent.mkdir(parents=True)
        unity.write_bytes(b"/* protected unity fixture */\n")
        moves = [migration.Move(
            "zr_vm_core/src/zr_vm_core/function_argument_staging.h",
            "zr_vm_core/src/zr_vm_core/function/function_argument_staging.h",
        )]
        context = migration.load_cmake_fallback_context(
            str(self.root.resolve()), ("tests/core/test_argument_staging.c",))
        self.assertEqual(len(context["tests/core/test_argument_staging.c"]), 1)
        fallback_roots = context["tests/core/test_argument_staging.c"][0]
        self.assertIn((self.root / "tests/harness").resolve(), fallback_roots)
        self.assertIn(unity.parent.resolve(), fallback_roots)
        self.assertEqual(migration.resolve_include(
            self.root, "tests/core/test_argument_staging.c", "unity.h", {}, {}, context),
            ("tests/third_party/zr_unity/Unity/src/unity.h", "tests/third_party/zr_unity/Unity/src/unity.h"))
        actual_context = {"tests/active.c": (self.root / "tests",)}
        propagated = migration.propagate_include_context(self.root, actual_context)
        self.assertNotIn("tests/core/test_argument_staging.c", propagated)
        self.assertEqual(migration.context_variants(actual_context, "tests/active.c"),
                         ((self.root / "tests",),))
        rewritten = migration.rewrite_bytes(
            source.read_bytes(), "tests/core/test_argument_staging.c",
            moves, {move.old: move.new for move in moves}, self.root,
            context=context,
        )
        self.assertIn(b'#include "function/function_argument_staging.h"', rewritten)
        self.mapping(f"{moves[0].old}\t{moves[0].new}\n")
        code, output = self.run_tool("--apply")
        self.assertEqual(code, 0, output)
        self.assertTrue((self.root / moves[0].new).is_file())
        self.assertIn(b'#include "function/function_argument_staging.h"', source.read_bytes())
        post_context = migration.load_cmake_fallback_context(
            str(self.root.resolve()), ("tests/core/test_argument_staging.c",))
        post_resolved = migration.resolve_include(
                self.root, "tests/core/test_argument_staging.c",
                "function/function_argument_staging.h", {}, post_context)
        self.assertEqual(post_resolved, (moves[0].new, moves[0].new), repr(post_context))
        code, output = self.run_tool("--check")
        self.assertEqual(code, 0, output)

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_dormant_cmake_fallback_rejects_ambiguous_registration [function]
    #
    # ## Summary
    # 拒绝同一dormant源被两个字面目标登记。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。在两个cmake片段各写同一源的wrapper和私有根。 assertRaisesRegex ambiguous dormant CMake registration；不执行配置或模拟条件取值。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:1989
    # - tests/tooling/test_layout_moves.py:1991
    # - tests/tooling/test_layout_moves.py:1994
    # - tests/tooling/test_layout_moves.py:2000
    # - tests/tooling/test_layout_moves.py:2001
    # - scripts/layout/apply_moves.py:2247
    # - scripts/layout/apply_moves.py:2269
    # - scripts/layout/apply_moves.py:2273
    # - scripts/layout/apply_moves.py:2274
    # - scripts/layout/apply_moves.py:2276
    def test_dormant_cmake_fallback_rejects_ambiguous_registration(self) -> None:
        source = "tests/core/test_argument_staging.c"
        for name in ("one.cmake", "two.cmake"):
            fragment = self.root / "tests/cmake" / name
            fragment.parent.mkdir(parents=True, exist_ok=True)
            target = name.split(".")[0]
            fragment.write_text(
                f"zr_vm_add_unity_test_target({target} ${{CMAKE_SOURCE_DIR}}/{source})\n"
                f"target_include_directories({target} PRIVATE ${{CMAKE_SOURCE_DIR}}/zr_vm_core/src/zr_vm_core)\n",
                encoding="utf-8",
            )
        with self.assertRaisesRegex(migration.MigrationError, "ambiguous dormant CMake registration"):
            migration.load_cmake_fallback_context(str(self.root.resolve()), (source,))

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_migration_test_fixture_is_excluded_from_path_scans [function]
    #
    # ## Summary
    # 固定包含大量旧路径样本的自身测试不被改写扫描。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。直接传精确测试逻辑名称。 excluded True、is_scan_file False；只证明该精确路径。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:2025
    # - tests/tooling/test_layout_moves.py:2026
    # - tests/tooling/test_layout_moves.py:2027
    # - tests/tooling/test_layout_moves.py:2028
    # - scripts/layout/apply_moves.py:441
    # - scripts/layout/apply_moves.py:469
    # - scripts/layout/apply_moves.py:470
    def test_migration_test_fixture_is_excluded_from_path_scans(self) -> None:
        self.assertTrue(migration.excluded("tests/tooling/test_layout_moves.py"))
        self.assertFalse(migration.is_scan_file(
            "tests/tooling/test_layout_moves.py", self.root / "tests/tooling/test_layout_moves.py"))

    # @unit tests/tooling/test_layout_moves.py::LayoutMoveTests.test_atomic_rewrite_preserves_mode [function]
    #
    # ## Summary
    # 验证一次替换保留宿主可表达的 mode 并清除临时文件名。
    #
    # ## Parameters
    # - self: 由 unittest 创建、成功 setUp 的当前测试实例。
    #
    # ## Returns
    # 所有到达的断言通过则隐式返回 None；AssertionError/其它异常由 runner 记录，无业务返回码。
    #
    # ## Constraints and lifecycle
    # 当前用例只创建有限人工夹具，不复制产品源码。在case内写script.sh并chmod，保存stat模式；用FIXTURE_ROOT/atomic-tmp作兄弟临时根。 完整bytes为new、stat模式同原，再rmdir临时根。mode意义受平台限制；若前置断言/I/O失败，固定兄弟目录不受 self.temp.cleanup 管理，调用者须独占该父根并处理失败遗留。 运行完成前不得并行修改同一夹具树；断言失败由 unittest 记录，成功 setUp 后 runner 调用 tearDown 尝试清理随机树，setUp 失败/进程中止不保证此清理。
    #
    # ## References
    # - tests/tooling/test_layout_moves.py:2059
    # - tests/tooling/test_layout_moves.py:2061
    # - tests/tooling/test_layout_moves.py:2062
    # - tests/tooling/test_layout_moves.py:2063
    # - tests/tooling/test_layout_moves.py:2064
    # - tests/tooling/test_layout_moves.py:2065
    # - tests/tooling/test_layout_moves.py:2066
    # - tests/tooling/test_layout_moves.py:2067
    # - tests/tooling/test_layout_moves.py:2068
    # - scripts/layout/apply_moves.py:1912
    # - scripts/layout/apply_moves.py:1913
    # - scripts/layout/apply_moves.py:1921
    # - scripts/layout/apply_moves.py:1922
    # - scripts/layout/apply_moves.py:1923
    def test_atomic_rewrite_preserves_mode(self) -> None:
        destination = self.root / "script.sh"
        destination.write_bytes(b"old\n")
        destination.chmod(0o751)
        original_mode = stat.S_IMODE(destination.stat().st_mode)
        temp = FIXTURE_ROOT / "atomic-tmp"
        migration.atomic_write(destination, b"new\n", temp)
        self.assertEqual(destination.read_bytes(), b"new\n")
        self.assertEqual(stat.S_IMODE(destination.stat().st_mode), original_mode)
        temp.rmdir()


if __name__ == "__main__":
    unittest.main()
