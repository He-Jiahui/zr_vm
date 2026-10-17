#!/usr/bin/env python3
"""保护旧语法扫描器的输入选择、分类协议和 CLI JSON 基线。"""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


def _repository_root_from_arguments() -> Path:
    """支持对独立 checkout 运行同一仓库扫描测试，并从 unittest 参数中移除自有选项。"""
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
FIXTURE_ROOT = REPOSITORY_ROOT / "tests" / "fixtures" / "syntax_migration_inventory"
INVENTORY_SCRIPT = SCRIPT_ROOT / "syntax_migration_inventory.py"
sys.path.insert(0, str(SCRIPT_ROOT))

import syntax_migration_inventory as inventory_module  # noqa: E402
from syntax_migration_inventory import (  # noqa: E402
    MigrationClassification,
    SourceKind,
    SourcePosition,
    SourceRange,
    _embedded_context_is_zr_input,
    _scan_path,
    build_inventory,
    build_repository_inventory,
    repository_candidate_paths,
)


class SyntaxMigrationInventoryProtocolTests(unittest.TestCase):
    """用人工 fixture 与真实仓库双重验证迁移清单，避免扫描器静默遗漏或误报。"""

    def test_repository_candidates_ignore_tracked_paths_missing_from_worktree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            current_path = root / "tests" / "current.zr"
            current_path.parent.mkdir(parents=True)
            current_path.write_text("fn current(): void {}\n", encoding="utf-8")

            with mock.patch.object(
                inventory_module,
                "_tracked_files",
                return_value=(Path("tests/current.zr"), Path("tests/deleted.zr")),
            ):
                self.assertEqual(
                    ("tests/current.zr",),
                    repository_candidate_paths(root),
                )

    def test_repository_inventory_reuses_unchanged_snapshot_and_invalidates_changes(self) -> None:
        """保证扫描缓存由文件内容失效；第二次同内容读取不能重新扫描。"""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "tests" / "sample.zr"
            source.parent.mkdir(parents=True)
            source.write_text("%module sample\n", encoding="utf-8")

            with mock.patch.object(
                inventory_module,
                "repository_candidate_paths",
                return_value=("tests/sample.zr",),
            ), mock.patch.object(
                inventory_module,
                "_scan_path",
                wraps=inventory_module._scan_path,
            ) as scan_path:
                first = build_repository_inventory(root)
                second = build_repository_inventory(root)
                self.assertEqual(first.to_json(), second.to_json())
                self.assertEqual(1, scan_path.call_count)

                source.write_text("%unknown changed\n", encoding="utf-8")
                third = build_repository_inventory(root)

            self.assertEqual(2, scan_path.call_count)
            self.assertNotEqual(first.to_json(), third.to_json())

    def test_fixture_inventory_has_stable_protocol_and_source_kinds(self) -> None:
        """将 source kind 与 JSON schema 作为工具消费者依赖的协议验收。"""
        report = build_inventory(FIXTURE_ROOT)
        payload = json.loads(report.to_json())

        self.assertEqual(1, payload["schemaVersion"])
        self.assertEqual(0, payload["unknownCount"])
        self.assertIsInstance(payload["findings"], list)
        self.assertEqual([], payload["exclusions"])
        self.assertEqual(
            [
                ("docs/current-snippets.md", SourceKind.CURRENT_DOCUMENTATION.value),
                ("embedded/embedded_legacy_fixture.c", SourceKind.EMBEDDED_ZR_FIXTURE.value),
                ("source/current_forms.zr", SourceKind.ZR_SOURCE.value),
                ("source/ignored_forms.zr", SourceKind.ZR_SOURCE.value),
                ("source/legacy_forms.zr", SourceKind.ZR_SOURCE.value),
            ],
            [(entry.file, entry.source_kind.value) for entry in report.scanned_files],
        )
        self.assertEqual(
            {
                "machineApplicable",
                "maybeIncorrect",
                "requiresReview",
                "blocked",
                "targetNotPromoted",
            },
            {classification.value for classification in MigrationClassification},
        )

    def test_lexes_legacy_forms_without_scanning_comments_strings_or_modulo(self) -> None:
        report = build_inventory(FIXTURE_ROOT)
        forms = {finding.legacy_form for finding in report.findings}

        self.assertTrue(
            {
                "percentModule",
                "percentImport",
                "percentAsync",
                "percentAwait",
                "percentExtern",
                "percentTest",
                "percentCompileTime",
                "percentFunc",
                "percentOwned",
                "percentRelease",
                "percentUpgrade",
                "percentWeak",
                "percentShared",
                "percentDetach",
                "percentUnique",
                "percentIn",
                "percentRef",
                "percentOut",
                "percentBorrow",
                "percentLoan",
                "percentBorrowed",
                "percentLoaned",
                "percentType",
                "percentUsing",
                "unrecognizedPercentDirective",
                "legacyFuncKeyword",
                "keywordlessFunction",
                "legacyDefinitionArrow",
                "legacyFunctionTypeArrow",
                "legacyLambdaWithoutFn",
                "legacyDollarConstruct",
                "legacyDynamicDollarConstruct",
                "legacyBareTypeCall",
                "legacyNewStruct",
                "nativePrototypeFactory",
                "legacyPropertyAccessor",
            }.issubset(forms)
        )
        self.assertFalse(
            any(
                finding.file in {"source/current_forms.zr", "source/ignored_forms.zr"}
                for finding in report.findings
            )
        )

        module_finding = next(
            finding
            for finding in report.findings
            if finding.file == "source/legacy_forms.zr"
            and finding.legacy_form == "percentModule"
        )
        self.assertEqual(
            SourceRange(
                start=SourcePosition(line=1, column=1),
                end=SourcePosition(line=1, column=8),
            ),
            module_finding.source_range,
        )
        self.assertTrue(
            any(
                finding.file == "embedded/embedded_legacy_fixture.c"
                and finding.legacy_form == "percentImport"
                for finding in report.findings
            )
        )
        self.assertTrue(
            any(
                finding.file == "docs/current-snippets.md"
                and finding.legacy_form == "percentModule"
                for finding in report.findings
            )
        )
        unsupported = next(
            finding
            for finding in report.findings
            if finding.legacy_form == "unrecognizedPercentDirective"
        )
        self.assertEqual(MigrationClassification.BLOCKED, unsupported.classification)

    def test_classifies_forms_by_promoted_target_and_matches_fixture_golden(self) -> None:
        report = build_inventory(FIXTURE_ROOT)
        expected = {
            "percentModule": (
                MigrationClassification.MACHINE_APPLICABLE,
                "06A",
                "module_directive_has_current_syntax",
            ),
            "percentImport": (
                MigrationClassification.REQUIRES_REVIEW,
                "06A",
                "import_binding_context_required",
            ),
            "percentAsync": (
                MigrationClassification.MACHINE_APPLICABLE,
                "12",
                "async_keyword_has_current_syntax",
            ),
            "percentAwait": (
                MigrationClassification.MACHINE_APPLICABLE,
                "12",
                "await_keyword_has_current_syntax",
            ),
            "percentExtern": (
                MigrationClassification.MACHINE_APPLICABLE,
                "10",
                "native_extern_has_current_syntax",
            ),
            "percentTest": (
                MigrationClassification.REQUIRES_REVIEW,
                "14",
                "test_metadata_function_required",
            ),
            "percentCompileTime": (
                MigrationClassification.MACHINE_APPLICABLE,
                "11",
                "comptime_keyword_has_current_syntax",
            ),
            "percentType": (
                MigrationClassification.MACHINE_APPLICABLE,
                "08",
                "runtime_type_query_has_current_syntax",
            ),
            "percentOwned": (
                MigrationClassification.MACHINE_APPLICABLE,
                "04",
                "resource_keyword_has_current_syntax",
            ),
            "percentRef": (
                MigrationClassification.MACHINE_APPLICABLE,
                "02",
                "parameter_contract_has_current_syntax",
            ),
            "percentBorrowed": (
                MigrationClassification.REQUIRES_REVIEW,
                "02",
                "borrow_place_and_escape_proof_required",
            ),
            "percentLoaned": (
                MigrationClassification.REQUIRES_REVIEW,
                "02",
                "loan_place_and_escape_proof_required",
            ),
            "legacyFuncKeyword": (
                MigrationClassification.MACHINE_APPLICABLE,
                "06A",
                "function_keyword_has_current_syntax",
            ),
            "legacyDollarConstruct": (
                MigrationClassification.MACHINE_APPLICABLE,
                "03",
                "static_constructor_has_current_syntax",
            ),
            "legacyLambdaWithoutFn": (
                MigrationClassification.REQUIRES_REVIEW,
                "06A",
                "lambda_keyword_has_current_syntax",
            ),
            "legacyDynamicDollarConstruct": (
                MigrationClassification.REQUIRES_REVIEW,
                "08",
                "dynamic_constructor_requires_reflection_review",
            ),
            "legacyBareTypeCall": (
                MigrationClassification.REQUIRES_REVIEW,
                "06A",
                "call_vs_constructor_binding_required",
            ),
            "legacyNewStruct": (
                MigrationClassification.REQUIRES_REVIEW,
                "06A",
                "new_struct_binding_required",
            ),
            "nativePrototypeFactory": (
                MigrationClassification.TARGET_NOT_PROMOTED,
                "10",
                "target_plan_not_promoted",
            ),
            "legacyPropertyAccessor": (
                MigrationClassification.REQUIRES_REVIEW,
                "05",
                "property_pairing_required",
            ),
            "unrecognizedPercentDirective": (
                MigrationClassification.BLOCKED,
                "06A",
                "unrecognized_legacy_directive",
            ),
        }
        for legacy_form, contract in expected.items():
            self.assertEqual(
                {contract},
                {
                    (finding.classification, finding.target_plan, finding.reason)
                    for finding in report.findings
                    if finding.legacy_form == legacy_form
                },
                legacy_form,
            )

        golden_path = FIXTURE_ROOT / "expected" / "inventory.json"
        self.assertEqual(golden_path.read_text(encoding="utf-8"), report.to_fixture_json())

    def test_repository_inventory_is_closed_deterministic_and_excludes_non_source_inputs(self) -> None:
        """把当前仓库输出与冻结 JSON 对齐，并核对每个候选文件必有扫描或排除结论。"""
        # TODO: 当前五处 wiki zr 围栏仍含旧语法，使 findings==[] 的仓库基线断言失败；
        # 应先核实并修正文档示例，再更新冻结清单，不要放宽扫描器或隐藏该失败。
        first = build_repository_inventory(REPOSITORY_ROOT)
        second = build_repository_inventory(REPOSITORY_ROOT)
        first_payload = json.loads(first.to_json())

        self.assertEqual(first.to_json(), second.to_json())
        self.assertEqual([], first_payload["findings"])
        self.assertEqual(
            {classification.value: 0 for classification in MigrationClassification},
            first_payload["classificationCounts"],
        )
        self.assertGreater(len(first_payload["reviewedCurrentFindings"]), 0)
        self.assertEqual(
            {"legacyBareTypeCall", "legacyNewStruct"},
            {
                entry["syntaxForm"]
                for entry in first_payload["reviewedCurrentFindings"]
            },
        )
        self.assertEqual(
            [
                (
                    "tests/language_server/test_lsp_current_syntax_formatting_cases.h",
                    14,
                    "percentCompileTime",
                ),
                (
                    "tests/language_server/test_lsp_current_syntax_formatting_cases.h",
                    28,
                    "percentFunc",
                ),
                (
                    "tests/language_server/test_lsp_current_syntax_formatting_cases.h",
                    39,
                    "legacyFunctionTypeArrow",
                ),
                (
                    "tests/language_server/test_lsp_interface.c",
                    21,
                    "percentUnique",
                ),
                (
                    "tests/language_server/test_lsp_project_features.c",
                    10,
                    "percentImport",
                ),
                (
                    "tests/parser/test_compiler_features.c",
                    46,
                    "percentUsing",
                ),
                (
                    "tests/parser/test_compiler_features.c",
                    67,
                    "percentUnique",
                ),
                (
                    "tests/parser/test_parser.c",
                    47,
                    "percentUsing",
                ),
                (
                    "tests/parser/test_percent_syntax_cutover.c",
                    42,
                    "unrecognizedPercentDirective",
                ),
                (
                    "tests/parser/test_property_consumer_contracts.c",
                    20,
                    "legacyPropertyAccessor",
                ),
                (
                    "tests/parser/test_property_consumer_contracts.c",
                    27,
                    "legacyPropertyAccessor",
                ),
                (
                    "tests/parser/test_property_consumer_contracts.c",
                    24,
                    "legacyPropertyAccessor",
                ),
                (
                    "tests/task/test_task_runtime.c",
                    27,
                    "unrecognizedPercentDirective",
                ),
                (
                    "tests/task/test_task_runtime.c",
                    24,
                    "unrecognizedPercentDirective",
                ),
            ],
            [
                (entry["file"], entry["column"], entry["legacyForm"])
                for entry in first_payload["allowlistedFindings"]
            ],
        )
        repository_golden = FIXTURE_ROOT / "expected" / "repository-inventory.json"
        self.assertEqual(
            repository_golden.read_text(encoding="utf-8"),
            first.to_json(),
        )
        self.assertEqual(0, first_payload["unknownCount"])
        self.assertIn("scannerVersion", first_payload)
        self.assertIn("selectedRoots", first_payload)
        self.assertIn("targetPlanCounts", first_payload)

        scanned = {entry.file for entry in first.scanned_files}
        excluded = {entry.file for entry in first.exclusions}
        self.assertFalse(scanned & excluded)
        self.assertEqual(
            set(repository_candidate_paths(REPOSITORY_ROOT)),
            scanned | excluded,
        )
        self.assertTrue(
            any(entry.reason == "binaryArtifact" for entry in first.exclusions)
        )
        self.assertTrue(
            any(entry.reason == "historicalPlan" for entry in first.exclusions)
        )
        exclusion_reasons = {entry.file: entry.reason for entry in first.exclusions}
        self.assertEqual(
            "legacyOrMigrationFixture",
            exclusion_reasons[
                "tests/fixtures/syntax_migration_frontend/input/review_and_blocked_forms.zr"
            ],
        )
        self.assertNotIn(
            "historicalLegacyParserFixture",
            exclusion_reasons.values(),
        )
        self.assertEqual(
            "expectedDiagnosticFixture",
            exclusion_reasons[
                "tests/fixtures/projects/syntax_reference_v1/negative/function_delimiters.zr"
            ],
        )
        self.assertIn("docs/zr_language_specification.md", scanned)
        self.assertFalse(
            any(entry.file.endswith((".zro", ".zri", ".zrs")) for entry in first.scanned_files)
        )
        self.assertTrue(
            all(
                finding.classification in MigrationClassification
                for finding in first.findings
            )
        )
        self.assertFalse(
            any(
                finding.file == "tests/benchmarks/native_runner/benchmark_support.c"
                and finding.legacy_form == "unrecognizedPercentDirective"
                for finding in first.findings
            ),
            "printf-style host strings are not embedded ZR source",
        )

    def test_embedded_context_recognizes_content_named_lsp_source_inputs(self) -> None:
        self.assertTrue(
            _embedded_context_is_zr_input(
                'const TZrChar *content = "',
                len('const TZrChar *content = "'),
                '%import("zr.math");',
            )
        )

    def test_repository_inventory_scans_current_syntax_reference(self) -> None:
        report = build_repository_inventory(REPOSITORY_ROOT)
        scanned_files = {entry.file for entry in report.scanned_files}

        self.assertIn("docs/zr_language_specification.md", scanned_files)

    def test_embedded_scan_tracks_adjacent_zr_source_literals(self) -> None:
        """宿主字符串拼接后仍需作为一个 ZR 输入扫描，避免多行 fixture 躲过迁移门禁。"""
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "adjacent_fixture.c"
            path.write_text(
                'const TZrChar *testCode =\n'
                '    "class Demo {\\n"\n'
                '    "}\\n"\n'
                '    "%import(\\\"zr.math\\\");\\n";\n',
                encoding="utf-8",
            )

            findings = _scan_path(
                path,
                file="adjacent_fixture.c",
                source_kind=SourceKind.EMBEDDED_ZR_FIXTURE,
                require_embedded_source_context=True,
            )

        self.assertEqual(["percentImport"], [finding.legacy_form for finding in findings])
        self.assertTrue(
            _embedded_context_is_zr_input(
                'const TZrChar *testCode = "',
                len('const TZrChar *testCode = "'),
                '%import("zr.math");',
            )
        )
        self.assertTrue(
            _embedded_context_is_zr_input(
                'const TZrChar *nativeContent = "',
                len('const TZrChar *nativeContent = "'),
                'native extern("zr.math") {}',
            )
        )

    def test_repository_inventory_has_no_machine_applicable_language_server_legacy_fixtures(self) -> None:
        report = build_repository_inventory(REPOSITORY_ROOT)
        findings = [
            finding
            for finding in report.findings
            if finding.file.startswith("tests/language_server/")
        ]

        self.assertEqual(
            [],
            [
                (finding.file, finding.legacy_form, finding.source_range.start.line)
                for finding in findings
                if finding.classification == MigrationClassification.MACHINE_APPLICABLE
            ],
        )

    def test_cli_json_output_is_utf8_lf_and_matches_the_repository_baseline(self) -> None:
        """确保命令行导出与库 API 使用同一稳定 JSON、UTF-8 及 LF 格式。"""
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_path = Path(temporary_directory) / "inventory.json"
            result = subprocess.run(
                [
                    sys.executable,
                    str(INVENTORY_SCRIPT),
                    "--root",
                    str(REPOSITORY_ROOT),
                    "--format",
                    "json",
                    "--output",
                    str(output_path),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            rendered = output_path.read_bytes()
            self.assertNotIn(b"\r\n", rendered)
            self.assertEqual(
                build_repository_inventory(REPOSITORY_ROOT).to_json().encode("utf-8"),
                rendered,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)

# @unit module entry and roots [config]
# ## Summary
# 在测试模块加载时解析自有仓库参数，固定生产脚本、人工 fixture 和 CLI 的路径，使同一组协议测试能针对明确的 checkout 运行。
# ## Parameters
# 这是模块初始化配置块，没有函数形参。输入来自 __file__、进程 sys.argv 以及被选根目录中的 scripts 和人工 fixture。
# ## Returns
# 初始化 REPOSITORY_ROOT、SCRIPT_ROOT、FIXTURE_ROOT 和 INVENTORY_SCRIPT，并把 SCRIPT_ROOT 插入 sys.path 首位后导入生产模块；不返回测试结果。
# ## Constraints and lifecycle
# 初始化发生在测试类定义和 unittest 参数解析之前；脚本目录的 sys.path 插入不在模块退出时撤销。正常导入会使用 Python 的 sys.modules 缓存，复用进程须防止已有同名模块来自其他 checkout。参数解析不验证根目录确有完整仓库。导入和文件读取仍可能抛异常；没有通用 fixture 回收钩子。应在独立解释器中顺序运行，外部并发改源会改变全仓用例输入。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:27
# tests/scripts/test_syntax_migration_inventory.py:29
# tests/scripts/test_syntax_migration_inventory.py:30
# tests/scripts/test_syntax_migration_inventory.py:31
# tests/scripts/test_syntax_migration_inventory.py:33
# tests/scripts/test_syntax_migration_inventory.py:34
# tests/scripts/test_syntax_migration_inventory.py:588
# scripts/syntax_migration_inventory.py:15

# @unit _repository_root_from_arguments [function]
# ## Summary
# 在 unittest 读取命令行前消费第一个 --repository 路径对，允许从指定 checkout 读取生产扫描器和测试数据。
# ## Parameters
# 没有显式形参；读取 sys.argv 和本文件 __file__。--repository 后一项被解释为路径，调用方负责传入期望的仓库根。
# ## Returns
# 返回 resolve() 后的 Path；未指定选项时返回测试文件 parents[2]。选项缺少后一项时抛 SystemExit；解析成功才删除该选项和值，其他解析异常不会转成测试返回码。
# ## Constraints and lifecycle
# 仅处理 sys.argv.index 找到的首次选项，不校验目标存在、类型或完整仓库结构。会原位修改共享 argv，因此必须在 unittest.main 前顺序调用，不适合与其他 argv 消费者并行。返回的 Path 是独立值，无需释放，不授予目录写入或清理所有权。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:14
# tests/scripts/test_syntax_migration_inventory.py:16
# tests/scripts/test_syntax_migration_inventory.py:17
# tests/scripts/test_syntax_migration_inventory.py:20
# tests/scripts/test_syntax_migration_inventory.py:22
# tests/scripts/test_syntax_migration_inventory.py:23
# tests/scripts/test_syntax_migration_inventory.py:27

# @unit SyntaxMigrationInventoryProtocolTests [type]
# ## Summary
# 组织人工 fixture 的词法与 JSON 协议检查、当前仓库快照检查以及 CLI/库输出对照；这些输入域承担不同的回归职责。
# ## Parameters
# 本类没有自定义构造参数、setUp 或 tearDown；实例创建和测试方法选择继承 unittest.TestCase 的协议，不能把仓库根当作构造参数。
# ## Returns
# 类定义本身不执行断言，也不产生通过状态；unittest 选择 11 个 test_ 方法后分别记录正常完成、失败或错误。
# ## Constraints and lifecycle
# 模块配置先完成。人工 fixture 只读；四个方法各持有自己的 TemporaryDirectory 上下文，mock.patch 上下文仅在其作用域替换生产对象。类没有在失败时统一清理全局生产缓存、sys.path 或其他方法资源的机制。顺序运行依赖进程内缓存；真实仓库用例受 Git 跟踪成员、文件元数据、源字节和冻结 golden 影响。测试字符串是扫描输入，不执行其中的 Zr/C 代码。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:47
# tests/scripts/test_syntax_migration_inventory.py:48
# tests/scripts/test_syntax_migration_inventory.py:51
# tests/scripts/test_syntax_migration_inventory.py:69
# tests/scripts/test_syntax_migration_inventory.py:509
# tests/scripts/test_syntax_migration_inventory.py:561
# tests/scripts/test_syntax_migration_inventory.py:588
# scripts/syntax_migration_inventory.py:1061

# @unit test_repository_candidates_ignore_tracked_paths_missing_from_worktree [function]
# ## Summary
# 用受控跟踪列表证明候选选择会过滤工作树中已经缺失的路径，避免把已删除的跟踪记录交给文件扫描。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；待测生产模块来自已初始化的全局配置，本用例的根目录在自己的 TemporaryDirectory 中创建，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 在独占 TemporaryDirectory 中创建 tests/current.zr，并把 _tracked_files 临时替换为一个现存路径和一个从未创建的路径；不调用真实 git ls-files。断言仅比较返回的候选元组，不覆盖删除与读取之间的竞争。mock.patch 和临时目录上下文在正常返回、断言或其他异常展开时退出；目录清理本身仍可能失败。方法不清空生产模块的其他全局状态。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:50
# tests/scripts/test_syntax_migration_inventory.py:51
# tests/scripts/test_syntax_migration_inventory.py:55
# tests/scripts/test_syntax_migration_inventory.py:57
# tests/scripts/test_syntax_migration_inventory.py:60
# tests/scripts/test_syntax_migration_inventory.py:62
# tests/scripts/test_syntax_migration_inventory.py:64
# scripts/syntax_migration_inventory.py:1124
# scripts/syntax_migration_inventory.py:1130

# @unit test_repository_inventory_reuses_unchanged_snapshot_and_invalidates_changes [function]
# ## Summary
# 以单文件候选和真实扫描包装计数，核对同一快照的第二次请求复用报告，并在本次文件重写后重新扫描。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；待测生产模块来自已初始化的全局配置，本用例的根目录在自己的 TemporaryDirectory 中创建，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 候选函数只在 mock 作用域返回 tests/sample.zr，_scan_path 使用 wraps 调用真实扫描。两次未改写请求的 JSON 相等且累计扫描一次；把内容从 %module sample 改为长度不同的 %unknown changed 后累计扫描两次且 JSON 改变。失效依据是生产签名中的路径和 stat 元数据，不是内容哈希；本例没有证明相同元数据下的内容改写可被察觉，也没有断言报告对象身份。patch 会恢复，TemporaryDirectory 会退出；生产缓存可继续持有已删除临时根对应的值直到后续替换或进程结束，本方法不显式清除它。不得并行替换这些模块全局函数。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:67
# tests/scripts/test_syntax_migration_inventory.py:73
# tests/scripts/test_syntax_migration_inventory.py:75
# tests/scripts/test_syntax_migration_inventory.py:79
# tests/scripts/test_syntax_migration_inventory.py:84
# tests/scripts/test_syntax_migration_inventory.py:87
# tests/scripts/test_syntax_migration_inventory.py:89
# tests/scripts/test_syntax_migration_inventory.py:90
# tests/scripts/test_syntax_migration_inventory.py:92
# tests/scripts/test_syntax_migration_inventory.py:93
# scripts/syntax_migration_inventory.py:1069
# scripts/syntax_migration_inventory.py:1083
# scripts/syntax_migration_inventory.py:1174
# scripts/syntax_migration_inventory.py:1267

# @unit test_fixture_inventory_has_stable_protocol_and_source_kinds [function]
# ## Summary
# 锁定人工 fixture 报告的 schema、来源类型排序和迁移分类枚举，保护消费 JSON 的工具对协议形状的依赖。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 只读 FIXTURE_ROOT 的五个人工源文件；build_inventory 的目录遍历不依赖 Git 跟踪列表。要求 schemaVersion=1、unknownCount=0、findings 是 list、exclusions 为空，并精确比较五个 scanned_files 的路径/来源顺序和五个分类枚举值。fixture 故意保留旧语法，本例不要求 findings 为空，也不在此比较整个 golden。报告和解析字典由 Python 管理；未创建文件、fixture 或外部进程，读取异常会中断当前方法。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:95
# tests/scripts/test_syntax_migration_inventory.py:97
# tests/scripts/test_syntax_migration_inventory.py:100
# tests/scripts/test_syntax_migration_inventory.py:103
# tests/scripts/test_syntax_migration_inventory.py:104
# tests/scripts/test_syntax_migration_inventory.py:112
# tests/scripts/test_syntax_migration_inventory.py:114
# tests/scripts/test_syntax_migration_inventory.py:122
# scripts/syntax_migration_inventory.py:929
# scripts/syntax_migration_inventory.py:951
# tests/fixtures/syntax_migration_inventory/docs/current-snippets.md:4

# @unit test_lexes_legacy_forms_without_scanning_comments_strings_or_modulo [function]
# ## Summary
# 在人工正负样例上区分待迁移语法与注释、字符串、求余及当前语法，并核对代表性来源和一个字符范围。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 所列 36 个形式只要求是实际 findings 形式集合的子集；不声称形式全集精确相等。current_forms.zr 和 ignored_forms.zr 不能产生 findings。只对 legacy_forms.zr 的 percentModule 比较一基行列 (1,1) 到 (1,8)，坐标是 Python 文本字符位置，不是文件字节或 LSP UTF-16。还要求宿主 fixture 的 percentImport、文档围栏的 percentModule 存在，并将未知指令分为 BLOCKED。next 找不到代表项会先抛 StopIteration；后面的范围或分类断言将不再执行。本例只扫描字符串，不编译或运行其中程序。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:125
# tests/scripts/test_syntax_migration_inventory.py:129
# tests/scripts/test_syntax_migration_inventory.py:167
# tests/scripts/test_syntax_migration_inventory.py:169
# tests/scripts/test_syntax_migration_inventory.py:176
# tests/scripts/test_syntax_migration_inventory.py:182
# tests/scripts/test_syntax_migration_inventory.py:189
# tests/scripts/test_syntax_migration_inventory.py:196
# tests/scripts/test_syntax_migration_inventory.py:203
# tests/scripts/test_syntax_migration_inventory.py:208
# scripts/syntax_migration_inventory.py:283
# tests/fixtures/syntax_migration_inventory/source/ignored_forms.zr:1
# tests/fixtures/syntax_migration_inventory/source/legacy_forms.zr:1

# @unit test_classifies_forms_by_promoted_target_and_matches_fixture_golden [function]
# ## Summary
# 把 21 种选定旧形式映射到分类、目标计划和理由的单一合同，再与人工 fixture 的稳定 JSON 投影比较。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# expected 表内每种形式的全部 finding 三元组集合必须精确等于一个指定三元组；这允许同合同的多处出现，不约束每形式只有一个 occurrence。全部表项断言满足后才读 expected/inventory.json，并比较 report.to_fixture_json 的完整文本。该投影按形式归并合同，不冻结每个源位置；冲突合同也可由生产投影抛 ValueError。只读人工源与 golden，无生成或更新 golden 的行为；前序映射断言失败时最终 golden 比较不可达。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:210
# tests/scripts/test_syntax_migration_inventory.py:211
# tests/scripts/test_syntax_migration_inventory.py:212
# tests/scripts/test_syntax_migration_inventory.py:319
# tests/scripts/test_syntax_migration_inventory.py:320
# tests/scripts/test_syntax_migration_inventory.py:330
# tests/scripts/test_syntax_migration_inventory.py:331
# scripts/syntax_migration_inventory.py:210
# scripts/syntax_migration_inventory.py:218
# tests/fixtures/syntax_migration_inventory/expected/inventory.json:2
# tests/fixtures/syntax_migration_inventory/expected/inventory.json:9

# @unit test_repository_inventory_is_closed_deterministic_and_excludes_non_source_inputs [function]
# ## Summary
# 检查同一仓库快照的 JSON 稳定性、待迁移清单门禁、冻结报告和候选扫描/排除闭包，使每个被选路径都有可审计归属。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 输入是当前 REPOSITORY_ROOT 的 Git 跟踪候选、工作树字节和 stat 签名，而非仅本文件或人工 fixture；全仓编辑、成员增删及缓存状态都可改变结果。先比较两次 JSON，再要求 findings 为空；只有这些前置断言通过，后续全零分类、reviewedCurrent、14 条 file/column/form allowlist、repository golden、元数据及扫描/排除闭包断言才可能执行。allowlist 的数字是列号，不是行号。后续闭包会重新请求候选列表，不构成并发快照隔离；本方法不清除缓存或修正源文件。
# TODO: tests/scripts/test_syntax_migration_inventory.py:335 的“五处 wiki”是已有历史解释，不能据此断言当前计数或当前首个失败。须在同一有限输入快照下核对 tests/scripts/test_syntax_migration_inventory.py:342、tests/scripts/test_syntax_migration_inventory.py:434 及 tests/scripts/test_syntax_migration_inventory.py:447 的实际可达结果，并沿 scripts/syntax_migration_inventory.py:1069、scripts/syntax_migration_inventory.py:1172 核对缓存/候选变化；未到达的 golden 和闭包断言不构成已验证信用。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:333
# tests/scripts/test_syntax_migration_inventory.py:335
# tests/scripts/test_syntax_migration_inventory.py:337
# tests/scripts/test_syntax_migration_inventory.py:341
# tests/scripts/test_syntax_migration_inventory.py:342
# tests/scripts/test_syntax_migration_inventory.py:347
# tests/scripts/test_syntax_migration_inventory.py:355
# tests/scripts/test_syntax_migration_inventory.py:429
# tests/scripts/test_syntax_migration_inventory.py:433
# tests/scripts/test_syntax_migration_inventory.py:434
# tests/scripts/test_syntax_migration_inventory.py:445
# tests/scripts/test_syntax_migration_inventory.py:447
# tests/scripts/test_syntax_migration_inventory.py:457
# tests/scripts/test_syntax_migration_inventory.py:467
# tests/scripts/test_syntax_migration_inventory.py:473
# tests/scripts/test_syntax_migration_inventory.py:483
# scripts/syntax_migration_inventory.py:1069
# scripts/syntax_migration_inventory.py:1172
# tests/fixtures/syntax_migration_inventory/expected/repository-inventory.json:1

# @unit test_embedded_context_recognizes_content_named_lsp_source_inputs [function]
# ## Summary
# 保护名为 content 的宿主字符串输入识别，避免只接受 testCode 等常见命名而漏掉 LSP 的内容变量。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 向纯上下文谓词提供一个 content 赋值前缀、该前缀的字符长度和以 %import 开头的 payload，断言返回真。没有读取真实 LSP 文件、构建语义查询或执行 payload；一个正例不能证明所有变量名、窗口边界、非 Zr 字符串或转义处理。局部 str 由 Python 管理，无文件、进程或需要关闭的生成器。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:492
# tests/scripts/test_syntax_migration_inventory.py:494
# tests/scripts/test_syntax_migration_inventory.py:495
# tests/scripts/test_syntax_migration_inventory.py:496
# tests/scripts/test_syntax_migration_inventory.py:497
# scripts/syntax_migration_inventory.py:809
# scripts/syntax_migration_inventory.py:814
# scripts/syntax_migration_inventory.py:841

# @unit test_repository_inventory_scans_current_syntax_reference [function]
# ## Summary
# 要求当前语言规范文档被仓库清单列为已扫描输入，避免根目录或来源选择规则遗漏规范。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 调用当前仓库报告构建 API 后只检查 scanned_files 中包含 docs/zr_language_specification.md；允许 API 命中已有进程缓存。该断言不检查规范是否无 finding、报告是否等于 golden、全部候选闭包或是否发生新扫描。全仓输入、Git 子进程和读取错误仍会在成员断言前使方法失败；本方法不修改规范或清理全局缓存。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:501
# tests/scripts/test_syntax_migration_inventory.py:502
# tests/scripts/test_syntax_migration_inventory.py:503
# tests/scripts/test_syntax_migration_inventory.py:505
# scripts/syntax_migration_inventory.py:1170
# scripts/syntax_migration_inventory.py:1175
# docs/zr_language_specification.md:1

# @unit test_embedded_scan_tracks_adjacent_zr_source_literals [function]
# ## Summary
# 为分行宿主字符串的连续 source 序列保留迁移扫描入口，使首片段之后的 import 仍被上下文过滤器接受。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；待测生产函数来自已初始化的全局导入，人工宿主文件在本用例自己的 TemporaryDirectory 中创建，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 在自有 TemporaryDirectory 写入人工 C 字符串文本，以 require_embedded_source_context=True 调用 _scan_path。C 内容只供有限宿主字面量扫描，不由 C 编译器或 Zr runtime 执行。方法退出临时目录上下文后才断言 finding 形式恰为一个 percentImport，并另核 testCode/import 和 nativeContent/native 两个正向上下文。只比较形式，不比较 host source_range，也不证明完整 C 拼接/转义语义。文件在上下文展开时清理，已构造的 Python finding 值可继续用于随后断言；清理异常会先阻止这些断言。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:507
# tests/scripts/test_syntax_migration_inventory.py:509
# tests/scripts/test_syntax_migration_inventory.py:511
# tests/scripts/test_syntax_migration_inventory.py:519
# tests/scripts/test_syntax_migration_inventory.py:523
# tests/scripts/test_syntax_migration_inventory.py:526
# tests/scripts/test_syntax_migration_inventory.py:528
# tests/scripts/test_syntax_migration_inventory.py:535
# scripts/syntax_migration_inventory.py:845
# scripts/syntax_migration_inventory.py:859
# scripts/syntax_migration_inventory.py:868
# scripts/syntax_migration_inventory.py:872
# scripts/syntax_migration_embedded.py:19
# scripts/syntax_migration_embedded.py:46

# @unit test_repository_inventory_has_no_machine_applicable_language_server_legacy_fixtures [function]
# ## Summary
# 对当前仓库尚待迁移的 LSP finding 应用自动迁移门禁，防止 machineApplicable 旧形式继续留在服务端测试源中。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 仅从 report.findings 选择 tests/language_server/ 前缀项，再拒绝 classification 为 MACHINE_APPLICABLE 的条目；未拒绝其他分类，也没有遍历 allowlisted_findings 或 reviewed_current_findings。可复用进程内仓库缓存，不证明目录内容被重新扫描或全部旧形式消失。只读当前仓库报告，不改 LSP fixture；报告构建或断言失败由 unittest 记录。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:542
# tests/scripts/test_syntax_migration_inventory.py:543
# tests/scripts/test_syntax_migration_inventory.py:544
# tests/scripts/test_syntax_migration_inventory.py:547
# tests/scripts/test_syntax_migration_inventory.py:550
# tests/scripts/test_syntax_migration_inventory.py:555
# scripts/syntax_migration_inventory.py:1174
# scripts/syntax_migration_inventory.py:1206

# @unit test_cli_json_output_is_utf8_lf_and_matches_the_repository_baseline [function]
# ## Summary
# 在独立 CLI 进程与当前测试进程之间比较同一仓库报告的输出字节，保护 UTF-8/LF 序列化协议。
# ## Parameters
# self 是 unittest 为当前用例管理的 TestCase 实例；根目录和生产模块来自本模块已初始化的全局配置，不作为方法形参传入。
# ## Returns
# 所有可达断言满足时隐式返回 None；断言失败抛出 AssertionError，其他未捕获异常由 unittest 记录为错误。该方法不返回状态码；同一方法中较早的失败会阻止后续断言执行。
# ## Constraints and lifecycle
# 在自有 TemporaryDirectory 指定 inventory.json，使用 sys.executable 执行生产脚本 --root/--format json/--output；子进程 check=False，捕获文本 stdout/stderr，没有显式 cwd、timeout、env 或 -B。必须先返回码为零，随后才读文件、拒绝 CRLF 并与当前进程 API 的 UTF-8 JSON 字节比较；不比较冻结 repository golden，也不要求 findings 为空。父解释器的 -B 标志不自动成为子进程参数，若禁止字节码写入须由继承环境 PYTHONDONTWRITEBYTECODE 或隔离缓存策略保证。CLI 与父进程扫描之间的并发源码/成员变化可能使字节不同；没有超时隔离。上下文负责正常和异常路径的临时输出清理，子进程启动、输出读取或清理异常仍可终止当前用例。
# ## References
# tests/scripts/test_syntax_migration_inventory.py:559
# tests/scripts/test_syntax_migration_inventory.py:561
# tests/scripts/test_syntax_migration_inventory.py:563
# tests/scripts/test_syntax_migration_inventory.py:565
# tests/scripts/test_syntax_migration_inventory.py:567
# tests/scripts/test_syntax_migration_inventory.py:571
# tests/scripts/test_syntax_migration_inventory.py:574
# tests/scripts/test_syntax_migration_inventory.py:578
# tests/scripts/test_syntax_migration_inventory.py:579
# tests/scripts/test_syntax_migration_inventory.py:580
# tests/scripts/test_syntax_migration_inventory.py:582
# scripts/syntax_migration_inventory.py:1272
# scripts/syntax_migration_inventory.py:1281
# scripts/syntax_migration_inventory.py:1286
# scripts/syntax_migration_inventory.py:1287
