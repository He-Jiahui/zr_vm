#!/usr/bin/env python3
"""Versioned schema and comparison rules for benchmark environments."""

from __future__ import annotations

import copy
import hashlib
import json
import math
import os
import re
import tempfile
from pathlib import Path
from typing import Any


ENVIRONMENT_SCHEMA_VERSION = 2
BUILD_CONTRACT_VERSION = 1
SOURCE_IDENTITY_CONTRACT_VERSION = 2
FINGERPRINT_ALGORITHM = "sha256"
FINGERPRINT_CONTRACT_VERSION = 2
CACHE_CONTRACT_VERSION = 2
CAPTURE_STATUS_IN_PROGRESS = "IN_PROGRESS"
CAPTURE_STATUS_COMPLETE = "COMPLETE"
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
REQUIRED_BUILD_IDENTITY_FIELDS = (
    "contract_version",
    "generator",
    "configuration",
    "flags",
    "performance_options",
    "toolchain_file",
    "target_evidence",
)
REQUIRED_PERFORMANCE_OPTION_FIELDS = (
    "build_shared_libs",
    "interprocedural_optimization",
    "sanitizers",
    "abi_runtime",
    "project_compile_definitions",
    "project_compile_features",
    "project_compile_options",
)

_HEX_DIGEST_PATTERN = re.compile(r"^[0-9a-f]{64}$")
_COMMIT_PATTERN = re.compile(r"^[0-9a-fA-F]{40,64}$")


class EnvironmentContractError(ValueError):
    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"invalid JSON constant {value!r}")


def _object_without_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON field {key!r}")
        result[key] = value
    return result


def strict_json_loads(payload: str | bytes) -> Any:
    return json.loads(
        payload,
        parse_constant=_reject_json_constant,
        object_pairs_hook=_object_without_duplicate_keys,
    )


def canonical_json_bytes(value: Any) -> bytes:
    rendered = json.dumps(
        value,
        ensure_ascii=True,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    )
    return rendered.encode("ascii")


def atomic_write_json(path: str | os.PathLike[str], value: Any) -> None:
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = canonical_json_bytes(value)
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            dir=output.parent,
            prefix=f".{output.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, output)
        temporary_name = None
    finally:
        if temporary_name is not None:
            try:
                Path(temporary_name).unlink()
            except FileNotFoundError:
                pass


def _mapping(value: Any, code: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise EnvironmentContractError(code)
    return value


def _identity(value: Any, code: str, *, nullable: bool = False) -> str | None:
    if value is None and nullable:
        return None
    if not isinstance(value, str) or not value.strip():
        raise EnvironmentContractError(code)
    return " ".join(value.split())


def _path_identity(value: Any, code: str, *, nullable: bool = False) -> str | None:
    if value is None and nullable:
        return None
    if not isinstance(value, str) or not value.strip():
        raise EnvironmentContractError(code)
    return value.strip().replace("\\", "/")


def _integer(value: Any, code: str, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise EnvironmentContractError(code)
    return value


def _boolean(value: Any, code: str) -> bool:
    if type(value) is not bool:
        raise EnvironmentContractError(code)
    return value


def _ordered_name_values(
    value: Any,
    code: str,
    *,
    require_nonempty: bool = False,
) -> list[dict[str, str]]:
    if not isinstance(value, list) or (require_nonempty and not value):
        raise EnvironmentContractError(code)
    result: list[dict[str, str]] = []
    names: set[str] = set()
    for item in value:
        record = _mapping(item, code)
        if set(record) != {"name", "value"}:
            raise EnvironmentContractError(code)
        name = _identity(record.get("name"), code)
        raw_value = record.get("value")
        if not isinstance(raw_value, str) or name in names:
            raise EnvironmentContractError(code)
        names.add(name)
        result.append(
            {
                "name": name,
                "value": raw_value.replace("\r\n", "\n").replace("\r", "\n"),
            }
        )
    return result


def normalize_build_flags(value: Any) -> list[dict[str, str]]:
    return _ordered_name_values(value, "INVALID_BUILD_FLAGS", require_nonempty=True)


def _runtime_versions(value: Any) -> dict[str, dict[str, str | None]]:
    runtimes = _mapping(value, "INVALID_RUNTIMES")
    for required in REQUIRED_RUNTIME_NAMES:
        if required not in runtimes:
            raise EnvironmentContractError(f"MISSING_RUNTIME_{required.upper()}")
    result: dict[str, dict[str, str | None]] = {}
    for raw_name, raw_record in runtimes.items():
        name = _identity(raw_name, "INVALID_RUNTIME_NAME")
        if name in result:
            raise EnvironmentContractError("INVALID_RUNTIME_NAME")
        record = _mapping(raw_record, "INVALID_RUNTIME_RECORD")
        status = record.get("status")
        if status not in {"available", "unavailable"}:
            raise EnvironmentContractError("INVALID_RUNTIME_STATUS")
        version = record.get("version")
        if status == "available":
            version = _identity(version, "INVALID_RUNTIME_VERSION")
        elif version is not None:
            raise EnvironmentContractError("INVALID_RUNTIME_VERSION")
        result[name] = {"status": status, "version": version}
    return result


def _normalize_performance_options(value: Any) -> dict[str, Any]:
    options = _mapping(value, "INVALID_BUILD_PERFORMANCE_OPTIONS")
    for field in REQUIRED_PERFORMANCE_OPTION_FIELDS:
        if field not in options:
            raise EnvironmentContractError(
                f"MISSING_BUILD_PERFORMANCE_OPTION_{field.upper()}"
            )
    return {
        "build_shared_libs": _identity(
            options["build_shared_libs"],
            "INVALID_BUILD_SHARED_LIBS",
        ),
        "interprocedural_optimization": _identity(
            options["interprocedural_optimization"],
            "INVALID_BUILD_INTERPROCEDURAL_OPTIMIZATION",
        ),
        "sanitizers": _ordered_name_values(
            options["sanitizers"], "INVALID_BUILD_SANITIZERS"
        ),
        "abi_runtime": _ordered_name_values(
            options["abi_runtime"], "INVALID_BUILD_ABI_RUNTIME"
        ),
        "project_compile_definitions": _ordered_name_values(
            options["project_compile_definitions"],
            "INVALID_BUILD_COMPILE_DEFINITIONS",
        ),
        "project_compile_features": _ordered_name_values(
            options["project_compile_features"],
            "INVALID_BUILD_COMPILE_FEATURES",
        ),
        "project_compile_options": _ordered_name_values(
            options["project_compile_options"],
            "INVALID_BUILD_COMPILE_OPTIONS",
        ),
    }


def _normalize_toolchain_file(value: Any) -> dict[str, str | None]:
    record = _mapping(value, "INVALID_BUILD_TOOLCHAIN_FILE")
    if set(record) != {"path", "sha256"}:
        raise EnvironmentContractError("INVALID_BUILD_TOOLCHAIN_FILE")
    path = record.get("path")
    digest = record.get("sha256")
    if path is None and digest is None:
        return {"path": None, "sha256": None}
    normalized_path = _path_identity(path, "INVALID_BUILD_TOOLCHAIN_FILE_PATH")
    if not isinstance(digest, str) or _HEX_DIGEST_PATTERN.fullmatch(digest) is None:
        raise EnvironmentContractError("INVALID_BUILD_TOOLCHAIN_FILE_DIGEST")
    return {"path": normalized_path, "sha256": digest}


def _normalize_target_evidence(
    value: Any,
    *,
    allow_unavailable: bool = False,
) -> dict[str, Any]:
    record = _mapping(value, "INVALID_BUILD_TARGET_EVIDENCE")
    if set(record) != {"status", "entry_count", "sha256"}:
        raise EnvironmentContractError("INVALID_BUILD_TARGET_EVIDENCE")
    status = record.get("status")
    if status not in {"available", "unavailable"}:
        raise EnvironmentContractError("INVALID_BUILD_TARGET_EVIDENCE_STATUS")
    count = _integer(record.get("entry_count"), "INVALID_BUILD_TARGET_ENTRY_COUNT")
    digest = record.get("sha256")
    if status == "unavailable":
        if count != 0 or digest is not None:
            raise EnvironmentContractError("INVALID_BUILD_TARGET_EVIDENCE")
        if not allow_unavailable:
            raise EnvironmentContractError("MISSING_BUILD_TARGET_EVIDENCE")
    elif count < 1 or not isinstance(digest, str) or _HEX_DIGEST_PATTERN.fullmatch(digest) is None:
        raise EnvironmentContractError("INVALID_BUILD_TARGET_EVIDENCE")
    return {"status": status, "entry_count": count, "sha256": digest}


def normalize_build_contract(
    value: Any,
    *,
    allow_unavailable_target_evidence: bool = False,
) -> dict[str, Any]:
    build = _mapping(value, "MISSING_BUILD")
    for field in REQUIRED_BUILD_IDENTITY_FIELDS:
        if field not in build:
            raise EnvironmentContractError(f"MISSING_BUILD_{field.upper()}")
    if build["contract_version"] != BUILD_CONTRACT_VERSION:
        raise EnvironmentContractError("INVALID_BUILD_CONTRACT_VERSION")
    return {
        "contract_version": BUILD_CONTRACT_VERSION,
        "generator": _identity(build["generator"], "MISSING_BUILD_GENERATOR"),
        "configuration": _identity(
            build["configuration"], "MISSING_BUILD_CONFIGURATION"
        ),
        "flags": normalize_build_flags(build["flags"]),
        "performance_options": _normalize_performance_options(
            build["performance_options"]
        ),
        "toolchain_file": _normalize_toolchain_file(build["toolchain_file"]),
        "target_evidence": _normalize_target_evidence(
            build["target_evidence"],
            allow_unavailable=allow_unavailable_target_evidence,
        ),
    }


def stable_environment_payload(environment: Any) -> dict[str, Any]:
    root = _mapping(environment, "ENVIRONMENT_NOT_OBJECT")
    if root.get("schema_version") != ENVIRONMENT_SCHEMA_VERSION:
        raise EnvironmentContractError("INVALID_SCHEMA_VERSION")
    if root.get("capture_status") != CAPTURE_STATUS_COMPLETE:
        raise EnvironmentContractError("CAPTURE_INCOMPLETE")

    platform = _mapping(root.get("platform"), "MISSING_PLATFORM")
    cpu = _mapping(root.get("cpu"), "MISSING_CPU")
    compiler = _mapping(root.get("compiler"), "MISSING_COMPILER")
    governor = _mapping(root.get("governor"), "MISSING_GOVERNOR")
    isolation = _mapping(root.get("isolation"), "MISSING_ISOLATION")

    governor_status = governor.get("status")
    if governor_status not in {"available", "unavailable"}:
        raise EnvironmentContractError("INVALID_GOVERNOR_STATUS")
    governor_value = governor.get("value")
    if governor_status == "available":
        governor_value = _identity(governor_value, "INVALID_GOVERNOR_VALUE")
    elif governor_value is not None:
        raise EnvironmentContractError("INVALID_GOVERNOR_VALUE")

    isolation_status = isolation.get("status")
    if isolation_status not in {"ISOLATED", "NON_ISOLATED"}:
        raise EnvironmentContractError("INVALID_ISOLATION_STATUS")

    return {
        "schema_version": ENVIRONMENT_SCHEMA_VERSION,
        "platform": {
            "system": _identity(platform.get("system"), "MISSING_PLATFORM_SYSTEM"),
            "architecture": _identity(
                platform.get("architecture"), "MISSING_ARCHITECTURE"
            ),
            "kernel_release": _identity(
                platform.get("kernel_release"), "MISSING_KERNEL_RELEASE"
            ),
            "wsl_release": _identity(
                platform.get("wsl_release"), "INVALID_WSL_RELEASE", nullable=True
            ),
        },
        "cpu": {
            "model": _identity(cpu.get("model"), "MISSING_CPU_MODEL"),
            "logical_count": _integer(
                cpu.get("logical_count"), "INVALID_LOGICAL_CPU_COUNT", minimum=1
            ),
        },
        "compiler": {
            "version": _identity(
                compiler.get("version"), "MISSING_COMPILER_VERSION"
            ),
            "target": _identity(compiler.get("target"), "MISSING_COMPILER_TARGET"),
        },
        "build": normalize_build_contract(root.get("build")),
        "runtimes": _runtime_versions(root.get("runtimes")),
        "governor": {"status": governor_status, "value": governor_value},
        "isolation": {
            "policy": _identity(
                isolation.get("policy"), "MISSING_ISOLATION_POLICY"
            ),
            "status": isolation_status,
            "level": _identity(isolation.get("level"), "MISSING_ISOLATION_LEVEL"),
        },
    }


def stable_environment_fingerprint(environment: Any) -> str:
    payload = canonical_json_bytes(stable_environment_payload(environment))
    return hashlib.sha256(payload).hexdigest()


def environment_with_fingerprint(environment: Any) -> dict[str, Any]:
    result = copy.deepcopy(_mapping(environment, "ENVIRONMENT_NOT_OBJECT"))
    result.pop("stable_fingerprint", None)
    result["stable_fingerprint"] = {
        "algorithm": FINGERPRINT_ALGORITHM,
        "contract_version": FINGERPRINT_CONTRACT_VERSION,
        "value": stable_environment_fingerprint(result),
    }
    return result


def _validate_source_snapshot(source: Any, suffix: str = "") -> list[str]:
    if not isinstance(source, dict):
        return [f"MISSING_SOURCE{suffix}"]
    issues: list[str] = []
    if source.get("contract_version") != SOURCE_IDENTITY_CONTRACT_VERSION:
        issues.append(f"MISSING_SOURCE{suffix}_CONTRACT_VERSION")
    commit = source.get("commit")
    if not isinstance(commit, str) or _COMMIT_PATTERN.fullmatch(commit) is None:
        issues.append(f"INVALID_SOURCE{suffix}_COMMIT")
    try:
        dirty = _boolean(source.get("dirty"), f"INVALID_SOURCE{suffix}_DIRTY")
    except EnvironmentContractError as exc:
        issues.append(exc.code)
        dirty = False
    digest = source.get("dirty_tree_digest")
    if dirty:
        if not isinstance(digest, str) or _HEX_DIGEST_PATTERN.fullmatch(digest) is None:
            issues.append(f"INVALID_SOURCE{suffix}_DIRTY_TREE_DIGEST")
    elif digest is not None:
        issues.append(f"INVALID_SOURCE{suffix}_DIRTY_TREE_DIGEST")
    return issues


def _source_snapshot(source: dict[str, Any]) -> dict[str, Any]:
    return {
        key: source.get(key)
        for key in (
            "contract_version",
            "commit",
            "dirty",
            "dirty_tree_digest",
        )
    }


def _validate_source(source: Any) -> list[str]:
    if not isinstance(source, dict):
        return ["MISSING_SOURCE"]
    issues = _validate_source_snapshot(source)
    if source.get("finalized") is not True:
        issues.append("SOURCE_NOT_FINALIZED")
    after = source.get("after")
    if not isinstance(after, dict):
        issues.append("MISSING_SOURCE_AFTER")
    else:
        issues.extend(_validate_source_snapshot(after, "_AFTER"))
    try:
        changed = _boolean(
            source.get("changed_during_run"), "INVALID_SOURCE_CHANGED_DURING_RUN"
        )
        if changed:
            issues.append("SOURCE_CHANGED_DURING_RUN")
        if isinstance(after, dict) and changed != (_source_snapshot(source) != after):
            issues.append("SOURCE_CHANGE_MARKER_MISMATCH")
    except EnvironmentContractError as exc:
        issues.append(exc.code)
    return issues


def _validate_volatile(volatile: Any) -> list[str]:
    if not isinstance(volatile, dict):
        return ["MISSING_VOLATILE"]
    issues: list[str] = []
    for field in ("captured_at_utc", "completed_at_utc", "repo_path", "build_path"):
        if not isinstance(volatile.get(field), str) or not volatile[field]:
            issues.append(f"INVALID_{field.upper()}")
    process_id = volatile.get("process_id")
    if isinstance(process_id, bool) or not isinstance(process_id, int) or process_id <= 0:
        issues.append("INVALID_PROCESS_ID")
    for field in ("load_average_start", "load_average_end"):
        values = volatile.get(field)
        if (
            not isinstance(values, list)
            or len(values) != 3
            or any(
                isinstance(value, bool)
                or not isinstance(value, (int, float))
                or not math.isfinite(float(value))
                for value in values
            )
        ):
            issues.append(f"INVALID_{field.upper()}")
    return issues


def validate_environment_contract(environment: Any) -> list[str]:
    try:
        stable_environment_payload(environment)
    except EnvironmentContractError as exc:
        return [exc.code]
    root = _mapping(environment, "ENVIRONMENT_NOT_OBJECT")
    issues = _validate_source(root.get("source"))
    issues.extend(_validate_volatile(root.get("volatile")))

    compiler = root.get("compiler")
    if not isinstance(compiler, dict) or not isinstance(compiler.get("path"), str) or not compiler["path"]:
        issues.append("INVALID_COMPILER_PATH")
    runtimes = root.get("runtimes")
    if isinstance(runtimes, dict):
        for name, runtime in runtimes.items():
            if not isinstance(runtime, dict):
                continue
            path = runtime.get("path")
            if runtime.get("status") == "available" and (
                not isinstance(path, str) or not path
            ):
                issues.append(f"INVALID_RUNTIME_PATH_{str(name).upper()}")
            if runtime.get("status") == "unavailable" and path is not None:
                issues.append(f"INVALID_RUNTIME_PATH_{str(name).upper()}")

    isolation = root.get("isolation")
    if isinstance(isolation, dict):
        selected = isolation.get("selected_cpu")
        observed = isolation.get("observed_mask")
        if isolation.get("status") == "ISOLATED":
            if (
                isinstance(selected, bool)
                or not isinstance(selected, int)
                or selected < 0
                or observed != str(selected)
            ):
                issues.append("ISOLATION_MASK_NOT_VERIFIED")
        elif not isinstance(observed, str) or not observed:
            issues.append("INVALID_OBSERVED_AFFINITY_MASK")

    fingerprint = root.get("stable_fingerprint")
    if not isinstance(fingerprint, dict):
        issues.append("FINGERPRINT_MISSING")
    elif (
        fingerprint.get("algorithm") != FINGERPRINT_ALGORITHM
        or fingerprint.get("contract_version") != FINGERPRINT_CONTRACT_VERSION
        or not isinstance(fingerprint.get("value"), str)
        or _HEX_DIGEST_PATTERN.fullmatch(fingerprint["value"]) is None
    ):
        issues.append("INVALID_FINGERPRINT")
    elif fingerprint["value"] != stable_environment_fingerprint(root):
        issues.append("FINGERPRINT_MISMATCH")
    return issues


_STABLE_COMPARISON_FIELDS = (
    (("cpu", "model"), "CPU_MODEL_MISMATCH"),
    (("platform", "architecture"), "ARCHITECTURE_MISMATCH"),
    (("cpu", "logical_count"), "LOGICAL_CPU_COUNT_MISMATCH"),
    (("platform", "kernel_release"), "KERNEL_RELEASE_MISMATCH"),
    (("platform", "wsl_release"), "WSL_RELEASE_MISMATCH"),
    (("compiler", "version"), "COMPILER_VERSION_MISMATCH"),
    (("compiler", "target"), "COMPILER_TARGET_MISMATCH"),
    (("build", "generator"), "BUILD_GENERATOR_MISMATCH"),
    (("build", "configuration"), "BUILD_CONFIGURATION_MISMATCH"),
    (("build", "flags"), "BUILD_FLAGS_MISMATCH"),
    (("build", "performance_options"), "BUILD_PERFORMANCE_OPTIONS_MISMATCH"),
    (("build", "toolchain_file"), "BUILD_TOOLCHAIN_FILE_MISMATCH"),
    (("build", "target_evidence"), "BUILD_TARGET_EVIDENCE_MISMATCH"),
    (("runtimes",), "RUNTIME_VERSIONS_MISMATCH"),
    (("governor",), "GOVERNOR_MISMATCH"),
    (("isolation", "policy"), "ISOLATION_POLICY_MISMATCH"),
    (("isolation", "level"), "ISOLATION_LEVEL_MISMATCH"),
)


def _nested_value(value: dict[str, Any], path: tuple[str, ...]) -> Any:
    result: Any = value
    for key in path:
        result = result[key]
    return result


def compare_environment_contracts(baseline: Any, current: Any) -> list[str]:
    baseline_issues = validate_environment_contract(baseline)
    current_issues = validate_environment_contract(current)
    reasons = [f"BASELINE_{issue}" for issue in baseline_issues]
    reasons.extend(f"CURRENT_{issue}" for issue in current_issues)
    if (
        isinstance(baseline, dict)
        and isinstance(baseline.get("isolation"), dict)
        and baseline["isolation"].get("status") == "NON_ISOLATED"
    ):
        reasons.append("BASELINE_NON_ISOLATED")
    if (
        isinstance(current, dict)
        and isinstance(current.get("isolation"), dict)
        and current["isolation"].get("status") == "NON_ISOLATED"
    ):
        reasons.append("CURRENT_NON_ISOLATED")
    if baseline_issues or current_issues:
        return reasons

    baseline_payload = stable_environment_payload(baseline)
    current_payload = stable_environment_payload(current)
    for path, reason in _STABLE_COMPARISON_FIELDS:
        if _nested_value(baseline_payload, path) != _nested_value(current_payload, path):
            reasons.append(reason)
    if baseline["stable_fingerprint"]["value"] != current["stable_fingerprint"]["value"]:
        reasons.append("ENVIRONMENT_FINGERPRINT_MISMATCH")
    return reasons
