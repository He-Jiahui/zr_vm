#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


def _repository_root_from_arguments() -> Path:
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
    def test_fixture_inventory_has_stable_protocol_and_source_kinds(self) -> None:
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
        self.assertEqual(
            "historicalLegacyParserFixture",
            exclusion_reasons["tests/fixtures/scripts/closures.zr"],
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
