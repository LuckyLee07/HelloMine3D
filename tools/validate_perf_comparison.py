#!/usr/bin/env python3
"""Runs the portable R1/Q1 comparator fixtures."""

from __future__ import annotations

import pathlib
import sys

from compare_perf_baselines import InvalidSummary, compare, read_summary


def run_case(
    fixture_root: pathlib.Path,
    name: str,
    baseline_name: str,
    candidate_name: str,
    expected: str,
) -> bool:
    try:
        result = compare(
            read_summary(fixture_root / baseline_name),
            read_summary(fixture_root / candidate_name),
        )
        actual = result.status
    except InvalidSummary:
        actual = "INVALID"
    passed = actual == expected
    print(
        f"[PERF_COMPARE_VERIFY] case={name} expected={expected} "
        f"actual={actual} status={'PASS' if passed else 'FAIL'}"
    )
    return passed


def main() -> int:
    fixture_root = pathlib.Path(__file__).resolve().parent / "fixtures" / "performance"
    cases = (
        ("r1-pass", "l4-baseline.summary.txt", "l4-baseline.summary.txt", "PASS"),
        ("r1-regression", "l4-baseline.summary.txt", "l4-regressed.summary.txt", "REGRESSION"),
        ("r1-incomparable", "l4-baseline.summary.txt", "incomparable.summary.txt", "INCOMPARABLE"),
        ("r1-invalid", "l4-baseline.summary.txt", "missing-metric.summary.txt", "INVALID"),
        ("q1-pass", "q1-baseline.summary.txt", "q1-baseline.summary.txt", "PASS"),
        ("q1-regression", "q1-baseline.summary.txt", "q1-regressed.summary.txt", "REGRESSION"),
        ("q1-incomparable", "q1-baseline.summary.txt", "q1-incomparable.summary.txt", "INCOMPARABLE"),
        ("q1-invalid", "q1-baseline.summary.txt", "q1-missing-identity.summary.txt", "INVALID"),
    )
    passed = all(run_case(fixture_root, *case) for case in cases)
    print(
        f"[PERF_COMPARE_VERIFY] status={'PASS' if passed else 'FAIL'} "
        f"cases={len(cases)}"
    )
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
