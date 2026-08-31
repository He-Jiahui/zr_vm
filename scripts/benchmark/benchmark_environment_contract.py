#!/usr/bin/env python3
"""Public API and CLI for reproducible benchmark environment contracts."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Any

from benchmark_environment_capture import (
    PROBE_SUBPROCESS_TIMEOUT_SECONDS,
    build_contract_from_cache,
    capture_environment,
    finalize_environment,
    parse_cmake_cache,
    parse_cpu_list,
    select_allowed_cpu,
)
from benchmark_environment_schema import (
    BUILD_CONTRACT_VERSION,
    CACHE_CONTRACT_VERSION,
    CAPTURE_STATUS_COMPLETE,
    CAPTURE_STATUS_IN_PROGRESS,
    ENVIRONMENT_SCHEMA_VERSION,
    FINGERPRINT_ALGORITHM,
    FINGERPRINT_CONTRACT_VERSION,
    REQUIRED_BUILD_IDENTITY_FIELDS,
    REQUIRED_PERFORMANCE_OPTION_FIELDS,
    REQUIRED_RUNTIME_NAMES,
    SOURCE_IDENTITY_CONTRACT_VERSION,
    EnvironmentContractError,
    atomic_write_json,
    canonical_json_bytes,
    compare_environment_contracts,
    environment_with_fingerprint,
    stable_environment_fingerprint,
    stable_environment_payload,
    strict_json_loads,
    validate_environment_contract,
)
from benchmark_source_identity import (
    DEFAULT_GIT_SUBPROCESS_TIMEOUT_SECONDS,
    HASH_CHUNK_SIZE,
    compute_cache_identity,
    compute_dirty_tree_digest,
    source_identity,
)


__all__ = (
    "BUILD_CONTRACT_VERSION",
    "CACHE_CONTRACT_VERSION",
    "CAPTURE_STATUS_COMPLETE",
    "CAPTURE_STATUS_IN_PROGRESS",
    "DEFAULT_GIT_SUBPROCESS_TIMEOUT_SECONDS",
    "ENVIRONMENT_SCHEMA_VERSION",
    "EnvironmentContractError",
    "FINGERPRINT_ALGORITHM",
    "FINGERPRINT_CONTRACT_VERSION",
    "HASH_CHUNK_SIZE",
    "PROBE_SUBPROCESS_TIMEOUT_SECONDS",
    "REQUIRED_BUILD_IDENTITY_FIELDS",
    "REQUIRED_PERFORMANCE_OPTION_FIELDS",
    "REQUIRED_RUNTIME_NAMES",
    "SOURCE_IDENTITY_CONTRACT_VERSION",
    "atomic_write_json",
    "build_contract_from_cache",
    "canonical_json_bytes",
    "capture_environment",
    "compare_environment_contracts",
    "compute_cache_identity",
    "compute_dirty_tree_digest",
    "environment_with_fingerprint",
    "finalize_environment",
    "main",
    "parse_cmake_cache",
    "parse_cpu_list",
    "select_allowed_cpu",
    "source_identity",
    "stable_environment_fingerprint",
    "stable_environment_payload",
    "strict_json_loads",
    "validate_environment_contract",
)


def _read_json_file(path: str | os.PathLike[str]) -> Any:
    try:
        return strict_json_loads(Path(path).read_bytes())
    except OSError as exc:
        raise ValueError(f"could not read {path}: {exc}") from exc


def _main_capture(args: argparse.Namespace) -> None:
    report = capture_environment(
        args.repo_root,
        args.build_dir,
        isolation_status=args.isolation_status,
        selected_cpu=args.selected_cpu,
        observed_mask=args.observed_mask,
        isolation_reason=args.isolation_reason,
        configuration=args.configuration,
        process_id=args.process_id,
    )
    atomic_write_json(args.output, report)


def _main_finalize(args: argparse.Namespace) -> None:
    report = finalize_environment(_read_json_file(args.input), args.repo_root)
    atomic_write_json(args.output or args.input, report)


def _main_select_cpu(args: argparse.Namespace) -> None:
    selected = select_allowed_cpu(
        args.allowed_list,
        requested_cpu=args.requested_cpu,
    )
    print(selected["selected_cpu"])


def _parse_flag_argument(value: str) -> dict[str, str]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("flag must use NAME=VALUE syntax")
    name, flag_value = value.split("=", 1)
    if not name:
        raise argparse.ArgumentTypeError("flag name must not be empty")
    return {"name": name, "value": flag_value}


def _main_cache_key(args: argparse.Namespace) -> None:
    build_contract = None
    if args.build_dir:
        build_directory = Path(args.build_dir).resolve()
        cache_path = build_directory / "CMakeCache.txt"
        try:
            cache = parse_cmake_cache(cache_path.read_text(encoding="utf-8"))
        except OSError as exc:
            raise ValueError(f"could not read {cache_path}: {exc}") from exc
        build_contract = build_contract_from_cache(
            cache,
            args.configuration,
            repository=args.repo_root,
            build_directory=build_directory,
        )
    identity = compute_cache_identity(
        args.repo_root,
        toolchain=args.toolchain,
        compiler_path=args.compiler_path,
        compiler_version=args.compiler_version,
        compiler_target=args.compiler_target,
        generator=args.generator,
        configuration=args.configuration,
        flags=args.flag,
        build_contract=build_contract,
    )
    if args.output:
        atomic_write_json(args.output, identity)
    else:
        sys.stdout.buffer.write(canonical_json_bytes(identity) + b"\n")


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    capture_parser = subparsers.add_parser("capture")
    capture_parser.add_argument("--repo-root", required=True)
    capture_parser.add_argument("--build-dir", required=True)
    capture_parser.add_argument("--output", required=True)
    capture_parser.add_argument(
        "--isolation-status",
        choices=("ISOLATED", "NON_ISOLATED"),
        required=True,
    )
    capture_parser.add_argument("--selected-cpu", type=int)
    capture_parser.add_argument("--observed-mask", required=True)
    capture_parser.add_argument("--isolation-reason")
    capture_parser.add_argument("--configuration")
    capture_parser.add_argument("--process-id", type=int)
    capture_parser.set_defaults(handler=_main_capture)

    finalize_parser = subparsers.add_parser("finalize")
    finalize_parser.add_argument("--repo-root", required=True)
    finalize_parser.add_argument("--input", required=True)
    finalize_parser.add_argument("--output")
    finalize_parser.set_defaults(handler=_main_finalize)

    select_parser = subparsers.add_parser("select-cpu")
    select_parser.add_argument("--allowed-list", required=True)
    select_parser.add_argument("--requested-cpu", type=int)
    select_parser.set_defaults(handler=_main_select_cpu)

    cache_parser = subparsers.add_parser("cache-key")
    cache_parser.add_argument("--repo-root", required=True)
    cache_parser.add_argument(
        "--build-dir",
        help="Configured build containing CMakeCache.txt and compile_commands.json.",
    )
    cache_parser.add_argument("--toolchain", required=True)
    cache_parser.add_argument("--compiler-path")
    cache_parser.add_argument("--compiler-version", required=True)
    cache_parser.add_argument("--compiler-target", required=True)
    cache_parser.add_argument("--generator", required=True)
    cache_parser.add_argument("--configuration", required=True)
    cache_parser.add_argument(
        "--flag",
        action="append",
        type=_parse_flag_argument,
        required=True,
    )
    cache_parser.add_argument("--output")
    cache_parser.set_defaults(handler=_main_cache_key)
    return parser


def main() -> int:
    parser = _argument_parser()
    args = parser.parse_args()
    try:
        args.handler(args)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
