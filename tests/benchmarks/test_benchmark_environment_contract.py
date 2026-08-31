#!/usr/bin/env python3
from __future__ import annotations

import copy
import hashlib
import json
import os
import platform
import shlex
import shutil
import subprocess
import tempfile
import time
import unittest
from unittest import mock
from pathlib import Path
import sys


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CONTRACT_MODULE = (
    REPOSITORY_ROOT / "scripts" / "benchmark" / "benchmark_environment_contract.py"
)
CAPTURE_WRAPPER = (
    REPOSITORY_ROOT / "scripts" / "benchmark" / "capture_benchmark_environment.sh"
)
TEST_SUBPROCESS_TIMEOUT_SECONDS = 60
REQUIRED_RUNTIME_NAMES = (
    "cargo",
    "dotnet",
    "java",
    "javac",
    "lua",
    "node",
    "python",
    "qjs",
    "valgrind",
)
sys.path.insert(0, str(CONTRACT_MODULE.parent))

import benchmark_environment_contract as contract  # noqa: E402
import benchmark_source_identity as source_identity_module  # noqa: E402


def _environment() -> dict[str, object]:
    unavailable_runtime = {"status": "unavailable", "path": None, "version": None}
    source_snapshot = {
        "contract_version": contract.SOURCE_IDENTITY_CONTRACT_VERSION,
        "commit": "1" * 40,
        "dirty": False,
        "dirty_tree_digest": None,
    }
    return {
        "schema_version": contract.ENVIRONMENT_SCHEMA_VERSION,
        "capture_status": "COMPLETE",
        "platform": {
            "system": "Linux",
            "architecture": "x86_64",
            "kernel_release": "6.18.33.2-microsoft-standard-WSL2",
            "wsl_release": "WSL2",
        },
        "cpu": {
            "model": "AMD Ryzen 7 5800H with Radeon Graphics",
            "logical_count": 16,
        },
        "compiler": {
            "path": "/usr/bin/gcc",
            "version": "gcc (Ubuntu 11.4.0) 11.4.0",
            "target": "x86_64-linux-gnu",
        },
        "build": {
            "contract_version": 1,
            "generator": "Ninja",
            "configuration": "Release",
            "flags": [
                {"name": "CMAKE_C_FLAGS", "value": ""},
                {"name": "CMAKE_C_FLAGS_RELEASE", "value": "-O3 -DNDEBUG"},
            ],
            "performance_options": {
                "build_shared_libs": "OFF",
                "interprocedural_optimization": "ON",
                "sanitizers": [],
                "abi_runtime": [],
                "project_compile_definitions": [
                    {"name": "ZR_VM_TARGET_COMPILE_DEFINITIONS", "value": "ZR_FAST=1"},
                ],
                "project_compile_features": [],
                "project_compile_options": [
                    {"name": "ZR_VM_TARGET_COMPILE_OPTIONS", "value": "-fno-omit-frame-pointer"},
                ],
            },
            "toolchain_file": {"path": None, "sha256": None},
            "target_evidence": {
                "status": "available",
                "entry_count": 1,
                "sha256": "a" * 64,
            },
        },
        "runtimes": {
            "cargo": copy.deepcopy(unavailable_runtime),
            "dotnet": copy.deepcopy(unavailable_runtime),
            "java": copy.deepcopy(unavailable_runtime),
            "javac": copy.deepcopy(unavailable_runtime),
            "lua": copy.deepcopy(unavailable_runtime),
            "node": copy.deepcopy(unavailable_runtime),
            "python": {
                "status": "available",
                "path": "/usr/bin/python3",
                "version": "Python 3.10.12",
            },
            "qjs": {
                "status": "unavailable",
                "path": None,
                "version": None,
            },
            "valgrind": copy.deepcopy(unavailable_runtime),
        },
        "governor": {
            "status": "unavailable",
            "value": None,
            "reason": "cpufreq is not exposed by WSL",
        },
        "isolation": {
            "policy": "single_logical_cpu_v1",
            "status": "ISOLATED",
            "level": "affinity_only",
            "selected_cpu": 0,
            "observed_mask": "0",
            "reason": None,
        },
        "source": {
            **source_snapshot,
            "after": copy.deepcopy(source_snapshot),
            "changed_during_run": False,
            "finalized": True,
        },
        "volatile": {
            "captured_at_utc": "2026-08-30T00:00:00Z",
            "completed_at_utc": "2026-08-30T00:01:00Z",
            "load_average_start": [0.1, 0.2, 0.3],
            "load_average_end": [0.4, 0.5, 0.6],
            "process_id": 123,
            "repo_path": "/mnt/e/Git/zr_vm",
            "build_path": "/home/user/.cache/zr-vm-benchmark/build",
        },
    }


def _fingerprinted_environment() -> dict[str, object]:
    attach = getattr(contract, "environment_with_fingerprint", None)
    if attach is None:
        return _environment()
    return attach(_environment())


def _run_git(repository: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=repository,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=TEST_SUBPROCESS_TIMEOUT_SECONDS,
    )
    return result.stdout.strip()


def _initialize_git_repository(repository: Path) -> str:
    _run_git(repository, "init", "--quiet")
    _run_git(repository, "config", "user.name", "Benchmark Contract Test")
    _run_git(repository, "config", "user.email", "benchmark-contract@example.invalid")
    (repository / ".gitignore").write_text("ignored/\n", encoding="utf-8")
    (repository / "tracked.txt").write_text("base\n", encoding="utf-8")
    _run_git(repository, "add", ".gitignore", "tracked.txt")
    _run_git(repository, "commit", "--quiet", "-m", "baseline")
    return _run_git(repository, "rev-parse", "HEAD")


def _write_cmake_cache(build_directory: Path) -> None:
    compiler = shutil.which("gcc") or shutil.which("cc")
    if compiler is None:
        raise unittest.SkipTest("a GCC-compatible C compiler is required")
    build_directory.mkdir(parents=True)
    (build_directory / "CMakeCache.txt").write_text(
        "\n".join(
            (
                "CMAKE_BUILD_TYPE:STRING=Release",
                f"CMAKE_C_COMPILER:FILEPATH={compiler}",
                "CMAKE_C_FLAGS:STRING=",
                "CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG",
                "CMAKE_EXE_LINKER_FLAGS:STRING=",
                "CMAKE_EXE_LINKER_FLAGS_RELEASE:STRING=",
                "CMAKE_SHARED_LINKER_FLAGS:STRING=",
                "CMAKE_SHARED_LINKER_FLAGS_RELEASE:STRING=",
                "BUILD_SHARED_LIBS:BOOL=OFF",
                "CMAKE_INTERPROCEDURAL_OPTIMIZATION:BOOL=OFF",
                "CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE:BOOL=OFF",
                "CMAKE_MSVC_RUNTIME_LIBRARY:STRING=",
                "ZR_VM_ENABLE_ASAN:BOOL=OFF",
                "ZR_VM_TARGET_COMPILE_DEFINITIONS:STRING=",
                "ZR_VM_TARGET_COMPILE_FEATURES:STRING=",
                "ZR_VM_TARGET_COMPILE_OPTIONS:STRING=",
                "CMAKE_GENERATOR:INTERNAL=Ninja",
                "",
            )
        ),
        encoding="utf-8",
    )
    (build_directory / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(build_directory),
                    "file": str(build_directory / "benchmark_fixture.c"),
                    "command": "gcc -O3 -DNDEBUG -c benchmark_fixture.c",
                }
            ]
        ),
        encoding="utf-8",
    )


class BenchmarkEnvironmentContractBootstrapTests(unittest.TestCase):
    def test_contract_module_exists(self) -> None:
        self.assertTrue(
            CONTRACT_MODULE.is_file(),
            f"missing benchmark environment contract module: {CONTRACT_MODULE}",
        )

    def test_contract_exposes_versioned_schema_constants(self) -> None:
        self.assertEqual(2, getattr(contract, "ENVIRONMENT_SCHEMA_VERSION", None))
        self.assertEqual(1, getattr(contract, "BUILD_CONTRACT_VERSION", None))
        self.assertEqual(2, getattr(contract, "SOURCE_IDENTITY_CONTRACT_VERSION", None))
        self.assertEqual("IN_PROGRESS", getattr(contract, "CAPTURE_STATUS_IN_PROGRESS", None))
        self.assertEqual("COMPLETE", getattr(contract, "CAPTURE_STATUS_COMPLETE", None))
        self.assertEqual(
            REQUIRED_RUNTIME_NAMES,
            getattr(contract, "REQUIRED_RUNTIME_NAMES", None),
        )
        self.assertEqual(
            (
                "contract_version",
                "generator",
                "configuration",
                "flags",
                "performance_options",
                "toolchain_file",
                "target_evidence",
            ),
            getattr(contract, "REQUIRED_BUILD_IDENTITY_FIELDS", None),
        )
        self.assertEqual(
            (
                "build_shared_libs",
                "interprocedural_optimization",
                "sanitizers",
                "abi_runtime",
                "project_compile_definitions",
                "project_compile_features",
                "project_compile_options",
            ),
            getattr(contract, "REQUIRED_PERFORMANCE_OPTION_FIELDS", None),
        )


class StrictJsonTests(unittest.TestCase):
    def test_strict_loader_rejects_duplicate_keys_at_every_depth(self) -> None:
        loader = getattr(contract, "strict_json_loads", None)
        self.assertIsNotNone(loader, "strict_json_loads API is missing")

        for payload in (
            '{"value":1,"value":2}',
            '{"nested":{"value":1,"value":2}}',
        ):
            with self.subTest(payload=payload):
                with self.assertRaisesRegex(ValueError, "duplicate JSON field"):
                    loader(payload)

    def test_strict_loader_rejects_non_finite_constants(self) -> None:
        loader = getattr(contract, "strict_json_loads", None)
        self.assertIsNotNone(loader, "strict_json_loads API is missing")

        for constant in ("NaN", "Infinity", "-Infinity"):
            with self.subTest(constant=constant):
                with self.assertRaisesRegex(ValueError, "invalid JSON constant"):
                    loader('{"value":' + constant + "}")

    def test_canonical_json_is_ascii_sorted_compact_and_deterministic(self) -> None:
        serializer = getattr(contract, "canonical_json_bytes", None)
        self.assertIsNotNone(serializer, "canonical_json_bytes API is missing")
        value = {"z": [1, True], "a": "中"}

        first = serializer(value)
        second = serializer({"a": "中", "z": [1, True]})

        self.assertEqual(b'{"a":"\\u4e2d","z":[1,true]}', first)
        self.assertEqual(first, second)
        self.assertTrue(first.isascii())

    def test_canonical_json_rejects_non_finite_numbers(self) -> None:
        serializer = getattr(contract, "canonical_json_bytes", None)
        self.assertIsNotNone(serializer, "canonical_json_bytes API is missing")

        with self.assertRaises((TypeError, ValueError)):
            serializer({"value": float("nan")})


class StableEnvironmentFingerprintTests(unittest.TestCase):
    def test_fingerprint_excludes_source_volatile_paths_and_selected_cpu(self) -> None:
        fingerprint = getattr(contract, "stable_environment_fingerprint", None)
        self.assertIsNotNone(fingerprint, "stable_environment_fingerprint API is missing")
        baseline = _environment()
        changed = copy.deepcopy(baseline)
        changed["source"] = {
            "commit": "2" * 40,
            "dirty": True,
            "dirty_tree_digest": "f" * 64,
            "changed_during_run": False,
        }
        changed["volatile"] = {
            "captured_at_utc": "2030-01-01T00:00:00Z",
            "completed_at_utc": "2030-01-01T00:02:00Z",
            "load_average_start": [9.0, 8.0, 7.0],
            "load_average_end": [6.0, 5.0, 4.0],
            "process_id": 9999,
            "repo_path": "/different/repo",
            "build_path": "/different/build",
        }
        changed["compiler"]["path"] = "/opt/gcc/bin/gcc"
        changed["runtimes"]["python"]["path"] = "/opt/python/bin/python3"
        changed["isolation"]["selected_cpu"] = 14
        changed["isolation"]["observed_mask"] = "14"

        self.assertEqual(fingerprint(baseline), fingerprint(changed))

    def test_fingerprint_normalizes_identity_whitespace_but_preserves_flags(self) -> None:
        fingerprint = getattr(contract, "stable_environment_fingerprint", None)
        payload = getattr(contract, "stable_environment_payload", None)
        self.assertIsNotNone(fingerprint, "stable_environment_fingerprint API is missing")
        self.assertIsNotNone(payload, "stable_environment_payload API is missing")
        baseline = _environment()
        normalized = copy.deepcopy(baseline)
        normalized["cpu"]["model"] = "  AMD   Ryzen 7 5800H with Radeon Graphics  "
        normalized["compiler"]["version"] = "gcc (Ubuntu 11.4.0)\r\n11.4.0"
        baseline["compiler"]["version"] = "gcc (Ubuntu 11.4.0) 11.4.0"
        normalized["runtimes"]["python"]["version"] = " Python 3.10.12\r\n"

        self.assertEqual(fingerprint(baseline), fingerprint(normalized))
        self.assertEqual("AMD Ryzen 7 5800H with Radeon Graphics", payload(normalized)["cpu"]["model"])

        flags_changed = copy.deepcopy(baseline)
        flags_changed["build"]["flags"].reverse()
        self.assertNotEqual(fingerprint(baseline), fingerprint(flags_changed))

    def test_each_required_stable_dimension_changes_fingerprint(self) -> None:
        fingerprint = getattr(contract, "stable_environment_fingerprint", None)
        self.assertIsNotNone(fingerprint, "stable_environment_fingerprint API is missing")
        baseline = _environment()
        baseline_fingerprint = fingerprint(baseline)
        changes = {
            "cpu model": ("cpu", "model", "Different CPU"),
            "architecture": ("platform", "architecture", "aarch64"),
            "logical count": ("cpu", "logical_count", 8),
            "kernel": ("platform", "kernel_release", "6.19.0"),
            "WSL release": ("platform", "wsl_release", "WSL1"),
            "compiler version": ("compiler", "version", "clang version 18"),
            "compiler target": ("compiler", "target", "aarch64-linux-gnu"),
            "generator": ("build", "generator", "Unix Makefiles"),
            "configuration": ("build", "configuration", "RelWithDebInfo"),
            "runtime version": ("runtimes", "python", "version", "Python 3.12.0"),
            "governor status": ("governor", "status", "available"),
            "isolation policy": ("isolation", "policy", "single_core_v2"),
            "isolation status": ("isolation", "status", "NON_ISOLATED"),
        }
        for name, path_and_value in changes.items():
            changed = copy.deepcopy(baseline)
            *path, value = path_and_value
            target = changed
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = value
            if name == "governor status":
                changed["governor"]["value"] = "performance"
            with self.subTest(field=name):
                self.assertNotEqual(baseline_fingerprint, fingerprint(changed))

    def test_complete_build_contract_changes_fingerprint(self) -> None:
        baseline = _environment()
        baseline_fingerprint = contract.stable_environment_fingerprint(baseline)
        changes = (
            (
                "shared library mode",
                ("build", "performance_options", "build_shared_libs"),
                "ON",
            ),
            (
                "IPO/LTO",
                ("build", "performance_options", "interprocedural_optimization"),
                "OFF",
            ),
            (
                "sanitizers",
                ("build", "performance_options", "sanitizers"),
                [{"name": "ZR_VM_ENABLE_ASAN", "value": "ON"}],
            ),
            (
                "ABI/runtime library",
                ("build", "performance_options", "abi_runtime"),
                [{"name": "CMAKE_MSVC_RUNTIME_LIBRARY", "value": "MultiThreaded"}],
            ),
            (
                "compile definitions",
                ("build", "performance_options", "project_compile_definitions"),
                [{"name": "ZR_VM_TARGET_COMPILE_DEFINITIONS", "value": "ZR_FAST=0"}],
            ),
            (
                "compile features",
                ("build", "performance_options", "project_compile_features"),
                [{"name": "ZR_VM_TARGET_COMPILE_FEATURES", "value": "c_std_17"}],
            ),
            (
                "compile options",
                ("build", "performance_options", "project_compile_options"),
                [{"name": "ZR_VM_TARGET_COMPILE_OPTIONS", "value": "-fno-inline"}],
            ),
            (
                "toolchain file",
                ("build", "toolchain_file"),
                {"path": "/toolchains/release.cmake", "sha256": "b" * 64},
            ),
            (
                "target compile evidence",
                ("build", "target_evidence", "sha256"),
                "c" * 64,
            ),
        )
        for name, path, value in changes:
            changed = copy.deepcopy(baseline)
            target = changed
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = value
            with self.subTest(field=name):
                self.assertNotEqual(
                    baseline_fingerprint,
                    contract.stable_environment_fingerprint(changed),
                )


class EnvironmentLifecycleAndSchemaTests(unittest.TestCase):
    def test_in_progress_contract_is_explicitly_non_comparable(self) -> None:
        interrupted = _environment()
        interrupted["capture_status"] = "IN_PROGRESS"
        interrupted["source"]["after"] = None
        interrupted["source"]["finalized"] = False
        interrupted["volatile"]["completed_at_utc"] = None
        interrupted["volatile"]["load_average_end"] = None

        self.assertIn("CAPTURE_INCOMPLETE", contract.validate_environment_contract(interrupted))
        reasons = contract.compare_environment_contracts(
            contract.environment_with_fingerprint(_environment()),
            interrupted,
        )
        self.assertIn("CURRENT_CAPTURE_INCOMPLETE", reasons)
        with self.assertRaisesRegex(ValueError, "CAPTURE_INCOMPLETE"):
            contract.environment_with_fingerprint(interrupted)

    def test_complete_contract_requires_final_source_identity(self) -> None:
        for field, expected in (
            ("after", "MISSING_SOURCE_AFTER"),
            ("finalized", "SOURCE_NOT_FINALIZED"),
        ):
            environment = _fingerprinted_environment()
            if field == "finalized":
                environment["source"][field] = False
            else:
                del environment["source"][field]
            with self.subTest(field=field):
                self.assertIn(expected, contract.validate_environment_contract(environment))

    def test_finalize_populates_all_completion_fields_before_complete_status(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            before = contract.source_identity(repository)
            in_progress = _environment()
            in_progress["capture_status"] = "IN_PROGRESS"
            in_progress["source"] = {
                **before,
                "after": None,
                "changed_during_run": False,
                "finalized": False,
            }
            in_progress["volatile"]["completed_at_utc"] = None
            in_progress["volatile"]["load_average_end"] = None
            original = copy.deepcopy(in_progress)

            completed = contract.finalize_environment(in_progress, repository)

            self.assertEqual(original, in_progress)
            self.assertEqual("COMPLETE", completed["capture_status"])
            self.assertTrue(completed["source"]["finalized"])
            self.assertEqual(before, completed["source"]["after"])
            self.assertIsInstance(completed["volatile"]["completed_at_utc"], str)
            self.assertEqual(3, len(completed["volatile"]["load_average_end"]))
            self.assertEqual([], contract.validate_environment_contract(completed))

    def test_schema_requires_every_build_and_runtime_identity(self) -> None:
        required_build_fields = getattr(contract, "REQUIRED_BUILD_IDENTITY_FIELDS", ())
        self.assertTrue(required_build_fields, "required build field constants are missing")
        for field in required_build_fields:
            environment = _fingerprinted_environment()
            del environment["build"][field]
            expected = f"MISSING_BUILD_{field.upper()}"
            with self.subTest(build_field=field):
                self.assertIn(expected, contract.validate_environment_contract(environment))

        required_option_fields = getattr(contract, "REQUIRED_PERFORMANCE_OPTION_FIELDS", ())
        self.assertTrue(required_option_fields, "required performance option constants are missing")
        for field in required_option_fields:
            environment = _fingerprinted_environment()
            del environment["build"]["performance_options"][field]
            expected = f"MISSING_BUILD_PERFORMANCE_OPTION_{field.upper()}"
            with self.subTest(performance_option=field):
                self.assertIn(expected, contract.validate_environment_contract(environment))

        for runtime in REQUIRED_RUNTIME_NAMES:
            environment = _fingerprinted_environment()
            del environment["runtimes"][runtime]
            with self.subTest(runtime=runtime):
                self.assertIn(
                    f"MISSING_RUNTIME_{runtime.upper()}",
                    contract.validate_environment_contract(environment),
                )

    def test_complete_contract_rejects_unavailable_target_compile_evidence(self) -> None:
        environment = _fingerprinted_environment()
        environment["build"]["target_evidence"] = {
            "status": "unavailable",
            "entry_count": 0,
            "sha256": None,
        }

        issues = contract.validate_environment_contract(environment)

        self.assertIn("MISSING_BUILD_TARGET_EVIDENCE", issues)
        reasons = contract.compare_environment_contracts(
            _fingerprinted_environment(),
            environment,
        )
        self.assertIn("CURRENT_MISSING_BUILD_TARGET_EVIDENCE", reasons)

    def test_source_identity_contract_version_is_required_and_fingerprinted(self) -> None:
        environment = _fingerprinted_environment()
        del environment["source"]["contract_version"]
        self.assertIn(
            "MISSING_SOURCE_CONTRACT_VERSION",
            contract.validate_environment_contract(environment),
        )

        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            identity = contract.source_identity(repository)
            self.assertEqual(
                contract.SOURCE_IDENTITY_CONTRACT_VERSION,
                identity["contract_version"],
            )
            cache = contract.compute_cache_identity(
                repository,
                toolchain="gcc",
                compiler_version="gcc 11.4.0",
                compiler_target="x86_64-linux-gnu",
                generator="Ninja",
                configuration="Release",
                flags=[
                    {"name": "CMAKE_C_FLAGS", "value": ""},
                    {"name": "CMAKE_C_FLAGS_RELEASE", "value": "-O3 -DNDEBUG"},
                ],
            )
            altered = copy.deepcopy(cache)
            altered["toolchain_contract"]["source_identity_contract_version"] = 999
            altered_source_payload = copy.deepcopy(altered["source_digest"])
            self.assertNotEqual(
                cache["toolchain_contract"],
                altered["toolchain_contract"],
            )
            self.assertNotEqual(
                cache["source_digest"],
                hashlib.sha256(
                    contract.canonical_json_bytes(
                        {
                            "contract_version": 999,
                            "commit": cache["commit"],
                            "dirty_tree_digest": cache["dirty_tree_digest"],
                        }
                    )
                ).hexdigest(),
            )
            self.assertIsInstance(altered_source_payload, str)
            with mock.patch.object(
                source_identity_module,
                "SOURCE_IDENTITY_CONTRACT_VERSION",
                contract.SOURCE_IDENTITY_CONTRACT_VERSION + 1,
            ):
                revised = contract.compute_cache_identity(
                    repository,
                    toolchain="gcc",
                    compiler_version="gcc 11.4.0",
                    compiler_target="x86_64-linux-gnu",
                    generator="Ninja",
                    configuration="Release",
                    flags=[
                        {"name": "CMAKE_C_FLAGS", "value": ""},
                        {"name": "CMAKE_C_FLAGS_RELEASE", "value": "-O3 -DNDEBUG"},
                    ],
                )
            self.assertNotEqual(cache["source_digest"], revised["source_digest"])
            self.assertNotEqual(cache["source_key"], revised["source_key"])

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux capture integration")
    def test_capture_rejects_build_without_target_compile_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            repository = root / "repo"
            build_directory = root / "build"
            repository.mkdir()
            _initialize_git_repository(repository)
            _write_cmake_cache(build_directory)
            (build_directory / "compile_commands.json").unlink()

            with self.assertRaisesRegex(ValueError, "target compile evidence"):
                contract.capture_environment(
                    repository,
                    build_directory,
                    isolation_status="NON_ISOLATED",
                    selected_cpu=None,
                    observed_mask="0-1",
                )

    def test_cache_key_cli_without_effective_build_contract_is_noncomparable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            result = subprocess.run(
                [
                    sys.executable,
                    str(CONTRACT_MODULE),
                    "cache-key",
                    "--repo-root",
                    str(repository),
                    "--toolchain",
                    "gcc",
                    "--compiler-path",
                    sys.executable,
                    "--compiler-version",
                    "Python test compiler",
                    "--compiler-target",
                    "test-target",
                    "--generator",
                    "Ninja",
                    "--configuration",
                    "Release",
                    "--flag",
                    "CMAKE_C_FLAGS=",
                    "--flag",
                    "CMAKE_C_FLAGS_RELEASE=-O3",
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=TEST_SUBPROCESS_TIMEOUT_SECONDS,
            )

            self.assertEqual(0, result.returncode, result.stderr)
            identity = contract.strict_json_loads(result.stdout)
            self.assertFalse(identity["comparable"])
            self.assertFalse(identity["toolchain_contract"]["comparable"])


class EnvironmentComparisonTests(unittest.TestCase):
    def test_identical_isolated_contracts_are_comparable(self) -> None:
        compare = getattr(contract, "compare_environment_contracts", None)
        self.assertIsNotNone(compare, "compare_environment_contracts API is missing")
        environment = _fingerprinted_environment()

        self.assertEqual([], compare(environment, copy.deepcopy(environment)))

    def test_missing_fields_and_tampered_fingerprint_fail_closed(self) -> None:
        compare = getattr(contract, "compare_environment_contracts", None)
        self.assertIsNotNone(compare, "compare_environment_contracts API is missing")
        baseline = _fingerprinted_environment()
        current = copy.deepcopy(baseline)
        del baseline["cpu"]["model"]
        current["stable_fingerprint"]["value"] = "0" * 64

        reasons = compare(baseline, current)

        self.assertIn("BASELINE_MISSING_CPU_MODEL", reasons)
        self.assertIn("CURRENT_FINGERPRINT_MISMATCH", reasons)

    def test_non_isolated_and_source_changed_contracts_fail_closed(self) -> None:
        compare = getattr(contract, "compare_environment_contracts", None)
        attach = getattr(contract, "environment_with_fingerprint", None)
        self.assertIsNotNone(compare, "compare_environment_contracts API is missing")
        self.assertIsNotNone(attach, "environment_with_fingerprint API is missing")
        baseline = _environment()
        current = _environment()
        baseline["isolation"]["status"] = "NON_ISOLATED"
        baseline["isolation"]["reason"] = "taskset unavailable"
        current["source"]["changed_during_run"] = True

        reasons = compare(attach(baseline), attach(current))

        self.assertIn("BASELINE_NON_ISOLATED", reasons)
        self.assertIn("CURRENT_SOURCE_CHANGED_DURING_RUN", reasons)

    def test_comparison_reports_specific_stable_mismatches(self) -> None:
        compare = getattr(contract, "compare_environment_contracts", None)
        attach = getattr(contract, "environment_with_fingerprint", None)
        self.assertIsNotNone(compare, "compare_environment_contracts API is missing")
        self.assertIsNotNone(attach, "environment_with_fingerprint API is missing")
        baseline = _environment()
        current = copy.deepcopy(baseline)
        current["cpu"]["model"] = "Different CPU"
        current["build"]["flags"][1]["value"] = "-O2 -DNDEBUG"

        reasons = compare(attach(baseline), attach(current))

        self.assertIn("CPU_MODEL_MISMATCH", reasons)
        self.assertIn("BUILD_FLAGS_MISMATCH", reasons)
        self.assertIn("ENVIRONMENT_FINGERPRINT_MISMATCH", reasons)

    def test_comparison_reports_effective_build_contract_mismatches(self) -> None:
        baseline = _environment()
        current = copy.deepcopy(baseline)
        current["build"]["performance_options"]["interprocedural_optimization"] = "OFF"
        current["build"]["toolchain_file"] = {
            "path": "/toolchains/alternate.cmake",
            "sha256": "b" * 64,
        }
        current["build"]["target_evidence"]["sha256"] = "c" * 64

        reasons = contract.compare_environment_contracts(
            contract.environment_with_fingerprint(baseline),
            contract.environment_with_fingerprint(current),
        )

        self.assertIn("BUILD_PERFORMANCE_OPTIONS_MISMATCH", reasons)
        self.assertIn("BUILD_TOOLCHAIN_FILE_MISMATCH", reasons)
        self.assertIn("BUILD_TARGET_EVIDENCE_MISMATCH", reasons)
        self.assertIn("ENVIRONMENT_FINGERPRINT_MISMATCH", reasons)


class BuildContractCaptureTests(unittest.TestCase):
    def test_build_contract_captures_effective_options_toolchain_and_target_evidence(self) -> None:
        capture_build = getattr(contract, "build_contract_from_cache", None)
        self.assertIsNotNone(capture_build, "build_contract_from_cache API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            repository = root / "repo"
            build_directory = root / "build"
            repository.mkdir()
            build_directory.mkdir()
            toolchain_file = root / "toolchains" / "linux.cmake"
            toolchain_file.parent.mkdir()
            toolchain_file.write_text("set(CMAKE_SYSTEM_NAME Linux)\n", encoding="utf-8")
            source_file = repository / "source.c"
            source_file.write_text("int main(void) { return 0; }\n", encoding="utf-8")
            compile_commands = [
                {
                    "directory": str(build_directory),
                    "file": str(source_file),
                    "command": (
                        f"cc -DZR_FAST=1 -O3 -c {source_file} "
                        f"-o {build_directory / 'source.c.o'}"
                    ),
                }
            ]
            (build_directory / "compile_commands.json").write_text(
                json.dumps(compile_commands),
                encoding="utf-8",
            )
            cache = {
                "CMAKE_GENERATOR": "Ninja",
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_C_FLAGS": "",
                "CMAKE_C_FLAGS_RELEASE": "-O3 -DNDEBUG",
                "CMAKE_EXE_LINKER_FLAGS": "",
                "CMAKE_EXE_LINKER_FLAGS_RELEASE": "",
                "CMAKE_SHARED_LINKER_FLAGS": "",
                "CMAKE_SHARED_LINKER_FLAGS_RELEASE": "",
                "BUILD_SHARED_LIBS": "ON",
                "CMAKE_INTERPROCEDURAL_OPTIMIZATION": "OFF",
                "CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE": "ON",
                "CMAKE_TOOLCHAIN_FILE": str(toolchain_file),
                "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded",
                "ZR_VM_ENABLE_ASAN": "OFF",
                "ZR_VM_ENABLE_UBSAN": "ON",
                "ZR_VM_TARGET_COMPILE_DEFINITIONS": "ZR_FAST=1",
                "ZR_VM_TARGET_COMPILE_FEATURES": "c_std_11",
                "ZR_VM_TARGET_COMPILE_OPTIONS": "-fno-omit-frame-pointer",
            }

            captured = capture_build(
                cache,
                "Release",
                repository=repository,
                build_directory=build_directory,
            )

            self.assertEqual("ON", captured["performance_options"]["build_shared_libs"])
            self.assertEqual(
                "ON",
                captured["performance_options"]["interprocedural_optimization"],
            )
            self.assertEqual(
                [
                    {"name": "ZR_VM_ENABLE_ASAN", "value": "OFF"},
                    {"name": "ZR_VM_ENABLE_UBSAN", "value": "ON"},
                ],
                captured["performance_options"]["sanitizers"],
            )
            self.assertEqual(
                [{"name": "CMAKE_MSVC_RUNTIME_LIBRARY", "value": "MultiThreaded"}],
                captured["performance_options"]["abi_runtime"],
            )
            self.assertEqual(str(toolchain_file.resolve()), captured["toolchain_file"]["path"])
            self.assertEqual(
                hashlib.sha256(toolchain_file.read_bytes()).hexdigest(),
                captured["toolchain_file"]["sha256"],
            )
            self.assertEqual("available", captured["target_evidence"]["status"])
            self.assertEqual(1, captured["target_evidence"]["entry_count"])
            self.assertRegex(captured["target_evidence"]["sha256"], r"^[0-9a-f]{64}$")

            first_evidence = captured["target_evidence"]["sha256"]
            compile_commands[0]["command"] = compile_commands[0]["command"].replace("-O3", "-O2")
            (build_directory / "compile_commands.json").write_text(
                json.dumps(compile_commands),
                encoding="utf-8",
            )
            changed = capture_build(
                cache,
                "Release",
                repository=repository,
                build_directory=build_directory,
            )
            self.assertNotEqual(first_evidence, changed["target_evidence"]["sha256"])

    def test_build_contract_records_unavailable_optional_evidence_explicitly(self) -> None:
        capture_build = getattr(contract, "build_contract_from_cache", None)
        self.assertIsNotNone(capture_build, "build_contract_from_cache API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            build_directory = root / "build"
            repository = root / "repo"
            build_directory.mkdir()
            repository.mkdir()
            cache = {
                "CMAKE_GENERATOR": "Ninja",
                "CMAKE_BUILD_TYPE": "Release",
            }

            captured = capture_build(
                cache,
                None,
                repository=repository,
                build_directory=build_directory,
            )

            self.assertEqual({"path": None, "sha256": None}, captured["toolchain_file"])
            self.assertEqual(
                {"status": "unavailable", "entry_count": 0, "sha256": None},
                captured["target_evidence"],
            )


class DirtyTreeAndCacheIdentityTests(unittest.TestCase):
    def test_dirty_digest_covers_modification_deletion_mode_and_untracked_content(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            self.assertIsNone(digest(repository))

            (repository / "tracked.txt").write_text("modified\n", encoding="utf-8")
            modified_digest = digest(repository)
            self.assertRegex(modified_digest, r"^[0-9a-f]{64}$")

            (repository / "tracked.txt").unlink()
            deleted_digest = digest(repository)
            self.assertNotEqual(modified_digest, deleted_digest)

            _run_git(repository, "restore", "tracked.txt")
            _run_git(repository, "update-index", "--chmod=+x", "tracked.txt")
            mode_digest = digest(repository)
            self.assertNotEqual(deleted_digest, mode_digest)

            _run_git(repository, "reset", "--quiet", "HEAD", "tracked.txt")
            untracked = repository / "untracked name-中.txt"
            untracked.write_text("first\n", encoding="utf-8")
            untracked_first = digest(repository)
            untracked.write_text("second\n", encoding="utf-8")
            untracked_second = digest(repository)
            self.assertNotEqual(untracked_first, untracked_second)

    def test_dirty_digest_excludes_ignored_files(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            ignored = repository / "ignored" / "artifact.bin"
            ignored.parent.mkdir()
            ignored.write_bytes(b"ignored build output")

            self.assertIsNone(digest(repository))

    def test_dirty_digest_is_independent_of_user_diff_rendering_configuration(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            (repository / "tracked.txt").write_text(
                "base\nline two\nline three\nline four\n",
                encoding="utf-8",
            )
            _run_git(repository, "add", "tracked.txt")
            _run_git(repository, "commit", "--quiet", "-m", "expand fixture")
            (repository / "tracked.txt").write_text(
                "base\nchanged two\nline three\nchanged four\n",
                encoding="utf-8",
            )
            (repository / "order-forward.txt").write_text(
                "tracked.txt\n.gitignore\n",
                encoding="utf-8",
            )
            (repository / "order-reverse.txt").write_text(
                ".gitignore\ntracked.txt\n",
                encoding="utf-8",
            )

            _run_git(repository, "config", "color.diff", "always")
            _run_git(repository, "config", "diff.algorithm", "histogram")
            _run_git(repository, "config", "diff.orderFile", "order-forward.txt")
            _run_git(repository, "config", "diff.submodule", "log")
            configured = digest(repository)
            _run_git(repository, "config", "color.diff", "never")
            _run_git(repository, "config", "diff.algorithm", "minimal")
            _run_git(repository, "config", "diff.orderFile", "order-reverse.txt")
            _run_git(repository, "config", "diff.submodule", "short")
            differently_configured = digest(repository)

            self.assertEqual(configured, differently_configured)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Git submodule rendering integration")
    def test_dirty_digest_is_independent_of_diff_order_and_submodule_rendering(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            child_source = root / "child-source"
            repository = root / "parent"
            child_source.mkdir()
            repository.mkdir()
            _initialize_git_repository(child_source)
            _initialize_git_repository(repository)
            _run_git(
                repository,
                "-c",
                "protocol.file.allow=always",
                "submodule",
                "add",
                "--quiet",
                str(child_source),
                "deps/child",
            )
            (repository / "a.txt").write_text("base a\n", encoding="utf-8")
            (repository / "b.txt").write_text("base b\n", encoding="utf-8")
            _run_git(repository, "add", "a.txt", "b.txt")
            _run_git(repository, "commit", "--quiet", "-am", "add submodule and files")
            (repository / "a.txt").write_text("changed a\n", encoding="utf-8")
            (repository / "b.txt").write_text("changed b\n", encoding="utf-8")
            (repository / "order-forward.txt").write_text("a.txt\nb.txt\n", encoding="utf-8")
            (repository / "order-reverse.txt").write_text("b.txt\na.txt\n", encoding="utf-8")
            child = repository / "deps" / "child"
            (child / "tracked.txt").write_text("submodule changed\n", encoding="utf-8")

            _run_git(repository, "config", "diff.orderFile", "order-forward.txt")
            _run_git(repository, "config", "diff.submodule", "log")
            first = digest(repository)
            _run_git(repository, "config", "diff.orderFile", "order-reverse.txt")
            _run_git(repository, "config", "diff.submodule", "short")
            second = digest(repository)

            self.assertEqual(first, second)

    @unittest.skipUnless(hasattr(os, "symlink"), "symlinks are not supported")
    def test_dirty_digest_covers_symlink_target_when_platform_allows_it(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            link = repository / "link"
            try:
                link.symlink_to("tracked.txt")
            except OSError as exc:
                self.skipTest(f"symlink creation is unavailable: {exc}")
            _run_git(repository, "add", "link")
            _run_git(repository, "commit", "--quiet", "-m", "add symlink")
            self.assertIsNone(digest(repository))

            link.unlink()
            link.symlink_to("different-target")
            self.assertRegex(digest(repository), r"^[0-9a-f]{64}$")

    def test_dirty_digest_streams_large_sparse_untracked_content(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            large_file = repository / "large-sparse.bin"
            with large_file.open("wb") as handle:
                handle.seek(16 * 1024 * 1024)
                handle.write(b"first")

            with mock.patch.object(
                Path,
                "read_bytes",
                side_effect=AssertionError("whole-file reads are forbidden"),
            ):
                first = digest(repository)

            with large_file.open("r+b") as handle:
                handle.seek(8 * 1024 * 1024)
                handle.write(b"changed")
            with mock.patch.object(
                Path,
                "read_bytes",
                side_effect=AssertionError("whole-file reads are forbidden"),
            ):
                second = digest(repository)

            self.assertRegex(first, r"^[0-9a-f]{64}$")
            self.assertNotEqual(first, second)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Git submodule content integration")
    def test_dirty_digest_recurses_into_initialized_submodule_content(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            child_source = root / "child-source"
            repository = root / "parent"
            child_source.mkdir()
            repository.mkdir()
            child_commit = _initialize_git_repository(child_source)
            _initialize_git_repository(repository)
            _run_git(
                repository,
                "-c",
                "protocol.file.allow=always",
                "submodule",
                "add",
                "--quiet",
                str(child_source),
                "deps/child",
            )
            _run_git(repository, "commit", "--quiet", "-am", "add submodule")
            initialized_child = repository / "deps" / "child"
            self.assertEqual(child_commit, _run_git(initialized_child, "rev-parse", "HEAD"))
            self.assertIsNone(digest(repository))

            tracked = initialized_child / "tracked.txt"
            tracked.write_text("submodule edit one\n", encoding="utf-8")
            first = digest(repository)
            tracked.write_text("submodule edit two\n", encoding="utf-8")
            second = digest(repository)

            self.assertEqual(child_commit, _run_git(initialized_child, "rev-parse", "HEAD"))
            self.assertRegex(first, r"^[0-9a-f]{64}$")
            self.assertNotEqual(first, second)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Git submodule initialization integration")
    def test_dirty_digest_rejects_missing_submodule_checkout(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            child_source = root / "child-source"
            repository = root / "parent"
            child_source.mkdir()
            repository.mkdir()
            _initialize_git_repository(child_source)
            _initialize_git_repository(repository)
            _run_git(
                repository,
                "-c",
                "protocol.file.allow=always",
                "submodule",
                "add",
                "--quiet",
                str(child_source),
                "deps/child",
            )
            _run_git(repository, "commit", "--quiet", "-am", "add submodule")
            submodule = repository / "deps" / "child"
            git_metadata = submodule / ".git"
            if git_metadata.is_dir():
                shutil.rmtree(git_metadata)
            else:
                git_metadata.unlink()

            with self.assertRaisesRegex(ValueError, "submodule is unavailable"):
                digest(repository)

    def test_git_subprocess_timeout_is_enforced(self) -> None:
        digest = getattr(contract, "compute_dirty_tree_digest", None)
        self.assertIsNotNone(digest, "compute_dirty_tree_digest API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            repository = root / "repo"
            repository.mkdir()
            fake_git = root / "slow_git.py"
            fake_git.write_text("import time\ntime.sleep(5)\n", encoding="utf-8")

            started = time.monotonic()
            with self.assertRaisesRegex(ValueError, "timed out"):
                digest(
                    repository,
                    git_command=(sys.executable, str(fake_git)),
                    timeout_seconds=0.05,
                )
            self.assertLess(time.monotonic() - started, 2.0)

    def test_cache_identity_uses_source_and_ordered_toolchain_contract(self) -> None:
        cache_identity = getattr(contract, "compute_cache_identity", None)
        self.assertIsNotNone(cache_identity, "compute_cache_identity API is missing")
        flags = [
            {"name": "CMAKE_C_FLAGS", "value": ""},
            {"name": "CMAKE_C_FLAGS_RELEASE", "value": "-O3 -DNDEBUG"},
        ]
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            commit = _initialize_git_repository(repository)
            clean = cache_identity(
                repository,
                toolchain="gcc",
                compiler_version="gcc 11.4.0",
                compiler_target="x86_64-linux-gnu",
                generator="Ninja",
                configuration="Release",
                flags=flags,
            )
            self.assertEqual(commit, clean["commit"])
            self.assertFalse(clean["dirty"])
            self.assertRegex(clean["source_digest"], r"^[0-9a-f]{64}$")
            self.assertTrue(clean["source_key"].startswith(f"{commit}-clean-"))
            self.assertTrue(clean["source_key"].endswith(clean["source_digest"]))
            self.assertEqual(
                f"{clean['source_key']}/{clean['toolchain_key']}",
                clean["relative_path"],
            )

            (repository / "tracked.txt").write_text("dirty one\n", encoding="utf-8")
            dirty_one = cache_identity(
                repository,
                toolchain="gcc",
                compiler_version="gcc 11.4.0",
                compiler_target="x86_64-linux-gnu",
                generator="Ninja",
                configuration="Release",
                flags=flags,
            )
            (repository / "tracked.txt").write_text("dirty two\n", encoding="utf-8")
            dirty_two = cache_identity(
                repository,
                toolchain="gcc",
                compiler_version="gcc 11.4.0",
                compiler_target="x86_64-linux-gnu",
                generator="Ninja",
                configuration="Release",
                flags=flags,
            )
            self.assertTrue(dirty_one["dirty"])
            self.assertIn(
                f"-dirty-{dirty_one['dirty_tree_digest']}",
                dirty_one["source_key"],
            )
            self.assertNotEqual(dirty_one["source_key"], dirty_two["source_key"])
            self.assertRegex(dirty_one["toolchain_digest"], r"^[0-9a-f]{64}$")
            self.assertTrue(dirty_one["toolchain_key"].endswith(dirty_one["toolchain_digest"]))

            reordered = cache_identity(
                repository,
                toolchain="gcc",
                compiler_version="gcc 11.4.0",
                compiler_target="x86_64-linux-gnu",
                generator="Ninja",
                configuration="Release",
                flags=list(reversed(flags)),
            )
            self.assertNotEqual(dirty_two["toolchain_key"], reordered["toolchain_key"])

    def test_cache_identity_includes_compiler_toolchain_and_effective_build_contract(self) -> None:
        cache_identity = getattr(contract, "compute_cache_identity", None)
        self.assertIsNotNone(cache_identity, "compute_cache_identity API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            _initialize_git_repository(repository)
            build_contract = copy.deepcopy(_environment()["build"])
            compiler_path = str(Path(sys.executable).resolve())

            baseline = cache_identity(
                repository,
                toolchain="python-test-toolchain",
                compiler_path=compiler_path,
                compiler_version="compiler 1.0",
                compiler_target="test-target",
                generator="Ninja",
                configuration="Release",
                flags=build_contract["flags"],
                build_contract=build_contract,
            )

            self.assertEqual(
                compiler_path,
                baseline["toolchain_contract"]["compiler_path"],
            )
            self.assertEqual(
                build_contract["toolchain_file"],
                baseline["toolchain_contract"]["build_contract"]["toolchain_file"],
            )
            self.assertRegex(baseline["toolchain_digest"], r"^[0-9a-f]{64}$")
            self.assertEqual(
                hashlib.sha256(
                    contract.canonical_json_bytes(baseline["toolchain_contract"])
                ).hexdigest(),
                baseline["toolchain_digest"],
            )
            self.assertIn(baseline["toolchain_digest"], baseline["relative_path"])

            changed_contract = copy.deepcopy(build_contract)
            changed_contract["performance_options"]["build_shared_libs"] = "ON"
            changed = cache_identity(
                repository,
                toolchain="python-test-toolchain",
                compiler_path=compiler_path,
                compiler_version="compiler 1.0",
                compiler_target="test-target",
                generator="Ninja",
                configuration="Release",
                flags=changed_contract["flags"],
                build_contract=changed_contract,
            )
            self.assertNotEqual(baseline["toolchain_digest"], changed["toolchain_digest"])


class CpuTopologyTests(unittest.TestCase):
    def test_cpu_list_parser_accepts_ranges_and_rejects_malformed_input(self) -> None:
        parser = getattr(contract, "parse_cpu_list", None)
        self.assertIsNotNone(parser, "parse_cpu_list API is missing")

        self.assertEqual([0, 1, 2, 4, 7, 8], parser("0-2,4,7-8"))
        for invalid in ("", "1-", "3-1", "1,,2", "cpu0", "-1"):
            with self.subTest(value=invalid):
                with self.assertRaises(ValueError):
                    parser(invalid)

    def test_topology_selection_uses_only_allowed_online_logical_cpus(self) -> None:
        selector = getattr(contract, "select_allowed_cpu", None)
        self.assertIsNotNone(selector, "select_allowed_cpu API is missing")
        with tempfile.TemporaryDirectory() as temporary_directory:
            sysfs = Path(temporary_directory)
            for cpu, package, core, siblings in (
                (0, 0, 0, "0-1"),
                (1, 0, 0, "0-1"),
                (2, 0, 1, "2-3"),
                (3, 0, 1, "2-3"),
            ):
                topology = sysfs / f"cpu{cpu}" / "topology"
                topology.mkdir(parents=True)
                (topology / "physical_package_id").write_text(str(package), encoding="ascii")
                (topology / "core_id").write_text(str(core), encoding="ascii")
                (topology / "thread_siblings_list").write_text(siblings, encoding="ascii")
                (sysfs / f"cpu{cpu}" / "online").write_text("1", encoding="ascii")

            selected = selector("1-3", sysfs_root=sysfs)
            overridden = selector("1-3", sysfs_root=sysfs, requested_cpu=2)

            self.assertEqual(
                {
                    "selected_cpu": 1,
                    "physical_package_id": 0,
                    "core_id": 0,
                    "thread_siblings_list": "0-1",
                },
                selected,
            )
            self.assertEqual(2, overridden["selected_cpu"])
            with self.assertRaisesRegex(ValueError, "not in the allowed CPU set"):
                selector("1-3", sysfs_root=sysfs, requested_cpu=0)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux capture integration")
class LinuxCaptureWrapperTests(unittest.TestCase):
    def test_wrapper_pins_child_and_writes_a_valid_environment_contract(self) -> None:
        self.assertTrue(CAPTURE_WRAPPER.is_file(), f"missing capture wrapper: {CAPTURE_WRAPPER}")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            repository = root / "repo"
            build_directory = root / "build"
            repository.mkdir()
            _initialize_git_repository(repository)
            _write_cmake_cache(build_directory)
            output = root / "environment.json"
            child_mask = root / "child-mask.txt"
            command = (
                "grep '^Cpus_allowed_list:' /proc/self/status | "
                f"sed 's/^[^:]*:[[:space:]]*//' > {shlex.quote(str(child_mask))}"
            )

            result = subprocess.run(
                [
                    "bash",
                    str(CAPTURE_WRAPPER),
                    "--repo-root",
                    str(repository),
                    "--build-dir",
                    str(build_directory),
                    "--output",
                    str(output),
                    "--",
                    "sh",
                    "-c",
                    command,
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=TEST_SUBPROCESS_TIMEOUT_SECONDS,
            )

            self.assertEqual(0, result.returncode, result.stderr)
            environment = contract.strict_json_loads(output.read_bytes())
            self.assertEqual([], contract.validate_environment_contract(environment))
            self.assertEqual("ISOLATED", environment["isolation"]["status"])
            self.assertEqual("affinity_only", environment["isolation"]["level"])
            self.assertEqual(
                str(environment["isolation"]["selected_cpu"]),
                child_mask.read_text(encoding="ascii").strip(),
            )
            self.assertFalse(environment["source"]["changed_during_run"])
            self.assertTrue(environment["source"]["finalized"])
            self.assertIsInstance(environment["source"]["after"], dict)
            self.assertIsInstance(environment["volatile"]["completed_at_utc"], str)
            self.assertEqual(3, len(environment["volatile"]["load_average_end"]))

    def test_wrapper_marks_nonisolated_detects_source_change_and_preserves_exit_code(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            repository = root / "repo"
            build_directory = root / "build"
            repository.mkdir()
            _initialize_git_repository(repository)
            _write_cmake_cache(build_directory)
            output = root / "environment.json"
            command = (
                f"printf changed >> {shlex.quote(str(repository / 'tracked.txt'))}; exit 7"
            )
            environment_variables = os.environ.copy()
            environment_variables["ZR_VM_BENCHMARK_DISABLE_AFFINITY"] = "1"

            result = subprocess.run(
                [
                    "bash",
                    str(CAPTURE_WRAPPER),
                    "--repo-root",
                    str(repository),
                    "--build-dir",
                    str(build_directory),
                    "--output",
                    str(output),
                    "--",
                    "sh",
                    "-c",
                    command,
                ],
                env=environment_variables,
                check=False,
                capture_output=True,
                text=True,
                timeout=TEST_SUBPROCESS_TIMEOUT_SECONDS,
            )

            self.assertEqual(7, result.returncode, result.stderr)
            environment = contract.strict_json_loads(output.read_bytes())
            self.assertEqual("NON_ISOLATED", environment["isolation"]["status"])
            self.assertIn("disabled", environment["isolation"]["reason"])
            self.assertTrue(environment["source"]["changed_during_run"])
            self.assertIn(
                "SOURCE_CHANGED_DURING_RUN",
                contract.validate_environment_contract(environment),
            )

    def test_interrupted_wrapper_leaves_in_progress_contract_that_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            repository = root / "repo"
            build_directory = root / "build"
            repository.mkdir()
            _initialize_git_repository(repository)
            _write_cmake_cache(build_directory)
            output = root / "environment.json"
            environment_variables = os.environ.copy()
            environment_variables["ZR_VM_BENCHMARK_DISABLE_AFFINITY"] = "1"

            result = subprocess.run(
                [
                    "bash",
                    str(CAPTURE_WRAPPER),
                    "--repo-root",
                    str(repository),
                    "--build-dir",
                    str(build_directory),
                    "--output",
                    str(output),
                    "--",
                    "sh",
                    "-c",
                    'kill -KILL "$PPID"; sleep 1',
                ],
                env=environment_variables,
                check=False,
                capture_output=True,
                text=True,
                timeout=TEST_SUBPROCESS_TIMEOUT_SECONDS,
            )

            self.assertNotEqual(0, result.returncode)
            environment = contract.strict_json_loads(output.read_bytes())
            self.assertEqual("IN_PROGRESS", environment["capture_status"])
            self.assertFalse(environment["source"]["finalized"])
            self.assertIsNone(environment["source"]["after"])
            self.assertIsNone(environment["volatile"]["completed_at_utc"])
            self.assertIsNone(environment["volatile"]["load_average_end"])
            self.assertNotIn("stable_fingerprint", environment)
            self.assertIn(
                "CAPTURE_INCOMPLETE",
                contract.validate_environment_contract(environment),
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
