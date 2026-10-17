"""Tests for conservative CMake context recovery for uncompiled test sources."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
MODULE_PATH = REPO / "scripts" / "layout" / "dormant_contexts.py"
APPLY_PATH = REPO / "scripts" / "layout" / "apply_moves.py"
APPLY_SPEC = importlib.util.spec_from_file_location("layout_apply_moves", APPLY_PATH)
assert APPLY_SPEC and APPLY_SPEC.loader
apply_moves = importlib.util.module_from_spec(APPLY_SPEC)
sys.modules[APPLY_SPEC.name] = apply_moves
APPLY_SPEC.loader.exec_module(apply_moves)
FIXTURE_ROOT = apply_moves.REGISTERED_FIXTURE_ROOT
SPEC = importlib.util.spec_from_file_location("layout_dormant_contexts", MODULE_PATH)
assert SPEC and SPEC.loader
dormant = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = dormant
SPEC.loader.exec_module(dormant)


class DormantContextTests(unittest.TestCase):
    def setUp(self) -> None:
        FIXTURE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="dormant-context-", dir=FIXTURE_ROOT)
        self.root = Path(self.temp.name)
        (self.root / "tests/cmake").mkdir(parents=True)
        (self.root / "tests/harness").mkdir(parents=True)
        (self.root / "tests/third_party/zr_unity/Unity/src").mkdir(parents=True)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write(self, relative: str, content: str = "") -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def file_api(self, target: str, source_groups: list[list[str]],
                 include_groups: list[list[str]], source_root: Path | None = None) -> Path:
        """Create a minimal File API codemodel: one source list and C groups."""
        build = self.root / "build"
        reply = build / ".cmake/api/v1/reply"
        reply.mkdir(parents=True)
        (reply / "index-test.json").write_text(json.dumps({
            "objects": [{"kind": "codemodel", "jsonFile": "codemodel.json"}],
        }), encoding="utf-8")
        (reply / "codemodel.json").write_text(json.dumps({
            "paths": {"source": str(source_root or self.root)},
            "configurations": [{"name": "Debug", "targets": [{
                "name": target, "jsonFile": "target.json",
            }]}],
        }), encoding="utf-8")
        source_paths = sorted({source for group in source_groups for source in group})
        target_doc = {
            "name": target,
            "sources": [{"path": source} for source in source_paths],
            "compileGroups": [],
        }
        for roots, sources in zip(include_groups, source_groups):
            target_doc["compileGroups"].append({
                "language": "C",
                "includes": [{"path": path, "isSystem": False} for path in roots],
                "sourceIndexes": [source_paths.index(source) for source in sources],
            })
        (reply / "target.json").write_text(json.dumps(target_doc), encoding="utf-8")
        return build

    def test_direct_registration_recovers_owner_file_api_variants_without_union(self) -> None:
        source = "tests/layout_fixture__/dormant.c"
        bare_relative_source = "tests/layout_fixture__/bare_relative.c"
        self.write(source, '#include "unity.h"\n')
        self.write(bare_relative_source, "int dormant;\n")
        self.write("tests/cmake/feature.cmake", (
            "zr_vm_add_unity_test_target(zr_vm_dormant_test "
            "${CMAKE_CURRENT_SOURCE_DIR}/layout_fixture__/dormant.c "
            "layout_fixture__/bare_relative.c)\n"
        ))
        self.write("tests/CMakeLists.txt", (
            "function(zr_vm_apply_common_test_settings target_name)\n"
            " target_include_directories(${target_name} PRIVATE "
            "${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/tests/harness "
            "${CMAKE_SOURCE_DIR}/tests/third_party/zr_unity/Unity/src)\n"
            "endfunction()\n"
            "function(zr_vm_add_unity_test_target target_name)\n"
            " zr_vm_apply_common_test_settings(${target_name})\n"
            "endfunction()\n"
        ))
        build = self.file_api("zr_vm_dormant_test",
                              [["tests/layout_fixture__/anchor_a.c"],
                               ["tests/layout_fixture__/anchor_b.c"]],
                              [["tests/inc-a"], ["tests/inc-b"]])
        contexts = dormant.recover_contexts(self.root, build, (source, bare_relative_source), "Debug")
        variants = contexts[source].variants
        self.assertEqual(contexts[bare_relative_source].status, "recovered")
        self.assertEqual(contexts[bare_relative_source].owners, ("zr_vm_dormant_test",))
        self.assertEqual(len(variants), 2)
        self.assertEqual(variants, (
            ((self.root / "tests/inc-a").resolve(),),
            ((self.root / "tests/inc-b").resolve(),),
        ))

    def test_target_sources_registration_is_found_and_static_owner_is_reported(self) -> None:
        source = "tests/layout_fixture__/dormant_eis6.c"
        self.write(source, "int dormant;\n")
        self.write("tests/cmake/ssa-tests.cmake", (
            "add_executable(zr_vm_ssa_binding_rows_artifact_test base.c)\n"
            "if (BUILD_STATIC_LIB AND NOT BUILD_SHARED_LIB)\n"
            " target_sources(zr_vm_ssa_binding_rows_artifact_test PRIVATE "
            "${CMAKE_SOURCE_DIR}/tests/layout_fixture__/dormant_eis6.c)\n"
            "endif()\n"
        ))
        build = self.file_api("zr_vm_ssa_binding_rows_artifact_test",
                              [["tests/layout_fixture__/base.c"]],
                              [["tests", "tests/third_party/zr_unity/Unity/src"]])
        contexts = dormant.recover_contexts(self.root, build, (source,), "Debug")
        self.assertEqual(contexts[source].status, "recovered")
        self.assertEqual(contexts[source].owners, ("zr_vm_ssa_binding_rows_artifact_test",))
        self.assertIn((self.root / "tests/third_party/zr_unity/Unity/src").resolve(),
                      contexts[source].variants[0])

    def test_actual_context_is_never_overwritten_or_augmented(self) -> None:
        source = "tests/layout_fixture__/active.c"
        self.write(source)
        self.write("tests/cmake/feature.cmake",
                   "zr_vm_add_unity_test_target(zr_vm_dormant_test ${CMAKE_SOURCE_DIR}/tests/layout_fixture__/active.c)\n")
        build = self.file_api("zr_vm_dormant_test", [[source]], [["tests/actual"]])
        contexts = dormant.recover_contexts(self.root, build, (source,), "Debug")
        self.assertEqual(contexts[source].status, "actual")
        self.assertEqual(contexts[source].variants,
                         (((self.root / "tests/actual").resolve(),),))

    def test_unregistered_or_foreign_source_stays_unknown(self) -> None:
        sources = ("tests/layout_fixture__/not_registered.c", "tests/layout_fixture__/foreign.c")
        self.write(sources[0])
        self.write(sources[1])
        self.write("tests/cmake/stale-owner.cmake", (
            "add_executable(zr_vm_stale_owner_test "
            "${CMAKE_SOURCE_DIR}/tests/layout_fixture__/not_registered.c)\n"
        ))
        build = self.file_api("unrelated", [["tests/layout_fixture__/other.c"]], [["tests"]])
        contexts = dormant.recover_contexts(self.root, build, sources, "Debug")
        for source in sources:
            self.assertEqual(contexts[source].status, "unknown")
            self.assertEqual(contexts[source].variants, ())
        self.assertEqual(contexts[sources[0]].owners, ("zr_vm_stale_owner_test",))

    def test_wrapper_fallback_requires_proven_common_helper_call(self) -> None:
        source = "tests/layout_fixture__/fallback.c"
        self.write(source)
        self.write("tests/cmake/wrapper.cmake", (
            "zr_vm_add_unity_test_target(zr_vm_fallback_test "
            "${CMAKE_SOURCE_DIR}/tests/layout_fixture__/fallback.c)\n"
        ))
        no_helper = (
            "function(zr_vm_apply_common_test_settings target_name)\n"
            " target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/tests)\n"
            "endfunction()\n"
            "function(zr_vm_add_unity_test_target target_name)\n"
            "endfunction()\n"
        )
        self.write("tests/CMakeLists.txt", no_helper)
        build = self.file_api("unrelated", [["tests/layout_fixture__/other.c"]], [["tests"]])
        denied = dormant.recover_contexts(self.root, build, (source,), "Debug")
        self.assertEqual(denied[source].status, "unknown")
        self.assertEqual(denied[source].variants, ())

        self.write("tests/CMakeLists.txt", no_helper.replace(
            "function(zr_vm_add_unity_test_target target_name)\n"
            "endfunction()",
            "function(zr_vm_add_unity_test_target target_name)\n"
            " zr_vm_apply_common_test_settings(${target_name})\n"
            "endfunction()",
        ))
        allowed = dormant.recover_contexts(self.root, build, (source,), "Debug")
        self.assertEqual(allowed[source].status, "recovered")
        self.assertEqual(allowed[source].variants,
                         (((self.root / "tests").resolve(),),))

    def test_one_owner_without_a_current_context_makes_recovery_unknown(self) -> None:
        source = "tests/layout_fixture__/shared_owner.c"
        self.write(source)
        self.write("tests/cmake/owners.cmake", (
            "add_executable(zr_vm_owner_current ${CMAKE_SOURCE_DIR}/tests/layout_fixture__/shared_owner.c)\n"
            "add_executable(zr_vm_owner_absent ${CMAKE_SOURCE_DIR}/tests/layout_fixture__/shared_owner.c)\n"
        ))
        build = self.file_api("zr_vm_owner_current", [["tests/layout_fixture__/anchor.c"]],
                              [["tests/current-owner"]])
        contexts = dormant.recover_contexts(self.root, build, (source,), "Debug")
        self.assertEqual(contexts[source].status, "unknown")
        self.assertEqual(contexts[source].variants, ())
        self.assertEqual(contexts[source].owners,
                         ("zr_vm_owner_absent", "zr_vm_owner_current"))

    def test_file_api_source_root_must_match_checkout(self) -> None:
        source = "tests/layout_fixture__/source_root.c"
        self.write(source)
        foreign_source_root = self.root / "foreign-source-root"
        build = self.file_api("zr_vm_source_root_test", [[source]], [["tests"]], foreign_source_root)
        with self.assertRaises(dormant.DormantContextError):
            dormant.recover_contexts(self.root, build, (source,), "Debug")

    def test_excluded_cmake_subtree_cannot_add_an_owner(self) -> None:
        source = "tests/layout_fixture__/filtered_owner.c"
        self.write(source)
        self.write("tests/cmake/current.cmake", (
            "add_executable(zr_vm_owner_current ${CMAKE_SOURCE_DIR}/tests/layout_fixture__/filtered_owner.c)\n"
        ))
        self.write("tests/acceptance/history/old.cmake", (
            "add_executable(zr_vm_owner_excluded ${CMAKE_SOURCE_DIR}/tests/layout_fixture__/filtered_owner.c)\n"
        ))
        build = self.file_api("zr_vm_owner_current", [["tests/layout_fixture__/anchor.c"]],
                              [["tests/current-owner"]])
        excluded_paths: list[str] = []

        def exclude(relative: str) -> bool:
            excluded_paths.append(relative.replace("\\", "/"))
            return relative.replace("\\", "/").startswith("tests/acceptance")

        contexts = dormant.recover_contexts(self.root, build, (source,), "Debug", exclude=exclude)
        self.assertEqual(contexts[source].status, "recovered")
        self.assertEqual(contexts[source].owners, ("zr_vm_owner_current",))
        self.assertTrue(any(path == "tests/acceptance" for path in excluded_paths))

    def test_dormant_header_propagation_is_separate_from_actual_graph(self) -> None:
        self.write("tests/consumer.c", '#include "support.h"\n')
        self.write("tests/support.h", '#include "nested.h"\n')
        self.write("tests/nested.h", "/* no includes */\n")
        actual = {"tests/actual.c": ((self.root / "tests").resolve(),)}
        propagated = dormant.propagate_dormant_headers(
            self.root, {"tests/consumer.c": ((self.root / "tests").resolve(),)},
            exclude=lambda path: path.endswith("nested.h"))
        self.assertEqual(set(propagated), {"tests/consumer.c", "tests/support.h"})
        self.assertEqual(set(actual), {"tests/actual.c"})
        self.assertNotIn("tests/consumer.c", actual)


if __name__ == "__main__":
    unittest.main()

# @unit tests/tooling/test_layout_dormant_contexts.py::REPO [state]
# ## Summary
# 从测试文件位置定位实际 checkout，仅用于装载相邻 layout 工具代码。
# ## Constraints and lifecycle
# Path 不持有源码树租约，不是夹具目录，也不授权写实际仓库。由 __file__ 的父级布局推导，移动测试文件后须连同该定位约束核查。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:13
# - tests/tooling/test_layout_dormant_contexts.py:14
# - tests/tooling/test_layout_dormant_contexts.py:15

# @unit tests/tooling/test_layout_dormant_contexts.py::MODULE_PATH [state]
# ## Summary
# 固定本测试所验证的 dormant_contexts 实现文件，避免依赖进程 sys.path 中同名模块。
# ## Constraints and lifecycle
# 路径指向当前 checkout 的人工源码；装载由 SPEC 协议执行，不复制或改写该文件。文件内容须在测试运行期间保持稳定。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:14
# - tests/tooling/test_layout_dormant_contexts.py:22
# - tests/tooling/test_layout_dormant_contexts.py:26

# @unit tests/tooling/test_layout_dormant_contexts.py::APPLY_PATH [state]
# ## Summary
# 定位提供已登记 layout 夹具根选择规则的 apply_moves 模块。
# ## Constraints and lifecycle
# 作为测试支持装载路径使用，不调用其 main，也不执行迁移、CMake 或真实源码扫描。该支持模块的装载仍受 Python 缓存与 sys.modules 规则影响。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:15
# - tests/tooling/test_layout_dormant_contexts.py:16
# - tests/tooling/test_layout_dormant_contexts.py:20
# - tests/tooling/test_layout_dormant_contexts.py:21
# - scripts/layout/apply_moves.py:105
# - scripts/layout/apply_moves.py:143

# @unit tests/tooling/test_layout_dormant_contexts.py::module::apply_moves_import [block]
# ## Summary
# 从指定文件创建、发布并执行 layout 支持模块，以获得一致的登记根选择规则。
# ## Constraints and lifecycle
# 顺序必须是 spec 检查、module_from_spec、sys.modules 发布、exec_module。模块名可能覆盖进程既有绑定，故运行器应使用受控新进程或自行管理命名空间。未调用支持模块 main；Python 缓存读取/写入由解释器配置决定，不能把装载概称为无缓存操作。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:16
# - tests/tooling/test_layout_dormant_contexts.py:17
# - tests/tooling/test_layout_dormant_contexts.py:18
# - tests/tooling/test_layout_dormant_contexts.py:19
# - tests/tooling/test_layout_dormant_contexts.py:20
# - tests/tooling/test_layout_dormant_contexts.py:21
# - scripts/layout/apply_moves.py:143
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::APPLY_SPEC [state]
# ## Summary
# 保存从实际 apply_moves 源路径创建的动态装载描述，供测试支持模块实例化。
# ## Constraints and lifecycle
# 描述及 loader 必须可用，紧随的 assert 检查此前提；启用 Python 优化会去掉该 assert，不能依赖优化模式保留同一保护。描述不代表模块已执行或资源根已创建。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:16
# - tests/tooling/test_layout_dormant_contexts.py:17
# - tests/tooling/test_layout_dormant_contexts.py:18
# - tests/tooling/test_layout_dormant_contexts.py:20
# - tests/tooling/test_layout_dormant_contexts.py:19

# @unit tests/tooling/test_layout_dormant_contexts.py::apply_moves [state]
# ## Summary
# 持有已在独立模块名下发布并执行的 layout 支持模块，借用其夹具根配置。
# ## Constraints and lifecycle
# sys.modules 的 layout_apply_moves 绑定在执行前建立；执行失败不会由本模块自动恢复先前绑定。此对象不代表调用过迁移 CLI，测试不拥有它所指的旧任务目录。导入缓存写入须由 Python -B 等运行配置控制。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:18
# - tests/tooling/test_layout_dormant_contexts.py:19
# - tests/tooling/test_layout_dormant_contexts.py:20
# - tests/tooling/test_layout_dormant_contexts.py:21

# @unit tests/tooling/test_layout_dormant_contexts.py::FIXTURE_ROOT [state]
# ## Summary
# 确定每个测试唯一临时子目录的父根，借用 apply_moves 的已登记根选择结果。
# ## Constraints and lifecycle
# 父根属于运行器配置，不属于某一 case；setUp 只确保它存在并在其下创建唯一子目录，tearDown 不递归重置父根。运行器须提前登记并稳定该根，不能因默认指向旧 layout 任务而取得复用或清理权限。并行进程也须各自约束根归属。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:21
# - tests/tooling/test_layout_dormant_contexts.py:31
# - tests/tooling/test_layout_dormant_contexts.py:32
# - tests/tooling/test_layout_dormant_contexts.py:39
# - scripts/layout/apply_moves.py:107
# - scripts/layout/apply_moves.py:110
# - scripts/layout/apply_moves.py:120

# @unit tests/tooling/test_layout_dormant_contexts.py::module::dormant_import [block]
# ## Summary
# 将待测实现发布到其真实动态模块名后执行，使 frozen dataclass 与直接测试调用共享同一模块环境。
# ## Constraints and lifecycle
# sys.modules 发布先于 exec_module；失败可能留下部分模块和名字绑定，不负责恢复旧模块。仅装载代码，不执行恢复查询或测试 case；需要运行器固定原文件与缓存查找配置。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:22
# - tests/tooling/test_layout_dormant_contexts.py:23
# - tests/tooling/test_layout_dormant_contexts.py:24
# - tests/tooling/test_layout_dormant_contexts.py:25
# - tests/tooling/test_layout_dormant_contexts.py:26
# - scripts/layout/dormant_contexts.py:23
# - scripts/layout/dormant_contexts.py:31
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::SPEC [state]
# ## Summary
# 保存从实际 dormant_contexts 源文件建立的动态模块装载描述。
# ## Constraints and lifecycle
# 描述及 loader 的存在由紧随 assert 检查；不是执行完成证明。模块需在 dataclass 执行前以同一名称发布到 sys.modules，且源码在运行期间须稳定。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:22
# - tests/tooling/test_layout_dormant_contexts.py:23
# - tests/tooling/test_layout_dormant_contexts.py:24
# - tests/tooling/test_layout_dormant_contexts.py:25
# - tests/tooling/test_layout_dormant_contexts.py:26
# - scripts/layout/dormant_contexts.py:23

# @unit tests/tooling/test_layout_dormant_contexts.py::dormant [state]
# ## Summary
# 持有测试直接调用的 dormant_contexts 模块实例，隔离普通 import 路径中的同名候选。
# ## Constraints and lifecycle
# 执行前绑定 sys.modules['layout_dormant_contexts'] 以满足 dataclass 的模块查找；绑定在进程中保留且失败时不自动回滚。它不持有 fixture 目录；当前测试经 recover/propagate 接口读取各 case 的模拟树。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:24
# - tests/tooling/test_layout_dormant_contexts.py:25
# - tests/tooling/test_layout_dormant_contexts.py:26
# - tests/tooling/test_layout_dormant_contexts.py:101
# - tests/tooling/test_layout_dormant_contexts.py:238
# - scripts/layout/dormant_contexts.py:24
# - tests/tooling/test_layout_dormant_contexts.py:124
# - tests/tooling/test_layout_dormant_contexts.py:136
# - tests/tooling/test_layout_dormant_contexts.py:150
# - tests/tooling/test_layout_dormant_contexts.py:172
# - tests/tooling/test_layout_dormant_contexts.py:183
# - tests/tooling/test_layout_dormant_contexts.py:197
# - tests/tooling/test_layout_dormant_contexts.py:208
# - tests/tooling/test_layout_dormant_contexts.py:209
# - tests/tooling/test_layout_dormant_contexts.py:228

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests [class]
# ## Summary
# 以每 case 独立模拟 checkout 与 File API JSON 验证 dormant 上下文模型的有限声明、归属和传播规则。
# ## Fields and invariants
# - temp: 由 setUp 创建、tearDown 清理的 TemporaryDirectory 控制对象。
# - root: temp 子目录的 Path；各 write/file_api 只应使用该所有权域。
# ## Constraints and lifecycle
# 继承 unittest.TestCase；默认发现器按 test 前缀装入九个方法。setUp 成功后每例拥有一个独立 TemporaryDirectory，父根由运行器管理。测试调用本仓库 Python 实现，并生成文本/JSON 夹具；不执行 CMake、编译器或夹具中的 C/H 源码。方法名不能代替实际断言范围。实例资源仅在成功 setUp 后由 runner 安排 tearDown，setUp 失败无此保证。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:29
# - tests/tooling/test_layout_dormant_contexts.py:30
# - tests/tooling/test_layout_dormant_contexts.py:38
# - tests/tooling/test_layout_dormant_contexts.py:77
# - tests/tooling/test_layout_dormant_contexts.py:233
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.setUp [function]
# ## Summary
# 为一个 case 创建唯一模拟 checkout 并准备声明扫描所需的空目录结构。
# ## Parameters
# @param[in,out] self 由 unittest 使用的测试实例，将获得 temp 与 root 属性。
# ## Returns
# 无 Python 返回值；成功时发布 self.temp/self.root，失败异常交给 unittest，可能已留下部分目录。
# ## Constraints and lifecycle
# 先创建受控父根，再创建 dormant-context- 前缀的唯一子目录；只建立空目录而非复制源码树。运行器须确保 FIXTURE_ROOT 是已登记、自有且稳定的 E 根，不能指望本函数验证祖先链接或清理旧父根。若本函数中途失败，unittest 不保证执行 tearDown；TemporaryDirectory 最终析构也不是立即清理合同。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:30
# - tests/tooling/test_layout_dormant_contexts.py:31
# - tests/tooling/test_layout_dormant_contexts.py:32
# - tests/tooling/test_layout_dormant_contexts.py:34
# - tests/tooling/test_layout_dormant_contexts.py:36
# - tests/tooling/test_layout_dormant_contexts.py:38
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.tearDown [function]
# ## Summary
# 释放本 case 的临时模拟树，保留运行器管理的共享父根。
# ## Parameters
# @param[in,out] self 已成功 setUp 且持有 temp 的实例。
# ## Returns
# 无 Python 返回值；正常完成表示本次 cleanup 调用完成，不证明其他进程或父根已清空。
# ## Constraints and lifecycle
# 只调用本实例 TemporaryDirectory.cleanup，不重置 FIXTURE_ROOT、不触及真实 checkout 或其他 case。正常断言失败/异常也由 runner 在成功 setUp 后安排此方法；setUp 失败不在保证内，cleanup 自身失败可作为测试错误向上传播。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:38
# - tests/tooling/test_layout_dormant_contexts.py:39
# - tests/tooling/test_layout_dormant_contexts.py:32
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.write [function]
# ## Summary
# 在当前 case 根内写入人工源或 CMake 文本，供静态模型读取。
# ## Parameters
# @param[in,out] self 已成功 setUp 的实例及其自有 root。
# @param[in] relative 本测试提供的受控相对路径；须保持在本 case 根内。
# @param[in] content 写入的 UTF-8 文本，默认空字符串。
# ## Returns
# @return 返回所写文件的 Path。
# ## Constraints and lifecycle
# 本 helper 不自行拒绝绝对路径或 ..，安全边界依赖受控 fixture 参数；本文件各调用使用固定根内拼写。创建父目录并直接覆盖文本，失败可留下目录或部分写入，不提供事务回滚；tearDown 只在 setUp 成功后负责本 case 树。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:41
# - tests/tooling/test_layout_dormant_contexts.py:42
# - tests/tooling/test_layout_dormant_contexts.py:43
# - tests/tooling/test_layout_dormant_contexts.py:44
# - tests/tooling/test_layout_dormant_contexts.py:80
# - tests/tooling/test_layout_dormant_contexts.py:176
# - tests/tooling/test_layout_dormant_contexts.py:234
# - tests/tooling/test_layout_dormant_contexts.py:81
# - tests/tooling/test_layout_dormant_contexts.py:82
# - tests/tooling/test_layout_dormant_contexts.py:87
# - tests/tooling/test_layout_dormant_contexts.py:113
# - tests/tooling/test_layout_dormant_contexts.py:114
# - tests/tooling/test_layout_dormant_contexts.py:132
# - tests/tooling/test_layout_dormant_contexts.py:133
# - tests/tooling/test_layout_dormant_contexts.py:143
# - tests/tooling/test_layout_dormant_contexts.py:144
# - tests/tooling/test_layout_dormant_contexts.py:145
# - tests/tooling/test_layout_dormant_contexts.py:158
# - tests/tooling/test_layout_dormant_contexts.py:159
# - tests/tooling/test_layout_dormant_contexts.py:170
# - tests/tooling/test_layout_dormant_contexts.py:190
# - tests/tooling/test_layout_dormant_contexts.py:191
# - tests/tooling/test_layout_dormant_contexts.py:205
# - tests/tooling/test_layout_dormant_contexts.py:213
# - tests/tooling/test_layout_dormant_contexts.py:214
# - tests/tooling/test_layout_dormant_contexts.py:217
# - tests/tooling/test_layout_dormant_contexts.py:235
# - tests/tooling/test_layout_dormant_contexts.py:236

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.file_api [function]
# ## Summary
# 为一次模型查询构造最小、单目标的 File API codemodel 与 C compile groups。
# ## Parameters
# @param[in,out] self 已成功 setUp 的实例，拥有将写入的 root/build 子树。
# @param[in] target 模拟目标名，不用于启动实际构建。
# @param[in] source_groups 逐 compile group 的源相对路径列表，用于构造共享 sources 及索引。
# @param[in] include_groups 与 source_groups 配对的 ordered include-root 列表。
# @param[in] source_root 可选 codemodel source 身份，默认本 case 根；可故意提供不同根测试拒绝。
# ## Returns
# @return 返回本 case 下的 build Path；该目录只含模拟 JSON，不是经过 CMake 配置的真实构建。
# ## Constraints and lifecycle
# reply.mkdir 不允许同目录重复创建；三份 JSON 顺次写入，失败不回滚已写前缀。zip 按较短输入停止，本 helper 不验证两组长度相等；各现有调用提供匹配组。调用方保持 fixture 输入受控且串行，不能把生成声明当作 native 或 CMake 运行证据。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:47
# - tests/tooling/test_layout_dormant_contexts.py:50
# - tests/tooling/test_layout_dormant_contexts.py:52
# - tests/tooling/test_layout_dormant_contexts.py:53
# - tests/tooling/test_layout_dormant_contexts.py:56
# - tests/tooling/test_layout_dormant_contexts.py:62
# - tests/tooling/test_layout_dormant_contexts.py:68
# - tests/tooling/test_layout_dormant_contexts.py:74
# - tests/tooling/test_layout_dormant_contexts.py:97
# - tests/tooling/test_layout_dormant_contexts.py:207
# - scripts/layout/dormant_contexts.py:280
# - tests/tooling/test_layout_dormant_contexts.py:121
# - tests/tooling/test_layout_dormant_contexts.py:135
# - tests/tooling/test_layout_dormant_contexts.py:149
# - tests/tooling/test_layout_dormant_contexts.py:171
# - tests/tooling/test_layout_dormant_contexts.py:195
# - tests/tooling/test_layout_dormant_contexts.py:220

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_direct_registration_recovers_owner_file_api_variants_without_union [function]
# ## Summary
# 检查 wrapper 的字面直接注册与裸相对源都能归属到当前目标，并保持两组 C roots 的顺序与分离。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 为两个源写固定注册，为同一目标设置 inc-a/inc-b 两个 compile group；对裸相对源断言 recovered 与 owner，对另一个源断言 variants 长度2及精确顺序。包含的 unity.h 是输入样本，不编译或要求该头真实存在；不额外证明所有 status/owner 字段组合或完整 CMake 求值。资源由本例 tearDown 收口。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:77
# - tests/tooling/test_layout_dormant_contexts.py:80
# - tests/tooling/test_layout_dormant_contexts.py:82
# - tests/tooling/test_layout_dormant_contexts.py:87
# - tests/tooling/test_layout_dormant_contexts.py:97
# - tests/tooling/test_layout_dormant_contexts.py:101
# - tests/tooling/test_layout_dormant_contexts.py:103
# - tests/tooling/test_layout_dormant_contexts.py:105
# - tests/tooling/test_layout_dormant_contexts.py:106
# - scripts/layout/dormant_contexts.py:404
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_target_sources_registration_is_found_and_static_owner_is_reported [function]
# ## Summary
# 检查 target_sources 的字面测试源注册被识别，并从该 owner 的当前 C group 借用 roots。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 样本故意把 target_sources 放在静态库条件文本中；本模型不求值该条件。断言 recovered、精确 owner 及一条 Unity root 存在；不运行目标、不证明此配置实际启用条件分支或其他 root 的完整顺序。fixture build 只是本例 JSON。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:111
# - tests/tooling/test_layout_dormant_contexts.py:114
# - tests/tooling/test_layout_dormant_contexts.py:116
# - tests/tooling/test_layout_dormant_contexts.py:117
# - tests/tooling/test_layout_dormant_contexts.py:121
# - tests/tooling/test_layout_dormant_contexts.py:124
# - tests/tooling/test_layout_dormant_contexts.py:125
# - tests/tooling/test_layout_dormant_contexts.py:126
# - tests/tooling/test_layout_dormant_contexts.py:127
# - scripts/layout/dormant_contexts.py:221
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_actual_context_is_never_overwritten_or_augmented [function]
# ## Summary
# 检查 File API 已给该源提供 C group 时，字面 wrapper 注册不会向其添加恢复 roots。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 断言 actual 与只含 tests/actual 的精确 variants；查询从模拟 JSON 取得实际源索引，未使用显式 raw_context 参数。它不测试对象身份保留、实际项目图的并发修改或所有 root 转换输入。方法只改本例目录。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:130
# - tests/tooling/test_layout_dormant_contexts.py:133
# - tests/tooling/test_layout_dormant_contexts.py:135
# - tests/tooling/test_layout_dormant_contexts.py:136
# - tests/tooling/test_layout_dormant_contexts.py:137
# - tests/tooling/test_layout_dormant_contexts.py:138
# - scripts/layout/dormant_contexts.py:388
# - scripts/layout/dormant_contexts.py:390
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_unregistered_or_foreign_source_stays_unknown [function]
# ## Summary
# 检查没有可用当前 owner 的注册源与完全未注册源均保持 unknown，而非借用无关目标 roots。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 给一个源配置陈旧 owner，另一个不注册；File API 只有 unrelated 目标。对两者断言 unknown/空 variants，并只对陈旧注册源断言保留该 owner。名字中的 foreign 指本模型的无关归属样本，不是访问外部 checkout；不验证任意 CMake 条件执行。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:141
# - tests/tooling/test_layout_dormant_contexts.py:145
# - tests/tooling/test_layout_dormant_contexts.py:149
# - tests/tooling/test_layout_dormant_contexts.py:150
# - tests/tooling/test_layout_dormant_contexts.py:152
# - tests/tooling/test_layout_dormant_contexts.py:153
# - tests/tooling/test_layout_dormant_contexts.py:154
# - scripts/layout/dormant_contexts.py:392
# - scripts/layout/dormant_contexts.py:414
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_wrapper_fallback_requires_proven_common_helper_call [function]
# ## Summary
# 比较同一字面 wrapper 在未调用和明确调用 common helper 时，fallback 从拒绝变为有限恢复。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 先写空 wrapper，断言 unknown/空 variants；再原位改其文本以调用 common helper，断言 recovered 和只含 tests root 的精确 variants。复用同一模拟 File API，不再次创建 reply。该对照检验静态证明条件，不执行 CMake function，也不证明通用 wrapper、动态参数或条件语义。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:156
# - tests/tooling/test_layout_dormant_contexts.py:163
# - tests/tooling/test_layout_dormant_contexts.py:170
# - tests/tooling/test_layout_dormant_contexts.py:172
# - tests/tooling/test_layout_dormant_contexts.py:173
# - tests/tooling/test_layout_dormant_contexts.py:176
# - tests/tooling/test_layout_dormant_contexts.py:183
# - tests/tooling/test_layout_dormant_contexts.py:184
# - tests/tooling/test_layout_dormant_contexts.py:185
# - scripts/layout/dormant_contexts.py:409
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_one_owner_without_a_current_context_makes_recovery_unknown [function]
# ## Summary
# 检查多 owner 源只要有一个 owner 缺乏当前组与受证 fallback，就不发布不完整恢复 roots。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 声明 current/absent 两 owner，但仅给 current 目标提供模拟 C group；断言 unknown、空 variants 与按名称排序的两个 owner。没有为 absent 构造有效 wrapper；不覆盖两个 owner 均完整时的所有组合或并发配置变化。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:188
# - tests/tooling/test_layout_dormant_contexts.py:191
# - tests/tooling/test_layout_dormant_contexts.py:195
# - tests/tooling/test_layout_dormant_contexts.py:197
# - tests/tooling/test_layout_dormant_contexts.py:198
# - tests/tooling/test_layout_dormant_contexts.py:199
# - tests/tooling/test_layout_dormant_contexts.py:200
# - scripts/layout/dormant_contexts.py:396
# - scripts/layout/dormant_contexts.py:414
# - scripts/layout/dormant_contexts.py:420
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_file_api_source_root_must_match_checkout [function]
# ## Summary
# 检查 codemodel 指向不同 source root 时，恢复接口以 DormantContextError 拒绝该回复。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# foreign-source-root 是本 case 根下的一个 Path 身份，不是实际外部工程；不创建或构建该树。assertRaises 只验证异常类型，不校验消息、首条内部诊断或其他畸形 JSON 的错误分类。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:203
# - tests/tooling/test_layout_dormant_contexts.py:206
# - tests/tooling/test_layout_dormant_contexts.py:207
# - tests/tooling/test_layout_dormant_contexts.py:208
# - tests/tooling/test_layout_dormant_contexts.py:209
# - scripts/layout/dormant_contexts.py:303
# - scripts/layout/dormant_contexts.py:304
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_excluded_cmake_subtree_cannot_add_an_owner [function]
# ## Summary
# 检查路径排除回调阻止 acceptance 子树的历史声明参与源 owner 恢复。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 本例有 current 与 acceptance 历史声明，但只为 current 提供 C group；嵌套 exclude 记录访问路径并拒绝 acceptance 前缀。断言 recovered、仅 current owner，并确认回调见到 tests/acceptance。不是任意异常/链接/并发删除路径的证明，也未测试所有被排除源入口。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:211
# - tests/tooling/test_layout_dormant_contexts.py:214
# - tests/tooling/test_layout_dormant_contexts.py:217
# - tests/tooling/test_layout_dormant_contexts.py:220
# - tests/tooling/test_layout_dormant_contexts.py:224
# - tests/tooling/test_layout_dormant_contexts.py:228
# - tests/tooling/test_layout_dormant_contexts.py:229
# - tests/tooling/test_layout_dormant_contexts.py:230
# - tests/tooling/test_layout_dormant_contexts.py:231
# - scripts/layout/dormant_contexts.py:191
# - tests/tooling/test_layout_dormant_contexts.py:247

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_excluded_cmake_subtree_cannot_add_an_owner::exclude [function]
# ## Summary
# 为该 case 记录扫描器询问的相对路径，并拒绝 tests/acceptance 前缀。
# ## Parameters
# @param[in] relative 扫描器给出的相对路径字符串。
# ## Returns
# @return 返回归一化路径是否以 tests/acceptance 开头。
# ## Constraints and lifecycle
# 闭包借用本方法局部 excluded_paths，调用时追加字符串；只在同步恢复查询期间使用，不跨 case 或线程保存。它不提供目录安全校验，路径匹配规则是本例人工 fixture 的限定选择。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:224
# - tests/tooling/test_layout_dormant_contexts.py:225
# - tests/tooling/test_layout_dormant_contexts.py:226
# - tests/tooling/test_layout_dormant_contexts.py:228
# - tests/tooling/test_layout_dormant_contexts.py:231
# - scripts/layout/dormant_contexts.py:191
# - scripts/layout/dormant_contexts.py:178
# - scripts/layout/dormant_contexts.py:244
# - scripts/layout/dormant_contexts.py:385

# @unit tests/tooling/test_layout_dormant_contexts.py::DormantContextTests.test_dormant_header_propagation_is_separate_from_actual_graph [function]
# ## Summary
# 检查独立 dormant seed 可传播到 support.h，并按排除回调停止进入 nested.h。
# ## Parameters
# @param[in,out] self 已成功 setUp 的 unittest 实例，仅写入其自有模拟树。
# ## Returns
# 无 Python 返回值；断言或意外异常交由 unittest 记录，成功 setUp 后 runner 安排 tearDown。进程最终状态由 unittest.main 处理。
# ## Constraints and lifecycle
# 建立 consumer→support→nested 字面 include 链；断言返回键精确为 consumer/support。actual 是未传给传播器的独立本地字典，断言其键不变且不含 consumer，只说明本例分图使用及返回集合，不扩大为所有集成路径或共享容器别名的不可变性证明。未断言完整 variants 内容、循环图或 IO 失败处理。
# ## References
# - tests/tooling/test_layout_dormant_contexts.py:233
# - tests/tooling/test_layout_dormant_contexts.py:234
# - tests/tooling/test_layout_dormant_contexts.py:235
# - tests/tooling/test_layout_dormant_contexts.py:236
# - tests/tooling/test_layout_dormant_contexts.py:237
# - tests/tooling/test_layout_dormant_contexts.py:238
# - tests/tooling/test_layout_dormant_contexts.py:240
# - tests/tooling/test_layout_dormant_contexts.py:241
# - tests/tooling/test_layout_dormant_contexts.py:242
# - tests/tooling/test_layout_dormant_contexts.py:243
# - scripts/layout/dormant_contexts.py:476
# - scripts/layout/dormant_contexts.py:490
# - tests/tooling/test_layout_dormant_contexts.py:247
