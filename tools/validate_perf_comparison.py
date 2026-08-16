#!/usr/bin/env python3
"""Runs the portable R1/Q1 comparator fixtures."""

from __future__ import annotations

import pathlib
import sys
from copy import deepcopy
from typing import Any

from compare_perf_baselines import (
    InvalidSummary,
    allowed_delta,
    compare,
    load_contract,
    read_summary,
)


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


def run_values_case(
    name: str,
    baseline: dict[str, str],
    candidate: dict[str, str],
    expected: str,
    contract: dict[str, Any],
) -> bool:
    try:
        result = compare(baseline, candidate, contract=contract)
        actual = result.status
    except InvalidSummary:
        actual = "INVALID"
    passed = actual == expected
    if not passed:
        print(
            f"[PERF_CONTRACT_VERIFY] case={name} expected={expected} "
            f"actual={actual} status=FAIL"
        )
    return passed


def changed_text(value: str) -> str:
    return f"{value}-changed"


def verify_contract_scenes(
    fixture_root: pathlib.Path, contract: dict[str, Any]
) -> tuple[bool, int]:
    scenes = contract.get("scenes")
    if not isinstance(scenes, dict):
        print("[PERF_CONTRACT_VERIFY] invalid=contract scenes are missing")
        return False, 0

    passed = True
    case_count = 0
    failure_count = 0

    def check(
        name: str,
        baseline: dict[str, str],
        candidate: dict[str, str],
        expected: str,
    ) -> None:
        nonlocal passed, case_count, failure_count
        case_count += 1
        case_passed = run_values_case(
            name, baseline, candidate, expected, contract
        )
        if not case_passed:
            failure_count += 1
        passed = case_passed and passed

    common_required = tuple(contract.get("common_required_keys", []))
    common_identity = tuple(contract.get("common_identity_keys", []))
    residency_policy = contract.get("residency_policy", {})
    residency_percent = float(residency_policy.get("percent", 0.0))
    residency_floors = residency_policy.get("metrics", {})
    frame_policy = contract.get("frame_policy", {})

    for scene_id, raw_scene in scenes.items():
        scene_case_start = case_count
        scene_failure_start = failure_count
        if not isinstance(raw_scene, dict):
            print(f"[PERF_CONTRACT_VERIFY] invalid=scene {scene_id} is not an object")
            return False, case_count
        path = fixture_root / f"{scene_id}.baseline.summary.txt"
        try:
            baseline = read_summary(path)
        except (InvalidSummary, OSError) as error:
            print(f"[PERF_CONTRACT_VERIFY] invalid={error}")
            return False, case_count

        prefix = scene_id.removeprefix("q1-").removesuffix("-v1")
        check(f"{prefix}/pass", baseline, deepcopy(baseline), "PASS")

        candidate = deepcopy(baseline)
        candidate["comparison_build_id"] = "fixture-build-candidate"
        check(f"{prefix}/different-build-record", baseline, candidate, "PASS")

        candidate = deepcopy(baseline)
        candidate["comparison_scene_id"] = "q1-different-scene-v1"
        check(f"{prefix}/scene-incomparable", baseline, candidate, "INCOMPARABLE")

        required_keys = common_required + tuple(raw_scene.get("identity_keys", []))
        for key in required_keys:
            candidate = deepcopy(baseline)
            candidate.pop(key, None)
            check(f"{prefix}/missing-{key}", baseline, candidate, "INVALID")

        identity_keys = common_identity + tuple(raw_scene.get("identity_keys", []))
        for key in identity_keys:
            candidate = deepcopy(baseline)
            candidate[key] = changed_text(candidate[key])
            check(
                f"{prefix}/incomparable-{key}",
                baseline,
                candidate,
                "INCOMPARABLE",
            )

        for key in raw_scene.get("required_metrics", []):
            candidate = deepcopy(baseline)
            candidate.pop(key, None)
            check(f"{prefix}/missing-{key}", baseline, candidate, "INVALID")

        for key in raw_scene.get("positive_metrics", []):
            candidate = deepcopy(baseline)
            candidate[key] = "0"
            check(f"{prefix}/nonpositive-{key}", baseline, candidate, "INVALID")

        for key in raw_scene.get("integer_metrics", []):
            candidate = deepcopy(baseline)
            candidate[key] = str(float(candidate[key]) + 0.5)
            check(f"{prefix}/fractional-{key}", baseline, candidate, "INVALID")

        for group_index, group in enumerate(raw_scene.get("monotonic_groups", [])):
            candidate = deepcopy(baseline)
            candidate[group[0]] = str(float(candidate[group[1]]) + 1.0)
            check(
                f"{prefix}/nonmonotonic-group-{group_index}",
                baseline,
                candidate,
                "INVALID",
            )

        for key, metric_range in raw_scene.get("metric_ranges", {}).items():
            candidate = deepcopy(baseline)
            candidate[key] = str(float(metric_range["maximum"]) + 1.0)
            check(f"{prefix}/out-of-range-{key}", baseline, candidate, "INVALID")

        for key in raw_scene.get("budget_metrics", []):
            candidate = deepcopy(baseline)
            candidate[key] = str(float(baseline[f"budget_{key}_max"]) + 1.0)
            check(f"{prefix}/budget-{key}", baseline, candidate, "REGRESSION")

            invalid_baseline = deepcopy(baseline)
            invalid_baseline.pop(f"budget_{key}_max", None)
            check(
                f"{prefix}/missing-budget-{key}",
                invalid_baseline,
                candidate,
                "INVALID",
            )

        for key in raw_scene.get("exact_metrics", []):
            candidate = deepcopy(baseline)
            candidate[key] = str(float(candidate[key]) + 1.0)
            check(f"{prefix}/exact-{key}", baseline, candidate, "INCOMPARABLE")

        for key in raw_scene.get("residency_metrics", []):
            candidate = deepcopy(baseline)
            baseline_value = float(baseline[key])
            minimum = float(residency_floors[key])
            candidate[key] = str(
                baseline_value
                + allowed_delta(baseline_value, residency_percent, minimum)
                + 1.0
            )
            check(f"{prefix}/residency-{key}", baseline, candidate, "INCOMPARABLE")

        if raw_scene.get("apply_frame_policy", False):
            for key, policy_key in (
                ("frame_p95_ms", "p95_percent"),
                ("frame_p99_ms", "p99_percent"),
            ):
                candidate = deepcopy(baseline)
                candidate[key] = str(
                    float(candidate[key])
                    * (1.0 + float(frame_policy[policy_key]) / 100.0)
                    + 0.001
                )
                check(f"{prefix}/frame-{key}", baseline, candidate, "REGRESSION")

            candidate = deepcopy(baseline)
            allowed = allowed_delta(
                float(baseline["frames"]),
                float(frame_policy["long_frame_percent"]),
                float(frame_policy["long_frame_minimum"]),
            )
            candidate["frames_over_50ms"] = str(
                float(baseline["frames_over_50ms"]) + allowed + 1.0
            )
            check(f"{prefix}/long-frames", baseline, candidate, "REGRESSION")

        scene_failures = failure_count - scene_failure_start
        print(
            f"[PERF_CONTRACT_VERIFY] scene={scene_id} "
            f"cases={case_count - scene_case_start} "
            f"status={'PASS' if scene_failures == 0 else 'FAIL'}"
        )

    unknown = read_summary(
        fixture_root / "q1-cold-start-v1.baseline.summary.txt"
    )
    unknown["comparison_scene_id"] = "q1-unknown-v1"
    check("contract/unknown-scene", unknown, deepcopy(unknown), "INVALID")
    return passed, case_count


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
    legacy_passed = all(run_case(fixture_root, *case) for case in cases)
    try:
        contract = load_contract()
        contract_passed, contract_cases = verify_contract_scenes(
            fixture_root, contract
        )
    except (InvalidSummary, OSError) as error:
        print(f"[PERF_CONTRACT_VERIFY] invalid={error}")
        contract_passed, contract_cases = False, 0
    passed = legacy_passed and contract_passed
    print(
        f"[PERF_COMPARE_VERIFY] status={'PASS' if passed else 'FAIL'} "
        f"legacy_cases={len(cases)} contract_cases={contract_cases}"
    )
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
