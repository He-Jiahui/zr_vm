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


REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "scripts" / "layout" / "apply_moves.py"
SPEC = importlib.util.spec_from_file_location("apply_moves", TOOL)
assert SPEC and SPEC.loader
migration = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = migration
SPEC.loader.exec_module(migration)

# The sandbox must stay under the tool's own registered fixture root, since
# --root is rejected outside it.  Both sides read the same portable constant.
FIXTURE_ROOT = migration.REGISTERED_FIXTURE_ROOT


class LayoutMoveTests(unittest.TestCase):
    def setUp(self) -> None:
        FIXTURE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case-", dir=FIXTURE_ROOT)
        self.root = Path(self.temp.name)
        (self.root / "scripts/layout").mkdir(parents=True)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def mapping(self, rows: str) -> None:
        (self.root / "scripts/layout/layout_map.tsv").write_text(rows, encoding="utf-8")

    def run_tool(self, *args: str) -> tuple[int, str]:
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            code = migration.main(["--root", str(self.root), *args])
        return code, output.getvalue()

    def fixture_module(self) -> tuple[Path, Path]:
        module = self.root / "zr_vm_demo/src/zr_vm_demo"
        module.mkdir(parents=True)
        source = module / "demo.c"
        header = module / "demo_internal.h"
        source.write_bytes(b"\xef\xbb\xbf#include \"demo_internal.h\"\r\n/* zr_vm_demo/src/zr_vm_demo/demo.c */\r\n")
        header.write_bytes(b"#define DEMO 1\n")
        return source, header

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

    def test_preview_does_not_mutate_dirty_bytes_or_paths(self) -> None:
        source, _ = self.fixture_module()
        original = source.read_bytes()
        self.mapping("zr_vm_demo/src/zr_vm_demo/demo.c\tzr_vm_demo/src/zr_vm_demo/group/demo.c\n")
        code, output = self.run_tool("--dry-run")
        self.assertEqual(code, 0, output)
        self.assertIn("preview:", output)
        self.assertEqual(source.read_bytes(), original)
        self.assertFalse((self.root / "zr_vm_demo/src/zr_vm_demo/group/demo.c").exists())

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

    def test_empty_private_directory_suffix_does_not_create_module_short_alias(self) -> None:
        old_root = "zr_vm_parser/src/zr_vm_parser"
        nested_root = old_root + "/zr_vm_parser/src/zr_vm_parser"
        rewriter = migration.path_rewriter(
            str(self.root.resolve()), (), ((old_root, nested_root),))
        token = b"zr_vm_parser/compiler/compiler_internal.h"
        self.assertEqual(rewriter.replace(token), token)

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

        def deny_generated_read(path: Path) -> bytes:
            if path == generated_header:
                raise AssertionError("generated header contents must never be read")
            return original_read_bytes(path)

        with patch.object(Path, "read_bytes", deny_generated_read):
            problems = migration.unresolved_includes(self.root, context, build=build)
        self.assertEqual(problems, [])
        self.assertEqual(migration.sha256(original_read_bytes(generated_header)), original_hash)

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

    def test_migration_test_fixture_is_excluded_from_path_scans(self) -> None:
        self.assertTrue(migration.excluded("tests/tooling/test_layout_moves.py"))
        self.assertFalse(migration.is_scan_file(
            "tests/tooling/test_layout_moves.py", self.root / "tests/tooling/test_layout_moves.py"))

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
