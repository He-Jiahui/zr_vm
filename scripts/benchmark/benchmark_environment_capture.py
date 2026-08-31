#!/usr/bin/env python3
"""Capture build, runtime, topology, and lifecycle benchmark evidence."""

from __future__ import annotations

import copy
import hashlib
import os
import platform
import re
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from benchmark_environment_schema import (
    BUILD_CONTRACT_VERSION,
    CAPTURE_STATUS_COMPLETE,
    CAPTURE_STATUS_IN_PROGRESS,
    ENVIRONMENT_SCHEMA_VERSION,
    EnvironmentContractError,
    canonical_json_bytes,
    environment_with_fingerprint,
    strict_json_loads,
)
from benchmark_source_identity import HASH_CHUNK_SIZE, source_identity


PROBE_SUBPROCESS_TIMEOUT_SECONDS = 10.0
_BUILD_FLAG_NAMES = (
    "CMAKE_C_FLAGS",
    "CMAKE_C_FLAGS_{CONFIG}",
    "CMAKE_EXE_LINKER_FLAGS",
    "CMAKE_EXE_LINKER_FLAGS_{CONFIG}",
    "CMAKE_SHARED_LINKER_FLAGS",
    "CMAKE_SHARED_LINKER_FLAGS_{CONFIG}",
)
_RUNTIME_PROBES = {
    "cargo": (("cargo",), ("--version",)),
    "dotnet": (("dotnet",), ("--version",)),
    "java": (("java",), ("-version",)),
    "javac": (("javac",), ("-version",)),
    "lua": (("lua", "lua54", "lua5.4", "luajit"), ("-v",)),
    "node": (("node",), ("--version",)),
    "python": (("python", "python3"), ("--version",)),
    "qjs": (("qjs", "quickjs"), ("-h",)),
    "valgrind": (("valgrind",), ("--version",)),
}
_SANITIZER_PATTERN = re.compile(
    r"(?:SANITIZER|SANITIZE|(?:^|_)(?:A|UB|T|M|L)SAN(?:_|$))"
)
_ABI_RUNTIME_MARKERS = (
    "ABI",
    "RUNTIME_LIBRARY",
    "CMAKE_C_STANDARD",
    "CMAKE_CXX_STANDARD",
    "GLIBCXX",
    "OSX_ARCHITECTURES",
    "SYSTEM_PROCESSOR",
)


def parse_cmake_cache(payload: str) -> dict[str, str]:
    if not isinstance(payload, str):
        raise ValueError("CMakeCache payload must be text")
    result: dict[str, str] = {}
    for line_number, line in enumerate(payload.splitlines(), start=1):
        if not line or line.startswith("//") or line.startswith("#"):
            continue
        match = re.fullmatch(r"([^:=]+):[^=]*=(.*)", line)
        if match is None:
            continue
        key, value = match.group(1), match.group(2)
        if key in result:
            raise ValueError(f"duplicate CMakeCache key {key!r} at line {line_number}")
        result[key] = value
    return result


def _sha256_file(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(HASH_CHUNK_SIZE)
            if not chunk:
                break
            hasher.update(chunk)
    return hasher.hexdigest()


def _cache_records(cache: dict[str, str], predicate: Any) -> list[dict[str, str]]:
    return [
        {"name": name, "value": cache[name]}
        for name in sorted(cache)
        if predicate(name.upper())
    ]


def _toolchain_file_identity(
    cache: dict[str, str],
    repository: Path,
    build_directory: Path,
) -> dict[str, str | None]:
    value = cache.get("CMAKE_TOOLCHAIN_FILE", "").strip()
    if not value:
        return {"path": None, "sha256": None}
    candidate = Path(value)
    if not candidate.is_absolute():
        build_candidate = build_directory / candidate
        candidate = build_candidate if build_candidate.exists() else repository / candidate
    resolved = candidate.resolve()
    if not resolved.is_file():
        raise ValueError(f"CMake toolchain file is unavailable: {resolved}")
    return {"path": str(resolved), "sha256": _sha256_file(resolved)}


def _replace_root(value: str, root: Path, marker: str) -> str:
    variants = {
        str(root.resolve()),
        str(root.resolve()).replace("\\", "/"),
        str(root.resolve()).replace("/", "\\"),
    }
    result = value
    for variant in sorted(variants, key=len, reverse=True):
        result = result.replace(variant, marker)
    return result.replace("\\", "/")


def _normalize_compile_entry(
    entry: Any,
    repository: Path,
    build_directory: Path,
) -> dict[str, Any]:
    if not isinstance(entry, dict):
        raise ValueError("compile_commands entry must be an object")
    if not isinstance(entry.get("file"), str) or not entry["file"]:
        raise ValueError("compile_commands entry is missing file")
    if "command" not in entry and "arguments" not in entry:
        raise ValueError("compile_commands entry is missing command or arguments")
    normalized: dict[str, Any] = {}
    for field in ("directory", "file", "output", "command"):
        if field in entry:
            if not isinstance(entry[field], str):
                raise ValueError(f"compile_commands {field} must be text")
            value = _replace_root(entry[field], repository, "$REPO")
            normalized[field] = _replace_root(value, build_directory, "$BUILD")
    if "arguments" in entry:
        if not isinstance(entry["arguments"], list) or any(
            not isinstance(value, str) for value in entry["arguments"]
        ):
            raise ValueError("compile_commands arguments must be a list of strings")
        normalized["arguments"] = [
            _replace_root(
                _replace_root(value, repository, "$REPO"),
                build_directory,
                "$BUILD",
            )
            for value in entry["arguments"]
        ]
    return normalized


def _target_compile_evidence(
    repository: Path,
    build_directory: Path,
) -> dict[str, Any]:
    path = build_directory / "compile_commands.json"
    if not path.is_file():
        return {"status": "unavailable", "entry_count": 0, "sha256": None}
    try:
        payload = strict_json_loads(path.read_bytes())
    except OSError as exc:
        raise ValueError(f"could not read {path}: {exc}") from exc
    if not isinstance(payload, list) or not payload:
        raise ValueError("compile_commands.json must contain a non-empty array")
    normalized = [
        _normalize_compile_entry(entry, repository, build_directory)
        for entry in payload
    ]
    normalized.sort(key=lambda entry: canonical_json_bytes(entry))
    return {
        "status": "available",
        "entry_count": len(normalized),
        "sha256": hashlib.sha256(canonical_json_bytes(normalized)).hexdigest(),
    }


def build_contract_from_cache(
    cache: dict[str, str],
    configuration: str | None,
    *,
    repository: str | os.PathLike[str],
    build_directory: str | os.PathLike[str],
) -> dict[str, Any]:
    if not isinstance(cache, dict):
        raise ValueError("CMake cache must be a mapping")
    generator = cache.get("CMAKE_GENERATOR")
    if not generator:
        raise ValueError("CMakeCache is missing CMAKE_GENERATOR")
    selected_configuration = configuration or cache.get("CMAKE_BUILD_TYPE")
    if not selected_configuration:
        raise ValueError("build configuration is required")
    config_key = re.sub(r"[^A-Za-z0-9_]", "_", selected_configuration).upper()
    flags = []
    for template in _BUILD_FLAG_NAMES:
        name = template.format(CONFIG=config_key)
        flags.append({"name": name, "value": cache.get(name, "")})

    shared_libraries = cache.get("BUILD_SHARED_LIBS")
    if shared_libraries is None:
        shared_libraries = cache.get("BUILD_SHARED_LIB", "UNSET")
    ipo = cache.get(f"CMAKE_INTERPROCEDURAL_OPTIMIZATION_{config_key}")
    if ipo is None:
        ipo = cache.get("CMAKE_INTERPROCEDURAL_OPTIMIZATION", "UNSET")
    performance_options = {
        "build_shared_libs": shared_libraries or "UNSET",
        "interprocedural_optimization": ipo or "UNSET",
        "sanitizers": _cache_records(
            cache,
            lambda name: _SANITIZER_PATTERN.search(name) is not None,
        ),
        "abi_runtime": _cache_records(
            cache,
            lambda name: any(marker in name for marker in _ABI_RUNTIME_MARKERS),
        ),
        "project_compile_definitions": _cache_records(
            cache,
            lambda name: "COMPILE_DEFINITIONS" in name,
        ),
        "project_compile_features": _cache_records(
            cache,
            lambda name: "COMPILE_FEATURES" in name,
        ),
        "project_compile_options": _cache_records(
            cache,
            lambda name: "COMPILE_OPTIONS" in name,
        ),
    }
    repo_root = Path(repository).resolve()
    build_root = Path(build_directory).resolve()
    return {
        "contract_version": BUILD_CONTRACT_VERSION,
        "generator": generator,
        "configuration": selected_configuration,
        "flags": flags,
        "performance_options": performance_options,
        "toolchain_file": _toolchain_file_identity(cache, repo_root, build_root),
        "target_evidence": _target_compile_evidence(repo_root, build_root),
    }


def parse_cpu_list(value: str) -> list[int]:
    if not isinstance(value, str) or not value:
        raise ValueError("CPU list must be a non-empty string")
    cpus: list[int] = []
    seen: set[int] = set()
    for part in value.split(","):
        if re.fullmatch(r"[0-9]+", part):
            first = last = int(part)
        else:
            match = re.fullmatch(r"([0-9]+)-([0-9]+)", part)
            if match is None:
                raise ValueError(f"invalid CPU list segment: {part!r}")
            first, last = (int(match.group(1)), int(match.group(2)))
            if last < first:
                raise ValueError(f"descending CPU range is invalid: {part!r}")
        for cpu in range(first, last + 1):
            if cpu in seen:
                raise ValueError(f"CPU list contains duplicate CPU {cpu}")
            seen.add(cpu)
            cpus.append(cpu)
    return cpus


def _read_topology_integer(path: Path, field: str) -> int:
    try:
        value = path.read_text(encoding="ascii").strip()
    except OSError as exc:
        raise ValueError(f"could not read CPU topology {field}: {exc}") from exc
    if re.fullmatch(r"[0-9]+", value) is None:
        raise ValueError(f"invalid CPU topology {field}: {value!r}")
    return int(value)


def select_allowed_cpu(
    allowed_list: str,
    *,
    sysfs_root: str | os.PathLike[str] = "/sys/devices/system/cpu",
    requested_cpu: int | None = None,
) -> dict[str, Any]:
    allowed = parse_cpu_list(allowed_list)
    if requested_cpu is not None:
        if isinstance(requested_cpu, bool) or not isinstance(requested_cpu, int):
            raise ValueError("requested CPU must be a non-negative integer")
        if requested_cpu not in allowed:
            raise ValueError(f"requested CPU {requested_cpu} is not in the allowed CPU set")
        candidates = [requested_cpu]
    else:
        candidates = allowed

    root = Path(sysfs_root)
    errors: list[str] = []
    for cpu in candidates:
        cpu_root = root / f"cpu{cpu}"
        online_path = cpu_root / "online"
        if online_path.exists():
            try:
                if online_path.read_text(encoding="ascii").strip() != "1":
                    continue
            except OSError as exc:
                errors.append(f"cpu{cpu}: could not read online state: {exc}")
                continue
        topology = cpu_root / "topology"
        try:
            package_id = _read_topology_integer(
                topology / "physical_package_id", "physical_package_id"
            )
            core_id = _read_topology_integer(topology / "core_id", "core_id")
            siblings = (topology / "thread_siblings_list").read_text(
                encoding="ascii"
            ).strip()
            if cpu not in parse_cpu_list(siblings):
                raise ValueError("selected CPU is absent from thread_siblings_list")
        except (OSError, ValueError) as exc:
            errors.append(f"cpu{cpu}: {exc}")
            continue
        return {
            "selected_cpu": cpu,
            "physical_package_id": package_id,
            "core_id": core_id,
            "thread_siblings_list": siblings,
        }
    details = "; ".join(errors) if errors else "no allowed CPU is online"
    raise ValueError(f"could not select an allowed CPU with valid topology: {details}")


def _load_average() -> list[float]:
    try:
        return [float(value) for value in os.getloadavg()]
    except (AttributeError, OSError):
        return [0.0, 0.0, 0.0]


def _utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _first_output_line(
    executable: str,
    arguments: tuple[str, ...],
    *,
    timeout_seconds: float = PROBE_SUBPROCESS_TIMEOUT_SECONDS,
) -> str | None:
    try:
        result = subprocess.run(
            [executable, *arguments],
            check=False,
            capture_output=True,
            timeout=timeout_seconds,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    output = (result.stdout + b"\n" + result.stderr).decode(
        "utf-8", errors="replace"
    )
    for line in output.splitlines():
        normalized = " ".join(line.split())
        if normalized:
            return normalized
    return None


def _resolve_executable(value: str) -> str:
    candidate = Path(value)
    if candidate.is_file():
        return str(candidate.resolve())
    resolved = shutil.which(value)
    if resolved is None:
        raise ValueError(f"executable is unavailable: {value}")
    return str(Path(resolved).resolve())


def _capture_runtimes() -> dict[str, dict[str, str | None]]:
    result: dict[str, dict[str, str | None]] = {}
    for name, (candidates, arguments) in _RUNTIME_PROBES.items():
        executable = next(
            (
                resolved
                for candidate in candidates
                if (resolved := shutil.which(candidate)) is not None
            ),
            None,
        )
        version = _first_output_line(executable, arguments) if executable else None
        if executable is None or version is None:
            result[name] = {"status": "unavailable", "path": None, "version": None}
        else:
            result[name] = {
                "status": "available",
                "path": str(Path(executable).resolve()),
                "version": version,
            }
    return result


def _cpu_model() -> str:
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        try:
            for line in cpuinfo.read_text(encoding="utf-8", errors="replace").splitlines():
                if line.lower().startswith("model name") and ":" in line:
                    model = " ".join(line.split(":", 1)[1].split())
                    if model:
                        return model
        except OSError:
            pass
    fallback = " ".join(platform.processor().split())
    return fallback or "unknown"


def _wsl_release(kernel_release: str) -> str | None:
    marker = kernel_release.lower()
    if "microsoft" not in marker:
        return None
    return "WSL2" if "wsl2" in marker or "microsoft-standard" in marker else "WSL1"


def _capture_governor(selected_cpu: int | None) -> dict[str, str | None]:
    cpu = selected_cpu if selected_cpu is not None else 0
    path = Path(f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_governor")
    try:
        value = path.read_text(encoding="ascii").strip()
    except OSError as exc:
        return {"status": "unavailable", "value": None, "reason": str(exc)}
    if not value:
        return {"status": "unavailable", "value": None, "reason": "empty governor value"}
    return {"status": "available", "value": value, "reason": None}


def capture_environment(
    repository: str | os.PathLike[str],
    build_directory: str | os.PathLike[str],
    *,
    isolation_status: str,
    selected_cpu: int | None,
    observed_mask: str,
    isolation_reason: str | None = None,
    configuration: str | None = None,
    process_id: int | None = None,
) -> dict[str, Any]:
    repo_root = Path(repository).resolve()
    build_root = Path(build_directory).resolve()
    cache_path = build_root / "CMakeCache.txt"
    try:
        cache = parse_cmake_cache(cache_path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError(f"could not read {cache_path}: {exc}") from exc
    compiler_value = cache.get("CMAKE_C_COMPILER")
    if not compiler_value:
        raise ValueError("CMakeCache is missing CMAKE_C_COMPILER")
    compiler_path = _resolve_executable(compiler_value)
    compiler_version = _first_output_line(compiler_path, ("--version",))
    compiler_target = _first_output_line(compiler_path, ("-dumpmachine",))
    if compiler_version is None or compiler_target is None:
        raise ValueError(f"could not identify compiler: {compiler_path}")
    build = build_contract_from_cache(
        cache,
        configuration,
        repository=repo_root,
        build_directory=build_root,
    )
    if build["target_evidence"]["status"] != "available":
        raise ValueError(
            "target compile evidence is required for a comparable benchmark capture"
        )
    kernel_release = platform.release()
    topology: dict[str, Any] = {}
    if selected_cpu is not None:
        topology = select_allowed_cpu(str(selected_cpu), requested_cpu=selected_cpu)
    before = source_identity(repo_root)
    return {
        "schema_version": ENVIRONMENT_SCHEMA_VERSION,
        "capture_status": CAPTURE_STATUS_IN_PROGRESS,
        "platform": {
            "system": platform.system() or "unknown",
            "architecture": platform.machine() or "unknown",
            "kernel_release": kernel_release or "unknown",
            "wsl_release": _wsl_release(kernel_release),
        },
        "cpu": {"model": _cpu_model(), "logical_count": os.cpu_count() or 1},
        "compiler": {
            "path": compiler_path,
            "version": compiler_version,
            "target": compiler_target,
        },
        "build": build,
        "runtimes": _capture_runtimes(),
        "governor": _capture_governor(selected_cpu),
        "isolation": {
            "policy": "single_logical_cpu_v1",
            "status": isolation_status,
            "level": "affinity_only",
            "selected_cpu": selected_cpu,
            "observed_mask": observed_mask,
            "reason": isolation_reason,
            **{key: value for key, value in topology.items() if key != "selected_cpu"},
        },
        "source": {
            **before,
            "after": None,
            "changed_during_run": False,
            "finalized": False,
        },
        "volatile": {
            "captured_at_utc": _utc_now(),
            "completed_at_utc": None,
            "load_average_start": _load_average(),
            "load_average_end": None,
            "process_id": process_id if process_id is not None else os.getpid(),
            "repo_path": str(repo_root),
            "build_path": str(build_root),
        },
    }


def finalize_environment(
    environment: Any,
    repository: str | os.PathLike[str],
) -> dict[str, Any]:
    if not isinstance(environment, dict):
        raise EnvironmentContractError("ENVIRONMENT_NOT_OBJECT")
    if environment.get("schema_version") != ENVIRONMENT_SCHEMA_VERSION:
        raise EnvironmentContractError("INVALID_SCHEMA_VERSION")
    if environment.get("capture_status") != CAPTURE_STATUS_IN_PROGRESS:
        raise EnvironmentContractError("CAPTURE_NOT_IN_PROGRESS")
    if "stable_fingerprint" in environment:
        raise EnvironmentContractError("IN_PROGRESS_FINGERPRINT_PRESENT")
    source = environment.get("source")
    volatile = environment.get("volatile")
    if not isinstance(source, dict):
        raise EnvironmentContractError("MISSING_SOURCE")
    if source.get("after") is not None or source.get("finalized") is not False:
        raise EnvironmentContractError("INVALID_IN_PROGRESS_SOURCE")
    if not isinstance(volatile, dict):
        raise EnvironmentContractError("MISSING_VOLATILE")
    if volatile.get("completed_at_utc") is not None or volatile.get("load_average_end") is not None:
        raise EnvironmentContractError("INVALID_IN_PROGRESS_VOLATILE")

    result = copy.deepcopy(environment)
    current = source_identity(repository)
    initial_identity = {
        key: result["source"].get(key)
        for key in (
            "contract_version",
            "commit",
            "dirty",
            "dirty_tree_digest",
        )
    }
    result["source"]["after"] = current
    result["source"]["changed_during_run"] = initial_identity != current
    result["volatile"]["completed_at_utc"] = _utc_now()
    result["volatile"]["load_average_end"] = _load_average()
    result["source"]["finalized"] = True
    result["capture_status"] = CAPTURE_STATUS_COMPLETE
    return environment_with_fingerprint(result)
