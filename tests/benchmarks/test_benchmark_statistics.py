#!/usr/bin/env python3
from __future__ import annotations

import json
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
BENCHMARK_SCRIPT_ROOT = REPOSITORY_ROOT / "scripts" / "benchmark"
STATISTICS_SCRIPT = BENCHMARK_SCRIPT_ROOT / "benchmark_statistics.py"
sys.path.insert(0, str(BENCHMARK_SCRIPT_ROOT))

from benchmark_statistics import (  # noqa: E402
    BOOTSTRAP_ALGORITHM,
    BOOTSTRAP_VERSION,
    DEFAULT_BOOTSTRAP_RESAMPLES,
    SplitMix64,
    calculate_statistics,
    coefficient_of_variation,
    median_absolute_deviation,
    sample_standard_deviation,
    serialize_statistics,
)


class BenchmarkStatisticsTests(unittest.TestCase):
    def test_splitmix64_matches_the_version_one_sequence(self) -> None:
        generator = SplitMix64(0)

        self.assertEqual(0xE220A8397B1DCDAF, generator.next_uint64())
        self.assertEqual(0x6E789E6AA1B965F4, generator.next_uint64())
        self.assertEqual(0x06C45D188009454F, generator.next_uint64())

    def test_known_sample_standard_deviation_and_mad(self) -> None:
        samples = [1.0, 2.0, 3.0, 4.0]

        self.assertAlmostEqual(math.sqrt(5.0 / 3.0), sample_standard_deviation(samples))
        self.assertEqual(1.0, median_absolute_deviation(samples))
        self.assertAlmostEqual(
            math.sqrt(5.0 / 3.0) / 2.5,
            coefficient_of_variation(samples),
        )

    def test_large_finite_samples_do_not_overflow_intermediate_calculations(self) -> None:
        constant = calculate_statistics([1e308, 1e308], seed=0)

        self.assertEqual(1e308, constant["mean"])
        self.assertEqual(1e308, constant["median"])
        self.assertEqual(0.0, constant["sample_stddev"])
        self.assertTrue(math.isfinite(sample_standard_deviation([1.0, 1e308])))

    def test_smallest_positive_finite_samples_do_not_underflow_the_mean(self) -> None:
        smallest = math.ulp(0.0)

        result = calculate_statistics([smallest, smallest], seed=0)

        self.assertEqual(smallest, result["mean"])
        self.assertEqual(0.0, result["sample_stddev"])
        self.assertEqual(0.0, result["coefficient_of_variation"])
        self.assertEqual("STABLE", result["stability"]["status"])

    def test_adjacent_subnormal_values_do_not_underflow_the_median(self) -> None:
        smallest = math.ulp(0.0)

        result = calculate_statistics([smallest, 2.0 * smallest], seed=0)

        self.assertEqual(2.0 * smallest, result["median"])

    def test_constant_samples_have_zero_dispersion_and_exact_bootstrap_interval(self) -> None:
        result = calculate_statistics([10] * 10, seed=7)

        self.assertEqual(10, result["count"])
        self.assertEqual(10.0, result["mean"])
        self.assertEqual(10.0, result["median"])
        self.assertEqual(0.0, result["sample_stddev"])
        self.assertEqual(0.0, result["mad"])
        self.assertEqual(0.0, result["coefficient_of_variation"])
        self.assertEqual(
            {"stable": True, "status": "STABLE", "threshold": 0.05},
            result["stability"],
        )
        self.assertEqual(
            {
                "algorithm": BOOTSTRAP_ALGORITHM,
                "version": BOOTSTRAP_VERSION,
                "seed": 7,
                "statistic": "median",
                "resamples": DEFAULT_BOOTSTRAP_RESAMPLES,
                "low": 10.0,
                "high": 10.0,
            },
            result["median_bootstrap_95_ci"],
        )

    def test_cv_threshold_marks_stable_and_unstable_samples(self) -> None:
        stable = calculate_statistics([100, 100, 101, 99, 100], seed=1)
        unstable = calculate_statistics([1, 1, 1, 10], seed=1)

        self.assertTrue(stable["stability"]["stable"])
        self.assertEqual("STABLE", stable["stability"]["status"])
        self.assertFalse(unstable["stability"]["stable"])
        self.assertEqual("UNSTABLE", unstable["stability"]["status"])

    def test_statistics_reject_malformed_samples_seed_and_threshold(self) -> None:
        invalid_samples = (
            None,
            [],
            [1],
            [1] * 21,
            [True, 2],
            ["1", 2],
            [0, 1],
            [-1, 2],
            [math.nan, 1],
            [math.inf, 1],
        )
        for samples in invalid_samples:
            with self.subTest(samples=samples):
                with self.assertRaises(ValueError):
                    calculate_statistics(samples, seed=0)

        for seed in (-1, 1 << 64, True, 1.5):
            with self.subTest(seed=seed):
                with self.assertRaises(ValueError):
                    calculate_statistics([1, 2], seed=seed)

        for threshold in (0, -0.1, math.inf, True, "0.05", 10**1000):
            with self.subTest(threshold=threshold):
                with self.assertRaises(ValueError):
                    calculate_statistics([1, 2], seed=0, stability_threshold=threshold)

    def test_serialized_statistics_are_byte_identical_for_the_same_seed(self) -> None:
        first = serialize_statistics([2, 3, 5, 7, 11, 13], seed=0x12345678)
        second = serialize_statistics([2, 3, 5, 7, 11, 13], seed=0x12345678)

        self.assertEqual(first, second)
        self.assertTrue(first.endswith(b"\n"))
        self.assertNotIn(b"\r\n", first)
        self.assertEqual(
            {"low": 2.5, "high": 12.0},
            {
                key: json.loads(first)["median_bootstrap_95_ci"][key]
                for key in ("low", "high")
            },
        )

    def test_cli_is_deterministic_for_stdin_and_file_io(self) -> None:
        request = json.dumps(
            {"samples": [2, 3, 5, 7, 11, 13], "seed": 305419896},
            separators=(",", ":"),
        ).encode("ascii")

        first = subprocess.run(
            [sys.executable, str(STATISTICS_SCRIPT)],
            input=request,
            check=False,
            capture_output=True,
        )
        second = subprocess.run(
            [sys.executable, str(STATISTICS_SCRIPT)],
            input=request,
            check=False,
            capture_output=True,
        )
        self.assertEqual(0, first.returncode, first.stderr.decode("utf-8"))
        self.assertEqual(first.stdout, second.stdout)
        self.assertEqual("median", json.loads(first.stdout)["median_bootstrap_95_ci"]["statistic"])

        with tempfile.TemporaryDirectory() as temporary_directory:
            input_path = Path(temporary_directory) / "statistics-input.json"
            output_path = Path(temporary_directory) / "statistics-output.json"
            input_path.write_bytes(request)
            file_result = subprocess.run(
                [
                    sys.executable,
                    str(STATISTICS_SCRIPT),
                    "--input",
                    str(input_path),
                    "--output",
                    str(output_path),
                ],
                check=False,
                capture_output=True,
            )

            self.assertEqual(0, file_result.returncode, file_result.stderr.decode("utf-8"))
            self.assertEqual(first.stdout, output_path.read_bytes())
            self.assertEqual(b"", file_result.stdout)

    def test_cli_rejects_unknown_fields_and_malformed_json(self) -> None:
        cases = (
            b"not-json",
            b"[]",
            b'{"samples":[1,2],"seed":0,"extra":true}',
            b'{"samples":[1,2]}',
        )
        for request in cases:
            with self.subTest(request=request):
                result = subprocess.run(
                    [sys.executable, str(STATISTICS_SCRIPT)],
                    input=request,
                    check=False,
                    capture_output=True,
                )
                self.assertEqual(2, result.returncode)
                self.assertTrue(result.stderr.startswith(b"error: "))
                self.assertEqual(b"", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
