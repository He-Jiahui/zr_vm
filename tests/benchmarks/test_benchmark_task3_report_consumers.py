#!/usr/bin/env python3

from __future__ import annotations

import pathlib
import sys
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "scripts" / "benchmark"))

import aggregate_benchmark_summary as aggregate  # noqa: E402
import benchmark_reports_to_csv as csv_report  # noqa: E402


def implementation(**updates: object) -> dict[str, object]:
    record: dict[str, object] = {
        "name": "C",
        "status": "PASS",
        "measurement_scope": "persistent_runtime",
        "prepare_scope": "runtime_start_before_measurement",
        "runtime_reused": True,
        "compiler_reused": False,
        "jit_state_reused": False,
    }
    record.update(updates)
    return record


class Task3ReportConsumerTests(unittest.TestCase):
    def assert_consumer_result(self, expected: bool, record: dict[str, object]) -> None:
        baseline = implementation()
        self.assertEqual(expected, aggregate._contract_is_comparable(record, baseline))
        self.assertEqual(expected, csv_report._records_are_comparable(record, baseline))

    def test_legacy_contract_remains_comparable(self) -> None:
        self.assert_consumer_result(True, implementation())

    def test_stable_task3_contract_is_comparable(self) -> None:
        task3 = {
            "stability": "STABLE",
            "comparable": True,
            "gate_eligible": True,
        }
        self.assert_consumer_result(True, implementation(**task3))

    def test_unstable_task3_contract_fails_closed(self) -> None:
        task3 = {
            "stability": "UNSTABLE",
            "comparable": True,
            "gate_eligible": True,
        }
        self.assert_consumer_result(False, implementation(**task3))

    def test_profile_task3_contract_fails_closed(self) -> None:
        task3 = {
            "stability": "NOT_COMPARABLE",
            "comparable": False,
            "gate_eligible": False,
        }
        self.assert_consumer_result(False, implementation(**task3))

    def test_gate_ineligible_task3_contract_fails_closed(self) -> None:
        task3 = {
            "stability": "STABLE",
            "comparable": True,
            "gate_eligible": False,
        }
        self.assert_consumer_result(False, implementation(**task3))


if __name__ == "__main__":
    unittest.main()
