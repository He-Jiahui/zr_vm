#!/usr/bin/env python3
"""Build deterministic filtered and shuffled benchmark execution plans."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Sequence

from benchmark_statistics import (
    SPLITMIX64_ALGORITHM,
    SPLITMIX64_VERSION,
    SplitMix64,
)


EXECUTION_PLAN_SCHEMA_VERSION = 1
SHUFFLE_ALGORITHM = "fisher_yates_splitmix64"
SHUFFLE_VERSION = 1


def _validate_identity(value: Any, location: str) -> str:
    if not isinstance(value, str) or not value or value.strip() != value:
        raise ValueError(f"{location} must be a non-empty string without surrounding whitespace")
    return value


def validate_jobs(jobs: Any) -> tuple[dict[str, str], ...]:
    if not isinstance(jobs, list):
        raise ValueError("jobs must be a non-empty JSON array")
    if not jobs:
        raise ValueError("jobs must be a non-empty JSON array")

    validated: list[dict[str, str]] = []
    identities: set[tuple[str, str]] = set()
    for index, job in enumerate(jobs):
        location = f"jobs[{index}]"
        if not isinstance(job, dict):
            raise ValueError(f"{location} must be an object")
        fields = set(job)
        expected_fields = {"case", "implementation"}
        if fields != expected_fields:
            missing = sorted(expected_fields - fields)
            unknown = sorted(fields - expected_fields)
            details: list[str] = []
            if missing:
                details.append(f"missing {', '.join(missing)}")
            if unknown:
                details.append(f"unknown {', '.join(unknown)}")
            raise ValueError(f"{location} has invalid fields ({'; '.join(details)})")
        case = _validate_identity(job["case"], f"{location}.case")
        implementation = _validate_identity(
            job["implementation"],
            f"{location}.implementation",
        )
        identity = (case, implementation)
        if identity in identities:
            raise ValueError(
                f"duplicate job identity case={case!r}, implementation={implementation!r}"
            )
        identities.add(identity)
        validated.append({"case": case, "implementation": implementation})
    return tuple(validated)


def _validate_filter(values: Any, name: str) -> tuple[str, ...] | None:
    if values is None:
        return None
    if not isinstance(values, (list, tuple)) or not values:
        raise ValueError(f"{name} filter must be a non-empty array")
    validated: list[str] = []
    seen: set[str] = set()
    for index, value in enumerate(values):
        identity = _validate_identity(value, f"{name}[{index}]")
        if identity in seen:
            raise ValueError(f"{name} filter contains duplicate identity {identity!r}")
        seen.add(identity)
        validated.append(identity)
    return tuple(validated)


def _filter_validated_jobs(
    jobs: Sequence[dict[str, str]],
    cases: tuple[str, ...] | None,
    implementations: tuple[str, ...] | None,
) -> list[dict[str, str]]:
    case_filter = set(cases) if cases is not None else None
    implementation_filter = (
        set(implementations) if implementations is not None else None
    )
    filtered = [
        dict(job)
        for job in jobs
        if (case_filter is None or job["case"] in case_filter)
        and (
            implementation_filter is None
            or job["implementation"] in implementation_filter
        )
    ]
    if not filtered:
        raise ValueError("filters select no benchmark jobs")
    return filtered


def filter_jobs(
    jobs: Any,
    *,
    cases: Any = None,
    implementations: Any = None,
) -> list[dict[str, str]]:
    validated_jobs = validate_jobs(jobs)
    validated_cases = _validate_filter(cases, "cases")
    validated_implementations = _validate_filter(
        implementations,
        "implementations",
    )
    return _filter_validated_jobs(
        validated_jobs,
        validated_cases,
        validated_implementations,
    )


def _shuffle_validated_jobs(
    jobs: Sequence[dict[str, str]],
    seed: int,
) -> list[dict[str, str]]:
    generator = SplitMix64(seed)
    shuffled = [dict(job) for job in jobs]
    for index in range(len(shuffled) - 1, 0, -1):
        swap_index = generator.next_uint64() % (index + 1)
        shuffled[index], shuffled[swap_index] = (
            shuffled[swap_index],
            shuffled[index],
        )
    return shuffled


def shuffle_jobs(jobs: Any, *, seed: int) -> list[dict[str, str]]:
    return _shuffle_validated_jobs(validate_jobs(jobs), seed)


def create_execution_plan(
    jobs: Any,
    *,
    seed: int,
    cases: Any = None,
    implementations: Any = None,
) -> dict[str, Any]:
    validated_jobs = validate_jobs(jobs)
    validated_cases = _validate_filter(cases, "cases")
    validated_implementations = _validate_filter(
        implementations,
        "implementations",
    )
    filtered_jobs = _filter_validated_jobs(
        validated_jobs,
        validated_cases,
        validated_implementations,
    )
    shuffled_jobs = _shuffle_validated_jobs(filtered_jobs, seed)
    return {
        "schema_version": EXECUTION_PLAN_SCHEMA_VERSION,
        "algorithm": SHUFFLE_ALGORITHM,
        "version": SHUFFLE_VERSION,
        "rng": {
            "algorithm": SPLITMIX64_ALGORITHM,
            "version": SPLITMIX64_VERSION,
        },
        "seed": seed,
        "filters": {
            "cases": list(validated_cases) if validated_cases is not None else None,
            "implementations": (
                list(validated_implementations)
                if validated_implementations is not None
                else None
            ),
        },
        "job_count": len(shuffled_jobs),
        "jobs": shuffled_jobs,
    }


build_execution_plan = create_execution_plan


def _serialize_payload(payload: Any) -> bytes:
    rendered = json.dumps(
        payload,
        ensure_ascii=True,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    )
    return (rendered + "\n").encode("utf-8")


def serialize_execution_plan(
    jobs: Any,
    *,
    seed: int,
    cases: Any = None,
    implementations: Any = None,
) -> bytes:
    return _serialize_payload(
        create_execution_plan(
            jobs,
            seed=seed,
            cases=cases,
            implementations=implementations,
        )
    )


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"invalid JSON constant {value!r}")


def _object_without_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON field {key!r}")
        result[key] = value
    return result


def _parse_request(raw: bytes) -> tuple[Any, int, Any, Any]:
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError("input is not valid UTF-8") from exc
    try:
        request = json.loads(
            text,
            parse_constant=_reject_json_constant,
            object_pairs_hook=_object_without_duplicate_keys,
        )
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON: {exc.msg}") from exc
    if not isinstance(request, dict):
        raise ValueError("input must be a JSON object")
    allowed_fields = {"jobs", "seed", "filters"}
    unknown_fields = sorted(set(request) - allowed_fields)
    if unknown_fields:
        raise ValueError(f"unknown input field(s): {', '.join(unknown_fields)}")
    missing_fields = sorted({"jobs", "seed"} - set(request))
    if missing_fields:
        raise ValueError(f"missing input field(s): {', '.join(missing_fields)}")

    filters = request.get("filters", {})
    if not isinstance(filters, dict):
        raise ValueError("filters must be an object")
    unknown_filter_fields = sorted(set(filters) - {"cases", "implementations"})
    if unknown_filter_fields:
        raise ValueError(
            f"unknown filter field(s): {', '.join(unknown_filter_fields)}"
        )
    return (
        request["jobs"],
        request["seed"],
        filters.get("cases"),
        filters.get("implementations"),
    )


def _read_input(path: str) -> bytes:
    if path == "-":
        return sys.stdin.buffer.read()
    return Path(path).read_bytes()


def _write_output(path: str, payload: bytes) -> None:
    if path == "-":
        sys.stdout.buffer.write(payload)
        return
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(payload)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Filter and deterministically shuffle benchmark jobs from JSON."
    )
    parser.add_argument(
        "--input",
        default="-",
        help="Input JSON path, or - for stdin (default: -).",
    )
    parser.add_argument(
        "--output",
        default="-",
        help="Output JSON path, or - for stdout (default: -).",
    )
    args = parser.parse_args()
    try:
        jobs, seed, cases, implementations = _parse_request(_read_input(args.input))
        payload = serialize_execution_plan(
            jobs,
            seed=seed,
            cases=cases,
            implementations=implementations,
        )
        _write_output(args.output, payload)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
