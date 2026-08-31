#!/usr/bin/env python3
"""Deterministic descriptive statistics for benchmark samples."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any, Sequence


UINT64_MASK = (1 << 64) - 1
SPLITMIX64_INCREMENT = 0x9E3779B97F4A7C15
SPLITMIX64_MULTIPLIER_1 = 0xBF58476D1CE4E5B9
SPLITMIX64_MULTIPLIER_2 = 0x94D049BB133111EB
SPLITMIX64_ALGORITHM = "splitmix64"
SPLITMIX64_VERSION = 1

STATISTICS_SCHEMA_VERSION = 1
BOOTSTRAP_ALGORITHM = "median_percentile_bootstrap_splitmix64"
BOOTSTRAP_VERSION = 1
DEFAULT_BOOTSTRAP_RESAMPLES = 10_000
DEFAULT_STABILITY_THRESHOLD = 0.05
MAX_SAMPLE_COUNT = 20


class SplitMix64:
    """Small deterministic PRNG with a fixed cross-language contract."""

    def __init__(self, seed: int) -> None:
        self._state = _validate_seed(seed)

    def next_uint64(self) -> int:
        self._state = (self._state + SPLITMIX64_INCREMENT) & UINT64_MASK
        value = self._state
        value = ((value ^ (value >> 30)) * SPLITMIX64_MULTIPLIER_1) & UINT64_MASK
        value = ((value ^ (value >> 27)) * SPLITMIX64_MULTIPLIER_2) & UINT64_MASK
        return (value ^ (value >> 31)) & UINT64_MASK


def _validate_seed(seed: Any) -> int:
    if isinstance(seed, bool) or not isinstance(seed, int):
        raise ValueError("seed must be an integer in the uint64 range")
    if seed < 0 or seed > UINT64_MASK:
        raise ValueError("seed must be an integer in the uint64 range")
    return seed


def _validate_positive_finite_number(value: Any, location: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{location} must be a positive finite number")
    try:
        result = float(value)
    except OverflowError as exc:
        raise ValueError(f"{location} must be a positive finite number") from exc
    if not math.isfinite(result) or result <= 0.0:
        raise ValueError(f"{location} must be a positive finite number")
    return result


def validate_samples(samples: Any, *, minimum_count: int = 2) -> tuple[float, ...]:
    if not isinstance(samples, (list, tuple)):
        raise ValueError("samples must be an array of positive finite numbers")
    if len(samples) < minimum_count:
        raise ValueError(f"samples must contain at least {minimum_count} values")
    if len(samples) > MAX_SAMPLE_COUNT:
        raise ValueError(f"samples must contain at most {MAX_SAMPLE_COUNT} values")
    return tuple(
        _validate_positive_finite_number(value, f"samples[{index}]")
        for index, value in enumerate(samples)
    )


def _median_validated(samples: Sequence[float]) -> float:
    ordered = sorted(samples)
    midpoint = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[midpoint]
    lower = ordered[midpoint - 1]
    upper = ordered[midpoint]
    total = lower + upper
    if math.isfinite(total):
        return total / 2.0
    return lower / 2.0 + upper / 2.0


def median(samples: Any) -> float:
    return _median_validated(validate_samples(samples, minimum_count=1))


def _mean_validated(samples: Sequence[float]) -> float:
    sample_count = len(samples)
    maximum = max(samples)
    normalized_mean = math.fsum(sample / maximum for sample in samples) / sample_count
    result = maximum * normalized_mean
    if not math.isfinite(result):
        raise ValueError("samples produce a non-finite mean")
    return result


def _sample_standard_deviation_validated(
    samples: Sequence[float],
    sample_mean: float | None = None,
) -> float:
    mean_value = sample_mean if sample_mean is not None else _mean_validated(samples)
    deviation_norm = math.hypot(*(sample - mean_value for sample in samples))
    result = deviation_norm / math.sqrt(len(samples) - 1)
    if not math.isfinite(result):
        raise ValueError("samples produce a non-finite sample standard deviation")
    return result


def sample_standard_deviation(samples: Any) -> float:
    validated = validate_samples(samples)
    return _sample_standard_deviation_validated(validated)


def median_absolute_deviation(samples: Any) -> float:
    validated = validate_samples(samples, minimum_count=1)
    sample_median = _median_validated(validated)
    return _median_validated(tuple(abs(sample - sample_median) for sample in validated))


def coefficient_of_variation(samples: Any) -> float:
    validated = validate_samples(samples)
    sample_mean = _mean_validated(validated)
    return _sample_standard_deviation_validated(validated, sample_mean) / sample_mean


def _validate_stability_threshold(value: Any) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("stability_threshold must be a positive finite number")
    try:
        result = float(value)
    except OverflowError as exc:
        raise ValueError("stability_threshold must be a positive finite number") from exc
    if not math.isfinite(result) or result <= 0.0:
        raise ValueError("stability_threshold must be a positive finite number")
    return result


def is_stable(samples: Any, *, threshold: float = DEFAULT_STABILITY_THRESHOLD) -> bool:
    validated_threshold = _validate_stability_threshold(threshold)
    return coefficient_of_variation(samples) <= validated_threshold


def bootstrap_median_confidence_interval(samples: Any, *, seed: int) -> dict[str, Any]:
    validated = validate_samples(samples)
    validated_seed = _validate_seed(seed)
    generator = SplitMix64(validated_seed)
    sample_count = len(validated)
    bootstrap_medians: list[float] = []
    for _ in range(DEFAULT_BOOTSTRAP_RESAMPLES):
        resample = tuple(
            validated[generator.next_uint64() % sample_count]
            for _ in range(sample_count)
        )
        bootstrap_medians.append(_median_validated(resample))

    bootstrap_medians.sort()
    low_index = math.floor((DEFAULT_BOOTSTRAP_RESAMPLES - 1) * 0.025)
    high_index = math.floor((DEFAULT_BOOTSTRAP_RESAMPLES - 1) * 0.975)
    return {
        "algorithm": BOOTSTRAP_ALGORITHM,
        "version": BOOTSTRAP_VERSION,
        "seed": validated_seed,
        "statistic": "median",
        "resamples": DEFAULT_BOOTSTRAP_RESAMPLES,
        "low": bootstrap_medians[low_index],
        "high": bootstrap_medians[high_index],
    }


def calculate_statistics(
    samples: Any,
    *,
    seed: int,
    stability_threshold: float = DEFAULT_STABILITY_THRESHOLD,
) -> dict[str, Any]:
    validated = validate_samples(samples)
    validated_seed = _validate_seed(seed)
    threshold = _validate_stability_threshold(stability_threshold)
    sample_mean = _mean_validated(validated)
    sample_stddev = _sample_standard_deviation_validated(validated, sample_mean)
    sample_median = _median_validated(validated)
    sample_mad = _median_validated(
        tuple(abs(sample - sample_median) for sample in validated)
    )
    cv = sample_stddev / sample_mean
    stable = cv <= threshold
    return {
        "schema_version": STATISTICS_SCHEMA_VERSION,
        "count": len(validated),
        "mean": sample_mean,
        "sample_stddev": sample_stddev,
        "median": sample_median,
        "mad": sample_mad,
        "coefficient_of_variation": cv,
        "stability": {
            "threshold": threshold,
            "stable": stable,
            "status": "STABLE" if stable else "UNSTABLE",
        },
        "median_bootstrap_95_ci": bootstrap_median_confidence_interval(
            validated,
            seed=validated_seed,
        ),
    }


def _serialize_payload(payload: Any) -> bytes:
    rendered = json.dumps(
        payload,
        ensure_ascii=True,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    )
    return (rendered + "\n").encode("utf-8")


def serialize_statistics(
    samples: Any,
    *,
    seed: int,
    stability_threshold: float = DEFAULT_STABILITY_THRESHOLD,
) -> bytes:
    return _serialize_payload(
        calculate_statistics(
            samples,
            seed=seed,
            stability_threshold=stability_threshold,
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


def _parse_request(raw: bytes) -> tuple[Any, int, float]:
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
    allowed_fields = {"samples", "seed", "stability_threshold"}
    unknown_fields = sorted(set(request) - allowed_fields)
    if unknown_fields:
        raise ValueError(f"unknown input field(s): {', '.join(unknown_fields)}")
    missing_fields = sorted({"samples", "seed"} - set(request))
    if missing_fields:
        raise ValueError(f"missing input field(s): {', '.join(missing_fields)}")
    return (
        request["samples"],
        request["seed"],
        request.get("stability_threshold", DEFAULT_STABILITY_THRESHOLD),
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
        description="Calculate deterministic benchmark sample statistics from JSON."
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
        samples, seed, threshold = _parse_request(_read_input(args.input))
        payload = serialize_statistics(
            samples,
            seed=seed,
            stability_threshold=threshold,
        )
        _write_output(args.output, payload)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
