#!/usr/bin/env python3

from __future__ import annotations

import copy
import importlib
import importlib.util
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT_DIR = REPO_ROOT / "scripts" / "benchmark"
sys.path.insert(0, str(SCRIPT_DIR))

import aggregate_benchmark_summary as aggregate  # noqa: E402
import benchmark_environment_contract as environment_contract  # noqa: E402
import test_benchmark_environment_contract as environment_tests  # noqa: E402


def _task4_module():
    spec = importlib.util.find_spec("benchmark_task4_contract")
    if spec is None:
        return None
    return importlib.import_module("benchmark_task4_contract")


def _publisher_module():
    spec = importlib.util.find_spec("benchmark_report_publisher")
    if spec is None:
        return None
    return importlib.import_module("benchmark_report_publisher")


def _implementation(**updates: object) -> dict[str, object]:
    record: dict[str, object] = {
        "id": "zr_interp",
        "name": "ZR interp",
        "status": "PASS",
        "measurement_scope": "persistent_runtime",
        "prepare_scope": "bytecode_compile_and_load_before_measurement",
        "runtime_reused": True,
        "compiler_reused": False,
        "jit_state_reused": False,
        "stability": "STABLE",
        "comparable": True,
        "gate_eligible": True,
        "summary": {"median_wall_ms": 5.0},
        "relative_to_c": 999.0,
        "speedup": 999.0,
    }
    record.update(updates)
    return record


def _benchmark_report(**updates: object) -> dict[str, object]:
    report: dict[str, object] = {
        "schema_version": 3,
        "suite": "performance_report",
        "tier": "core",
        "scope_mode": "steady",
        "measurement_policy": {
            "profile": False,
            "warmup": 5,
            "initial_sample_count": 10,
            "max_extra_sample_count": 10,
            "minimum_sample_ms_source": "registry",
            "calibration_strategy": "power_of_two_repetition_doubling",
            "stability": {
                "metric": "coefficient_of_variation",
                "maximum": 0.05,
            },
            "bootstrap": {
                "statistic": "median",
                "confidence": 0.95,
                "seed": "0",
            },
            "profile_comparable": False,
        },
        "cases": [
            {
                "name": "numeric_loops",
                "expected_checksum": 1234,
                "implementations": [
                    _implementation(),
                    _implementation(
                        id="c",
                        name="C",
                        prepare_scope="none",
                        runtime_reused=False,
                        summary={"median_wall_ms": 10.0},
                    ),
                ],
            }
        ],
    }
    report.update(updates)
    return report


def _summary(
    *,
    report: dict[str, object] | None = None,
    environment: dict[str, object] | None = None,
) -> dict[str, object]:
    return {
        "schema_version": 3,
        "reports": {
            "benchmark_report": report or _benchmark_report(),
            "environment_report": environment
            or environment_tests._fingerprinted_environment(),
        },
    }


def _refingerprint(environment: dict[str, object]) -> dict[str, object]:
    environment.pop("stable_fingerprint", None)
    return environment_contract.environment_with_fingerprint(environment)


class Task4ReportContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.task4 = _task4_module()

    def require_task4(self):
        self.assertIsNotNone(
            self.task4,
            "missing scripts/benchmark/benchmark_task4_contract.py",
        )
        return self.task4

    def test_complete_environment_is_attached_to_schema3_report(self) -> None:
        task4 = self.require_task4()
        environment = environment_tests._fingerprinted_environment()

        attached = task4.attach_environment_contract(
            _benchmark_report(),
            environment,
            report_reference="environment_report.json",
        )

        self.assertEqual("COMPARABLE", attached["environment"]["status"])
        self.assertEqual(
            environment["stable_fingerprint"]["value"],
            attached["environment"]["fingerprint"],
        )
        self.assertEqual("ISOLATED", attached["environment"]["isolation_status"])
        self.assertEqual("environment_report.json", attached["environment"]["report_path"])
        implementation = attached["cases"][0]["implementations"][0]
        self.assertEqual(0.5, implementation["relative_to_c"])
        self.assertTrue(implementation["gate_eligible"])

    def test_incomplete_or_invalid_environment_is_rejected(self) -> None:
        task4 = self.require_task4()
        environment = environment_tests._fingerprinted_environment()
        environment["capture_status"] = "IN_PROGRESS"

        with self.assertRaisesRegex(ValueError, "CAPTURE_INCOMPLETE"):
            task4.attach_environment_contract(_benchmark_report(), environment)

        unavailable = environment_tests._fingerprinted_environment()
        unavailable["build"]["target_evidence"] = {
            "status": "unavailable",
            "entry_count": 0,
            "sha256": None,
        }
        with self.assertRaisesRegex(ValueError, "MISSING_BUILD_TARGET_EVIDENCE"):
            task4.attach_environment_contract(_benchmark_report(), unavailable)

    def test_nonisolated_source_changed_and_profile_null_ratios_and_gates(self) -> None:
        task4 = self.require_task4()
        environments_and_reports: list[tuple[dict[str, object], dict[str, object], str]] = []

        nonisolated = environment_tests._fingerprinted_environment()
        nonisolated["isolation"].update(
            {"status": "NON_ISOLATED", "selected_cpu": None, "observed_mask": "0-3"}
        )
        environments_and_reports.append(
            (_refingerprint(nonisolated), _benchmark_report(), "NON_ISOLATED")
        )

        changed = environment_tests._fingerprinted_environment()
        changed["source"]["after"] = {
            **changed["source"]["after"],
            "dirty": True,
            "dirty_tree_digest": "b" * 64,
        }
        changed["source"]["changed_during_run"] = True
        environments_and_reports.append(
            (_refingerprint(changed), _benchmark_report(), "SOURCE_CHANGED_DURING_RUN")
        )

        profile_report = _benchmark_report()
        profile_report["measurement_policy"]["profile"] = True
        environments_and_reports.append(
            (
                environment_tests._fingerprinted_environment(),
                profile_report,
                "PROFILE_NONCOMPARABLE",
            )
        )

        for environment, report, expected_reason in environments_and_reports:
            with self.subTest(expected_reason=expected_reason):
                attached = task4.attach_environment_contract(report, environment)
                self.assertEqual("INCOMPARABLE", attached["environment"]["status"])
                self.assertIn(expected_reason, attached["environment"]["reasons"])
                record = attached["cases"][0]["implementations"][0]
                self.assertIsNone(record["relative_to_c"])
                self.assertIsNone(record["gate_eligible"])
                self.assertNotIn("speedup", record)

    def test_baseline_comparison_recomputes_speedup_from_raw_medians(self) -> None:
        task4 = self.require_task4()
        current = _summary()
        baseline_report = _benchmark_report()
        baseline_record = baseline_report["cases"][0]["implementations"][0]
        baseline_record["summary"]["median_wall_ms"] = 10.0
        baseline_record["speedup"] = 0.001
        baseline = _summary(report=baseline_report)

        comparison = task4.compare_benchmark_summaries(current, baseline)

        self.assertEqual("COMPARABLE", comparison["status"])
        self.assertEqual([], comparison["reasons"])
        self.assertEqual(2, len(comparison["records"]))
        result = next(
            record
            for record in comparison["records"]
            if record["implementation"] == "zr_interp"
        )
        self.assertEqual(2.0, result["baseline_to_current_speedup"])
        self.assertEqual(5.0, result["current_median_wall_ms"])
        self.assertEqual(10.0, result["baseline_median_wall_ms"])

    def test_baseline_comparison_rejects_every_required_contract_mismatch(self) -> None:
        task4 = self.require_task4()
        mutations = []

        def environment_mutation(path: tuple[str, ...], value: object):
            def mutate(summary: dict[str, object]) -> None:
                node = summary["reports"]["environment_report"]
                for key in path[:-1]:
                    node = node[key]
                node[path[-1]] = value
                summary["reports"]["environment_report"] = _refingerprint(
                    summary["reports"]["environment_report"]
                )

            return mutate

        mutations.extend(
            [
                (environment_mutation(("cpu", "model"), "different CPU"), "CPU_MODEL_MISMATCH"),
                (
                    environment_mutation(("build", "configuration"), "Debug"),
                    "BUILD_CONFIGURATION_MISMATCH",
                ),
                (
                    environment_mutation(
                        ("runtimes", "python", "version"), "Python 9.9"
                    ),
                    "RUNTIME_VERSIONS_MISMATCH",
                ),
            ]
        )

        def mutate_report(callback):
            def mutate(summary: dict[str, object]) -> None:
                callback(summary["reports"]["benchmark_report"])

            return mutate

        mutations.extend(
            [
                (
                    mutate_report(
                        lambda report: report["cases"][0]["implementations"][0].update(
                            measurement_scope="process_end_to_end"
                        )
                    ),
                    "MEASUREMENT_SCOPE_MISMATCH",
                ),
                (
                    mutate_report(
                        lambda report: report["cases"][0].update(expected_checksum=999)
                    ),
                    "CHECKSUM_CONTRACT_MISMATCH",
                ),
                (
                    mutate_report(
                        lambda report: report["measurement_policy"].update(warmup=99)
                    ),
                    "MEASUREMENT_POLICY_MISMATCH",
                ),
                (
                    mutate_report(
                        lambda report: report["measurement_policy"]["bootstrap"].update(
                            statistic="mean"
                        )
                    ),
                    "STATISTICAL_ALGORITHM_MISMATCH",
                ),
                (
                    mutate_report(
                        lambda report: report["measurement_policy"].update(profile=True)
                    ),
                    "BASELINE_PROFILE_NONCOMPARABLE",
                ),
                (
                    mutate_report(
                        lambda report: report["cases"][0]["implementations"][0].update(
                            stability="UNSTABLE"
                        )
                    ),
                    "BASELINE_RECORD_UNSTABLE",
                ),
                (
                    mutate_report(
                        lambda report: report["cases"][0]["implementations"][0].update(
                            comparable=False
                        )
                    ),
                    "BASELINE_RECORD_NONCOMPARABLE",
                ),
                (
                    mutate_report(
                        lambda report: report["cases"][0]["implementations"][0].update(
                            gate_eligible=False
                        )
                    ),
                    "BASELINE_RECORD_NOT_GATE_ELIGIBLE",
                ),
                (
                    mutate_report(
                        lambda report: report["cases"].append(copy.deepcopy(report["cases"][0]))
                    ),
                    "BASELINE_DUPLICATE_CASE",
                ),
                (
                    mutate_report(
                        lambda report: report["cases"][0]["implementations"].append(
                            copy.deepcopy(report["cases"][0]["implementations"][0])
                        )
                    ),
                    "BASELINE_DUPLICATE_IMPLEMENTATION",
                ),
            ]
        )

        for mutate, expected_reason in mutations:
            baseline = _summary()
            mutate(baseline)
            with self.subTest(expected_reason=expected_reason):
                comparison = task4.compare_benchmark_summaries(_summary(), baseline)
                self.assertEqual("INCOMPARABLE", comparison["status"])
                self.assertIn(expected_reason, comparison["reasons"])
                self.assertEqual([], comparison["records"])

    def test_missing_environment_is_explicitly_incomparable(self) -> None:
        task4 = self.require_task4()
        baseline = _summary()
        del baseline["reports"]["environment_report"]

        comparison = task4.compare_benchmark_summaries(_summary(), baseline)

        self.assertEqual("INCOMPARABLE", comparison["status"])
        self.assertIn("BASELINE_ENVIRONMENT_MISSING", comparison["reasons"])


class Task4AggregateAndCacheTests(unittest.TestCase):
    def test_aggregate_baseline_summary_is_fail_closed(self) -> None:
        task4 = _task4_module()
        self.assertIsNotNone(task4, "missing Task4 comparison helper")
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            perf = root / "performance"
            perf.mkdir()
            (perf / "benchmark_report.json").write_text(
                json.dumps(_benchmark_report()), encoding="utf-8"
            )
            baseline = root / "baseline.json"
            baseline.write_text(json.dumps(_summary()), encoding="utf-8")

            summary = aggregate._build_summary(
                root,
                "performance",
                baseline_summary=baseline,
            )

            self.assertEqual("INCOMPARABLE", summary["baseline_comparison"]["status"])
            self.assertIn(
                "CURRENT_ENVIRONMENT_MISSING",
                summary["baseline_comparison"]["reasons"],
            )

    def test_cache_key_cli_derives_comparable_actual_build_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            repository = root / "repo"
            repository.mkdir()
            subprocess.run(["git", "init", "-q", str(repository)], check=True)
            (repository / "fixture.c").write_text("int main(void) { return 0; }\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(repository), "add", "fixture.c"], check=True)
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(repository),
                    "-c",
                    "user.name=Task4 Test",
                    "-c",
                    "user.email=task4@example.invalid",
                    "commit",
                    "-qm",
                    "fixture",
                ],
                check=True,
            )
            build = root / "build"
            environment_tests._write_cmake_cache(build)
            output = root / "cache-key.json"
            compiler = shutil.which("gcc") or shutil.which("cc")
            if compiler is None:
                self.skipTest("a GCC-compatible compiler is required")
            completed = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT_DIR / "benchmark_environment_contract.py"),
                    "cache-key",
                    "--repo-root",
                    str(repository),
                    "--build-dir",
                    str(build),
                    "--toolchain",
                    "gcc",
                    "--compiler-path",
                    compiler,
                    "--compiler-version",
                    "gcc test",
                    "--compiler-target",
                    "x86_64-test",
                    "--generator",
                    "Ninja",
                    "--configuration",
                    "Release",
                    "--flag",
                    "CMAKE_C_FLAGS=",
                    "--flag",
                    "CMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG",
                    "--flag",
                    "CMAKE_EXE_LINKER_FLAGS=",
                    "--flag",
                    "CMAKE_EXE_LINKER_FLAGS_RELEASE=",
                    "--flag",
                    "CMAKE_SHARED_LINKER_FLAGS=",
                    "--flag",
                    "CMAKE_SHARED_LINKER_FLAGS_RELEASE=",
                    "--output",
                    str(output),
                ],
                text=True,
                capture_output=True,
            )
            self.assertEqual(0, completed.returncode, completed.stderr)
            identity = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(identity["comparable"])
            self.assertEqual(
                "available",
                identity["toolchain_contract"]["build_contract"]["target_evidence"]["status"],
            )
            self.assertEqual(
                f"{identity['source_key']}/{identity['toolchain_key']}",
                identity["relative_path"],
            )


class Task4PublisherTests(unittest.TestCase):
    def test_publish_is_manifested_immutable_atomic_and_excludes_binaries(self) -> None:
        publisher = _publisher_module()
        self.assertIsNotNone(
            publisher,
            "missing scripts/benchmark/benchmark_report_publisher.py",
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "tests_generated"
            performance = source / "performance"
            performance.mkdir(parents=True)
            (performance / "benchmark_report.json").write_text("{}\n", encoding="utf-8")
            (performance / "benchmark_report.md").write_text("# report\n", encoding="utf-8")
            binary = source / "performance_suite" / "toolchains" / "runner.exe"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"MZ\x00\x01")
            destination = root / "published"

            result = publisher.publish_report_bundle(
                source,
                destination,
                run_id="run-001",
            )

            run_directory = pathlib.Path(result["run_directory"])
            self.assertTrue((run_directory / "SHA256SUMS").is_file())
            self.assertTrue((run_directory / "performance" / "benchmark_report.json").is_file())
            self.assertFalse((run_directory / "performance_suite").exists())
            self.assertEqual("run-001", (destination / "LATEST").read_text(encoding="utf-8").strip())
            with self.assertRaises(FileExistsError):
                publisher.publish_report_bundle(source, destination, run_id="run-001")


class Task4ShellContractTests(unittest.TestCase):
    def test_release_build_uses_full_comparable_cache_identity(self) -> None:
        script = (SCRIPT_DIR / "build_benchmark_release.sh").read_text(encoding="utf-8")
        self.assertIn('HOME}/.cache/zr-vm-benchmark', script)
        self.assertIn('-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', script)
        self.assertIn('cache-key', script)
        self.assertIn('--build-dir', script)
        self.assertIn('relative_path', script)
        self.assertIn('source/build contract changed', script)
        self.assertNotIn('cmake --build "${bootstrap_root}"', script)
        self.assertNotIn('mv "${bootstrap_root}" "${build_dir}"', script)
        self.assertIn('cmake -S "${repo_root}" -B "${build_dir}"', script)
        self.assertIn('cmake --build "${build_dir}"', script)
        self.assertIn('cache hit', script)

    def test_wsl_runner_captures_attaches_compares_and_publishes(self) -> None:
        script = (SCRIPT_DIR / "run_wsl_benchmarks_report_csv.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn('capture_benchmark_environment.sh', script)
        self.assertIn('environment_report.json', script)
        self.assertIn('benchmark_task4_contract.py', script)
        self.assertIn('--baseline-summary', script)
        self.assertIn('benchmark_report_publisher.py', script)
        self.assertIn('ZR_VM_BENCHMARK_REPORT_DESTINATION', script)


if __name__ == "__main__":
    unittest.main()
