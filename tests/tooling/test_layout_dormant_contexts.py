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
