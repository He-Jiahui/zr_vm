#!/usr/bin/env python3
"""Deterministic Git and cache identities for benchmark artifacts."""

from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Any, BinaryIO, Sequence

from benchmark_environment_schema import (
    BUILD_CONTRACT_VERSION,
    CACHE_CONTRACT_VERSION,
    SOURCE_IDENTITY_CONTRACT_VERSION,
    canonical_json_bytes,
    normalize_build_contract,
    normalize_build_flags,
)


DEFAULT_GIT_SUBPROCESS_TIMEOUT_SECONDS = 30.0
HASH_CHUNK_SIZE = 1024 * 1024
_COMMIT_PATTERN = re.compile(r"^[0-9a-fA-F]{40,64}$")


def _validated_git_command(git_command: Sequence[str]) -> tuple[str, ...]:
    if (
        not isinstance(git_command, (tuple, list))
        or not git_command
        or any(not isinstance(part, str) or not part for part in git_command)
    ):
        raise ValueError("git command must be a non-empty sequence of strings")
    return tuple(git_command)


def _validate_timeout(timeout_seconds: float) -> float:
    if (
        isinstance(timeout_seconds, bool)
        or not isinstance(timeout_seconds, (int, float))
        or timeout_seconds <= 0
    ):
        raise ValueError("subprocess timeout must be positive")
    return float(timeout_seconds)


def _git_output(
    repository: Path,
    *arguments: str,
    git_command: Sequence[str],
    timeout_seconds: float,
) -> bytes:
    command = [*_validated_git_command(git_command), *arguments]
    timeout = _validate_timeout(timeout_seconds)
    try:
        result = subprocess.run(
            command,
            cwd=repository,
            check=False,
            capture_output=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as exc:
        raise ValueError(
            f"git {' '.join(arguments)} timed out after {timeout:g} seconds"
        ) from exc
    except OSError as exc:
        raise ValueError(f"could not execute git: {exc}") from exc
    if result.returncode != 0:
        message = result.stderr.decode("utf-8", errors="replace").strip()
        raise ValueError(f"git {' '.join(arguments)} failed: {message}")
    return result.stdout


def _hash_field(hasher: Any, value: bytes) -> None:
    hasher.update(len(value).to_bytes(8, byteorder="big", signed=False))
    hasher.update(value)


def _hash_bytes_record(
    hasher: Any,
    kind: bytes,
    name: bytes,
    mode: bytes,
    content: bytes,
) -> None:
    for field in (kind, name, mode, content):
        _hash_field(hasher, field)


def _hash_stream_record(
    hasher: Any,
    kind: bytes,
    name: bytes,
    mode: bytes,
    handle: BinaryIO,
    size: int,
) -> None:
    for field in (kind, name, mode):
        _hash_field(hasher, field)
    hasher.update(size.to_bytes(8, byteorder="big", signed=False))
    consumed = 0
    while True:
        chunk = handle.read(HASH_CHUNK_SIZE)
        if not chunk:
            break
        consumed += len(chunk)
        hasher.update(chunk)
    if consumed != size:
        raise ValueError("file changed while its benchmark source identity was hashed")


def _stream_tracked_diff(
    repository: Path,
    hasher: Any,
    *,
    git_command: Sequence[str],
    timeout_seconds: float,
) -> bool:
    timeout = _validate_timeout(timeout_seconds)
    with (
        tempfile.TemporaryDirectory(prefix="zr-vm-empty-order-") as order_directory,
        tempfile.TemporaryFile() as output,
        tempfile.TemporaryFile() as errors,
    ):
        order_file = Path(order_directory) / "order.txt"
        order_file.write_text("", encoding="ascii")
        arguments = (
            "-c",
            "core.quotePath=false",
            "diff",
            f"-O{order_file}",
            "--no-color",
            "--binary",
            "--full-index",
            "--no-ext-diff",
            "--no-textconv",
            "--no-renames",
            "--no-indent-heuristic",
            "--diff-algorithm=myers",
            "--unified=3",
            "--src-prefix=a/",
            "--dst-prefix=b/",
            "--submodule=short",
            "--ignore-submodules=none",
            "HEAD",
            "--",
        )
        command = [*_validated_git_command(git_command), *arguments]
        try:
            process = subprocess.Popen(
                command,
                cwd=repository,
                stdout=output,
                stderr=errors,
            )
        except OSError as exc:
            raise ValueError(f"could not execute git: {exc}") from exc
        try:
            returncode = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            process.kill()
            process.wait()
            raise ValueError(
                f"git diff timed out after {timeout:g} seconds"
            ) from exc
        if returncode != 0:
            errors.seek(0)
            message = errors.read().decode("utf-8", errors="replace").strip()
            raise ValueError(f"git diff failed: {message}")
        size = output.tell()
        if size == 0:
            return False
        output.seek(0)
        _hash_stream_record(
            hasher,
            b"tracked-diff-v2",
            b"",
            b"",
            output,
            size,
        )
        return True


def _gitlink_entries(
    repository: Path,
    *,
    git_command: Sequence[str],
    timeout_seconds: float,
) -> list[tuple[bytes, str]]:
    output = _git_output(
        repository,
        "ls-files",
        "--stage",
        "-z",
        git_command=git_command,
        timeout_seconds=timeout_seconds,
    )
    entries: list[tuple[bytes, str]] = []
    for raw_entry in output.split(b"\0"):
        if not raw_entry:
            continue
        try:
            metadata, raw_path = raw_entry.split(b"\t", 1)
            mode, object_id, stage = metadata.split(b" ")
        except ValueError as exc:
            raise ValueError("git returned a malformed index entry") from exc
        if mode == b"160000" and stage == b"0":
            entries.append((raw_path, object_id.decode("ascii").lower()))
    return sorted(entries)


def compute_dirty_tree_digest(
    repository: str | os.PathLike[str],
    *,
    git_command: Sequence[str] = ("git",),
    timeout_seconds: float = DEFAULT_GIT_SUBPROCESS_TIMEOUT_SECONDS,
) -> str | None:
    root = Path(repository).resolve()
    if not root.is_dir():
        raise ValueError(f"repository is not a directory: {root}")
    command = _validated_git_command(git_command)
    timeout = _validate_timeout(timeout_seconds)
    hasher = hashlib.sha256()
    gitlinks = _gitlink_entries(
        root,
        git_command=command,
        timeout_seconds=timeout,
    )
    for raw_path, _expected_commit in gitlinks:
        submodule = root / os.fsdecode(raw_path)
        if not (submodule / ".git").exists():
            raise ValueError(
                f"initialized submodule is unavailable: {os.fsdecode(raw_path)}"
            )
    dirty = _stream_tracked_diff(
        root,
        hasher,
        git_command=command,
        timeout_seconds=timeout,
    )

    untracked_raw = _git_output(
        root,
        "ls-files",
        "--others",
        "--exclude-standard",
        "-z",
        git_command=command,
        timeout_seconds=timeout,
    )
    for raw_name in sorted(name for name in untracked_raw.split(b"\0") if name):
        path = root / os.fsdecode(raw_name)
        if path.is_symlink():
            _hash_bytes_record(
                hasher,
                b"untracked-v2",
                raw_name,
                b"120000",
                os.fsencode(os.readlink(path)),
            )
        elif path.is_file():
            stat_result = path.stat()
            mode = b"100755" if stat_result.st_mode & 0o111 else b"100644"
            with path.open("rb") as handle:
                _hash_stream_record(
                    hasher,
                    b"untracked-v2",
                    raw_name,
                    mode,
                    handle,
                    stat_result.st_size,
                )
        else:
            _hash_bytes_record(
                hasher,
                b"untracked-v2",
                raw_name,
                b"missing",
                b"",
            )
        dirty = True

    for raw_path, expected_commit in gitlinks:
        submodule = root / os.fsdecode(raw_path)
        actual_commit = _git_output(
            submodule,
            "rev-parse",
            "--verify",
            "HEAD",
            git_command=command,
            timeout_seconds=timeout,
        ).decode("ascii").strip().lower()
        submodule_digest = compute_dirty_tree_digest(
            submodule,
            git_command=command,
            timeout_seconds=timeout,
        )
        if actual_commit != expected_commit or submodule_digest is not None:
            identity = canonical_json_bytes(
                {
                    "actual_commit": actual_commit,
                    "dirty_tree_digest": submodule_digest,
                    "expected_commit": expected_commit,
                }
            )
            _hash_bytes_record(
                hasher,
                b"initialized-submodule-v2",
                raw_path,
                b"160000",
                identity,
            )
            dirty = True
    return hasher.hexdigest() if dirty else None


def source_identity(
    repository: str | os.PathLike[str],
    *,
    git_command: Sequence[str] = ("git",),
    timeout_seconds: float = DEFAULT_GIT_SUBPROCESS_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    root = Path(repository).resolve()
    command = _validated_git_command(git_command)
    timeout = _validate_timeout(timeout_seconds)
    commit = _git_output(
        root,
        "rev-parse",
        "--verify",
        "HEAD",
        git_command=command,
        timeout_seconds=timeout,
    ).decode("ascii").strip().lower()
    if _COMMIT_PATTERN.fullmatch(commit) is None:
        raise ValueError(f"git returned an invalid commit identity: {commit!r}")
    dirty_digest = compute_dirty_tree_digest(
        root,
        git_command=command,
        timeout_seconds=timeout,
    )
    return {
        "contract_version": SOURCE_IDENTITY_CONTRACT_VERSION,
        "commit": commit,
        "dirty": dirty_digest is not None,
        "dirty_tree_digest": dirty_digest,
    }


def _slug(value: str) -> str:
    result = re.sub(r"[^a-z0-9._-]+", "-", value.lower()).strip("-._")
    return result or "unknown"


def _identity(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return " ".join(value.split())


def _resolved_executable(value: str) -> str:
    candidate = Path(value)
    if candidate.is_file():
        return str(candidate.resolve())
    resolved = shutil.which(value)
    if resolved is None:
        raise ValueError(f"compiler executable is unavailable: {value}")
    return str(Path(resolved).resolve())


def _default_build_contract(
    *,
    generator: str,
    configuration: str,
    flags: Any,
) -> dict[str, Any]:
    return {
        "contract_version": BUILD_CONTRACT_VERSION,
        "generator": generator,
        "configuration": configuration,
        "flags": flags,
        "performance_options": {
            "build_shared_libs": "UNSET",
            "interprocedural_optimization": "UNSET",
            "sanitizers": [],
            "abi_runtime": [],
            "project_compile_definitions": [],
            "project_compile_features": [],
            "project_compile_options": [],
        },
        "toolchain_file": {"path": None, "sha256": None},
        "target_evidence": {
            "status": "unavailable",
            "entry_count": 0,
            "sha256": None,
        },
    }


def compute_cache_identity(
    repository: str | os.PathLike[str],
    *,
    toolchain: str,
    compiler_version: str,
    compiler_target: str,
    generator: str,
    configuration: str,
    flags: Any,
    compiler_path: str | None = None,
    build_contract: Any = None,
    git_command: Sequence[str] = ("git",),
    timeout_seconds: float = DEFAULT_GIT_SUBPROCESS_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    source = source_identity(
        repository,
        git_command=git_command,
        timeout_seconds=timeout_seconds,
    )
    normalized_toolchain = _identity(toolchain, "toolchain")
    normalized_generator = _identity(generator, "generator")
    normalized_configuration = _identity(configuration, "configuration")
    normalized_flags = normalize_build_flags(flags)
    compiler_executable = _resolved_executable(compiler_path or normalized_toolchain)
    comparable = build_contract is not None
    normalized_build = normalize_build_contract(
        build_contract
        if build_contract is not None
        else _default_build_contract(
            generator=normalized_generator,
            configuration=normalized_configuration,
            flags=normalized_flags,
        ),
        allow_unavailable_target_evidence=not comparable,
    )
    if (
        normalized_build["generator"] != normalized_generator
        or normalized_build["configuration"] != normalized_configuration
        or normalized_build["flags"] != normalized_flags
    ):
        raise ValueError("cache build contract disagrees with generator, configuration, or flags")

    contract = {
        "schema_version": CACHE_CONTRACT_VERSION,
        "comparable": comparable and normalized_build["target_evidence"]["status"] == "available",
        "toolchain": normalized_toolchain,
        "compiler_path": compiler_executable,
        "compiler_version": _identity(compiler_version, "compiler version"),
        "compiler_target": _identity(compiler_target, "compiler target"),
        "generator": normalized_generator,
        "configuration": normalized_configuration,
        "flags": normalized_flags,
        "build_contract": normalized_build,
    }
    toolchain_digest = hashlib.sha256(canonical_json_bytes(contract)).hexdigest()
    source_contract = {
        "contract_version": SOURCE_IDENTITY_CONTRACT_VERSION,
        "commit": source["commit"],
        "dirty_tree_digest": source["dirty_tree_digest"],
    }
    source_digest = hashlib.sha256(canonical_json_bytes(source_contract)).hexdigest()
    if source["dirty"]:
        source_key = (
            f"{source['commit']}-dirty-{source['dirty_tree_digest']}-{source_digest}"
        )
    else:
        source_key = f"{source['commit']}-clean-{source_digest}"
    toolchain_key = "-".join(
        (
            _slug(normalized_toolchain),
            _slug(contract["compiler_target"]),
            _slug(normalized_configuration),
            toolchain_digest,
        )
    )
    return {
        "schema_version": CACHE_CONTRACT_VERSION,
        **source,
        "source_digest": source_digest,
        "toolchain_digest": toolchain_digest,
        "source_key": source_key,
        "toolchain_key": toolchain_key,
        "relative_path": f"{source_key}/{toolchain_key}",
        "toolchain_contract": contract,
        "comparable": contract["comparable"],
    }
