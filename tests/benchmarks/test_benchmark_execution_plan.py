#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
BENCHMARK_SCRIPT_ROOT = REPOSITORY_ROOT / "scripts" / "benchmark"
EXECUTION_PLAN_SCRIPT = BENCHMARK_SCRIPT_ROOT / "benchmark_execution_plan.py"
sys.path.insert(0, str(BENCHMARK_SCRIPT_ROOT))

from benchmark_execution_plan import (  # noqa: E402
    EXECUTION_PLAN_SCHEMA_VERSION,
    SHUFFLE_ALGORITHM,
    SHUFFLE_VERSION,
    create_execution_plan,
    serialize_execution_plan,
    shuffle_jobs,
)


def _jobs() -> list[dict[str, str]]:
    return [
        {"case": "a", "implementation": "zr"},
        {"case": "drop-1", "implementation": "python"},
        {"case": "b", "implementation": "zr"},
        {"case": "drop-2", "implementation": "python"},
        {"case": "c", "implementation": "zr"},
        {"case": "d", "implementation": "lua"},
    ]


def _identities(jobs: list[dict[str, str]]) -> list[tuple[str, str]]:
    return [(job["case"], job["implementation"]) for job in jobs]


class BenchmarkExecutionPlanTests(unittest.TestCase):
    def test_same_seed_reproduces_order_without_mutating_input(self) -> None:
        jobs = _jobs()
        original = [dict(job) for job in jobs]

        first = create_execution_plan(jobs, seed=42)
        second = create_execution_plan(jobs, seed=42)

        self.assertEqual(first, second)
        self.assertEqual(original, jobs)
        self.assertEqual(EXECUTION_PLAN_SCHEMA_VERSION, first["schema_version"])
        self.assertEqual(SHUFFLE_ALGORITHM, first["algorithm"])
        self.assertEqual(SHUFFLE_VERSION, first["version"])
        self.assertEqual(42, first["seed"])

    def test_different_seeds_change_nontrivial_order_and_preserve_each_job_once(self) -> None:
        jobs = _jobs()
        seed_zero = create_execution_plan(jobs, seed=0)["jobs"]
        seed_one = create_execution_plan(jobs, seed=1)["jobs"]

        self.assertNotEqual(seed_zero, seed_one)
        self.assertEqual(Counter(_identities(jobs)), Counter(_identities(seed_zero)))
        self.assertEqual(Counter(_identities(jobs)), Counter(_identities(seed_one)))

    def test_filtering_happens_before_shuffle(self) -> None:
        plan = create_execution_plan(
            _jobs(),
            seed=0,
            cases=["a", "b", "c", "d"],
        )

        self.assertEqual(
            ["c", "b", "a", "d"],
            [job["case"] for job in plan["jobs"]],
        )
        self.assertEqual(
            {
                "cases": ["a", "b", "c", "d"],
                "implementations": None,
            },
            plan["filters"],
        )
        self.assertEqual(4, plan["job_count"])

    def test_shuffle_jobs_matches_fixed_fisher_yates_vector(self) -> None:
        jobs = [
            {"case": name, "implementation": "zr"}
            for name in "abcdef"
        ]

        shuffled = shuffle_jobs(jobs, seed=0)

        self.assertEqual("ecfdab", "".join(job["case"] for job in shuffled))

    def test_rejects_malformed_duplicate_and_empty_job_identities(self) -> None:
        invalid_job_lists = (
            None,
            [],
            [{}],
            [{"case": "a"}],
            [{"case": "", "implementation": "zr"}],
            [{"case": "   ", "implementation": "zr"}],
            [{"case": " a", "implementation": "zr"}],
            [{"case": "a", "implementation": ""}],
            [{"case": "a", "implementation": 1}],
            [{"case": "a", "implementation": "zr", "extra": True}],
            [
                {"case": "a", "implementation": "zr"},
                {"case": "a", "implementation": "zr"},
            ],
        )
        for jobs in invalid_job_lists:
            with self.subTest(jobs=jobs):
                with self.assertRaises(ValueError):
                    create_execution_plan(jobs, seed=0)

    def test_rejects_invalid_seed_and_filters(self) -> None:
        for seed in (-1, 1 << 64, True, 1.5):
            with self.subTest(seed=seed):
                with self.assertRaises(ValueError):
                    create_execution_plan(_jobs(), seed=seed)

        invalid_filters = (
            {"cases": []},
            {"cases": [""]},
            {"cases": ["a", "a"]},
            {"cases": "a"},
            {"implementations": [" zr"]},
            {"implementations": [1]},
        )
        for filters in invalid_filters:
            with self.subTest(filters=filters):
                with self.assertRaises(ValueError):
                    create_execution_plan(_jobs(), seed=0, **filters)

        with self.assertRaises(ValueError):
            create_execution_plan(_jobs(), seed=0, cases=["not-present"])

    def test_serialized_plan_is_byte_identical_for_the_same_seed(self) -> None:
        first = serialize_execution_plan(_jobs(), seed=0x12345678)
        second = serialize_execution_plan(_jobs(), seed=0x12345678)

        self.assertEqual(first, second)
        self.assertTrue(first.endswith(b"\n"))
        self.assertNotIn(b"\r\n", first)

    def test_cli_supports_structured_stdin_and_file_io(self) -> None:
        request = {
            "jobs": _jobs(),
            "seed": 0,
            "filters": {"cases": ["a", "b", "c", "d"]},
        }
        request_bytes = json.dumps(request, separators=(",", ":")).encode("ascii")

        first = subprocess.run(
            [sys.executable, str(EXECUTION_PLAN_SCRIPT)],
            input=request_bytes,
            check=False,
            capture_output=True,
        )
        second = subprocess.run(
            [sys.executable, str(EXECUTION_PLAN_SCRIPT)],
            input=request_bytes,
            check=False,
            capture_output=True,
        )
        self.assertEqual(0, first.returncode, first.stderr.decode("utf-8"))
        self.assertEqual(first.stdout, second.stdout)
        self.assertEqual(["c", "b", "a", "d"], [
            job["case"] for job in json.loads(first.stdout)["jobs"]
        ])

        with tempfile.TemporaryDirectory() as temporary_directory:
            input_path = Path(temporary_directory) / "plan-input.json"
            output_path = Path(temporary_directory) / "plan-output.json"
            input_path.write_bytes(request_bytes)
            result = subprocess.run(
                [
                    sys.executable,
                    str(EXECUTION_PLAN_SCRIPT),
                    "--input",
                    str(input_path),
                    "--output",
                    str(output_path),
                ],
                check=False,
                capture_output=True,
            )

            self.assertEqual(0, result.returncode, result.stderr.decode("utf-8"))
            self.assertEqual(first.stdout, output_path.read_bytes())
            self.assertEqual(b"", result.stdout)

    def test_cli_rejects_malformed_duplicate_and_unknown_structures(self) -> None:
        cases = (
            b"not-json",
            b"[]",
            b'{"jobs":[],"seed":0}',
            b'{"jobs":[{"case":"a","implementation":"zr"}],"seed":0,"extra":1}',
            b'{"jobs":[{"case":"a","implementation":"zr"}],"seed":0,"filters":{"unknown":[]}}',
            b'{"jobs":[{"case":"a","implementation":"zr"},{"case":"a","implementation":"zr"}],"seed":0}',
        )
        for request in cases:
            with self.subTest(request=request):
                result = subprocess.run(
                    [sys.executable, str(EXECUTION_PLAN_SCRIPT)],
                    input=request,
                    check=False,
                    capture_output=True,
                )
                self.assertEqual(2, result.returncode)
                self.assertTrue(result.stderr.startswith(b"error: "))
                self.assertEqual(b"", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
