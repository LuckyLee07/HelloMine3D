#!/usr/bin/env python3
"""Cross-platform R1/Q1 performance-summary comparator."""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import sys
from dataclasses import dataclass
from typing import Any, Optional


DEFAULT_CONTRACT_PATH = pathlib.Path(__file__).with_name(
    "performance-contract-v1.json"
)


class InvalidSummary(RuntimeError):
    pass


@dataclass(frozen=True)
class ComparisonResult:
    status: str
    reasons: tuple[str, ...]

    @property
    def exit_code(self) -> int:
        return {"PASS": 0, "REGRESSION": 2, "INCOMPARABLE": 3}.get(
            self.status, 4
        )


def resolve_summary(path_text: str) -> pathlib.Path:
    path = pathlib.Path(path_text).expanduser().resolve()
    if path.is_dir():
        path = path / "summary.txt"
    if not path.is_file():
        raise InvalidSummary(f"summary does not exist: {path}")
    return path


def read_summary(path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), 1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line or line.startswith("="):
            raise InvalidSummary(f"malformed line {line_number} in {path}")
        key, value = (part.strip() for part in line.split("=", 1))
        if not key or not value:
            raise InvalidSummary(f"empty key/value at line {line_number} in {path}")
        if key in values:
            raise InvalidSummary(f"duplicate key '{key}' in {path}")
        values[key] = value
    return values


def require_text(summary: dict[str, str], key: str, label: str) -> str:
    value = summary.get(key, "").strip()
    if not value:
        raise InvalidSummary(f"{label} summary is missing '{key}'")
    return value


def require_number(summary: dict[str, str], key: str, label: str) -> float:
    text = require_text(summary, key, label)
    try:
        value = float(text)
    except ValueError as error:
        raise InvalidSummary(
            f"{label} summary has invalid numeric '{key}={text}'"
        ) from error
    if not math.isfinite(value) or value < 0.0:
        raise InvalidSummary(
            f"{label} summary has invalid numeric '{key}={text}'"
        )
    return value


def allowed_delta(value: float, percent: float, minimum: float) -> float:
    return max(minimum, math.ceil(abs(value) * percent / 100.0))


def load_contract(path: pathlib.Path = DEFAULT_CONTRACT_PATH) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise InvalidSummary(f"cannot load performance contract {path}: {error}") from error
    if not isinstance(value, dict):
        raise InvalidSummary(f"performance contract root must be an object: {path}")
    return value


def require_contract_list(
    owner: dict[str, Any], key: str, label: str
) -> tuple[str, ...]:
    values = owner.get(key, [])
    if not isinstance(values, list) or any(
        not isinstance(value, str) or not value for value in values
    ):
        raise InvalidSummary(f"performance contract has invalid {label}.{key}")
    if len(values) != len(set(values)):
        raise InvalidSummary(f"performance contract has duplicate {label}.{key}")
    return tuple(values)


def require_contract_number(
    owner: dict[str, Any], key: str, label: str
) -> float:
    value = owner.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise InvalidSummary(f"performance contract has invalid {label}.{key}")
    number = float(value)
    if not math.isfinite(number) or number < 0.0:
        raise InvalidSummary(f"performance contract has invalid {label}.{key}")
    return number


def validate_contract_metrics(
    summary: dict[str, str],
    label: str,
    scene: dict[str, Any],
) -> dict[str, float]:
    required_metrics = require_contract_list(scene, "required_metrics", "scene")
    values = {
        key: require_number(summary, key, label) for key in required_metrics
    }

    for key in require_contract_list(scene, "positive_metrics", "scene"):
        if key not in values:
            raise InvalidSummary(
                f"performance contract positive metric '{key}' is not required"
            )
        if values[key] <= 0.0:
            raise InvalidSummary(f"{label} summary requires positive '{key}'")

    for key in require_contract_list(scene, "integer_metrics", "scene"):
        if key not in values:
            raise InvalidSummary(
                f"performance contract integer metric '{key}' is not required"
            )
        if not values[key].is_integer():
            raise InvalidSummary(f"{label} summary requires integer '{key}'")

    groups = scene.get("monotonic_groups", [])
    if not isinstance(groups, list):
        raise InvalidSummary("performance contract has invalid scene.monotonic_groups")
    for group_index, raw_group in enumerate(groups):
        if not isinstance(raw_group, list) or len(raw_group) < 2 or any(
            not isinstance(key, str) or key not in values for key in raw_group
        ):
            raise InvalidSummary(
                "performance contract has invalid "
                f"scene.monotonic_groups[{group_index}]"
            )
        for earlier, later in zip(raw_group, raw_group[1:]):
            if values[later] < values[earlier]:
                raise InvalidSummary(
                    f"{label} summary has non-monotonic phase metrics "
                    f"'{earlier}={values[earlier]:g}' then "
                    f"'{later}={values[later]:g}'"
                )

    ranges = scene.get("metric_ranges", {})
    if not isinstance(ranges, dict):
        raise InvalidSummary("performance contract has invalid scene.metric_ranges")
    for key, raw_range in ranges.items():
        if key not in values or not isinstance(raw_range, dict):
            raise InvalidSummary(
                f"performance contract has invalid metric range '{key}'"
            )
        minimum = require_contract_number(raw_range, "minimum", f"range.{key}")
        maximum = require_contract_number(raw_range, "maximum", f"range.{key}")
        if maximum < minimum:
            raise InvalidSummary(
                f"performance contract range '{key}' has maximum below minimum"
            )
        if not minimum <= values[key] <= maximum:
            raise InvalidSummary(
                f"{label} summary metric '{key}={values[key]:g}' is outside "
                f"[{minimum:g}, {maximum:g}]"
            )

    return values


def compare_contract_scene(
    baseline: dict[str, str],
    candidate: dict[str, str],
    contract: dict[str, Any],
    frame_p95_percent: Optional[float],
    frame_p99_percent: Optional[float],
    residency_percent: Optional[float],
    long_frame_percent: Optional[float],
) -> ComparisonResult:
    contract_schema = str(contract.get("comparison_schema", ""))
    if contract_schema != "3":
        raise InvalidSummary(
            f"performance contract has unsupported comparison_schema={contract_schema}"
        )
    contract_version = contract.get("contract_version")
    if isinstance(contract_version, bool) or not isinstance(contract_version, int):
        raise InvalidSummary("performance contract has invalid contract_version")

    baseline_scene_id = require_text(
        baseline, "comparison_scene_id", "Baseline"
    )
    candidate_scene_id = require_text(
        candidate, "comparison_scene_id", "Candidate"
    )
    if baseline_scene_id != candidate_scene_id:
        return ComparisonResult(
            "INCOMPARABLE",
            (
                "comparison_scene_id "
                f"baseline='{baseline_scene_id}' candidate='{candidate_scene_id}'",
            ),
        )

    scenes = contract.get("scenes")
    if not isinstance(scenes, dict) or baseline_scene_id not in scenes:
        raise InvalidSummary(
            f"performance contract does not define scene '{baseline_scene_id}'"
        )
    scene = scenes[baseline_scene_id]
    if not isinstance(scene, dict):
        raise InvalidSummary(
            f"performance contract scene '{baseline_scene_id}' is invalid"
        )

    common_required = require_contract_list(
        contract, "common_required_keys", "contract"
    )
    common_identity = require_contract_list(
        contract, "common_identity_keys", "contract"
    )
    scene_identity = require_contract_list(scene, "identity_keys", "scene")
    for key in common_required + scene_identity:
        require_text(baseline, key, "Baseline")
        require_text(candidate, key, "Candidate")

    expected_version = str(contract_version)
    for label, summary in (("Baseline", baseline), ("Candidate", candidate)):
        actual_version = require_text(
            summary, "comparison_contract_version", label
        )
        if actual_version != expected_version:
            raise InvalidSummary(
                f"{label} summary has comparison_contract_version="
                f"{actual_version}, expected {expected_version}"
            )

    baseline_metrics = validate_contract_metrics(baseline, "Baseline", scene)
    candidate_metrics = validate_contract_metrics(candidate, "Candidate", scene)

    budget_metrics = require_contract_list(scene, "budget_metrics", "scene")
    budget_limits: dict[str, float] = {}
    for key in budget_metrics:
        if key not in baseline_metrics:
            raise InvalidSummary(
                f"performance contract budget metric '{key}' is not required"
            )
        limit_key = f"budget_{key}_max"
        limit = require_number(baseline, limit_key, "Baseline")
        if baseline_metrics[key] > limit:
            raise InvalidSummary(
                f"Baseline summary metric '{key}={baseline_metrics[key]:g}' "
                f"exceeds its approved '{limit_key}={limit:g}'"
            )
        budget_limits[key] = limit

    incomparable: list[str] = []
    for key in common_identity + scene_identity:
        baseline_value = baseline[key].strip()
        candidate_value = candidate[key].strip()
        if baseline_value != candidate_value:
            incomparable.append(
                f"{key} baseline='{baseline_value}' candidate='{candidate_value}'"
            )

    for key in require_contract_list(scene, "exact_metrics", "scene"):
        if key not in baseline_metrics:
            raise InvalidSummary(
                f"performance contract exact metric '{key}' is not required"
            )
        if baseline_metrics[key] != candidate_metrics[key]:
            incomparable.append(
                f"{key} baseline={baseline_metrics[key]:g} "
                f"candidate={candidate_metrics[key]:g}"
            )

    residency_policy = contract.get("residency_policy")
    if not isinstance(residency_policy, dict):
        raise InvalidSummary("performance contract has invalid residency_policy")
    default_residency_percent = require_contract_number(
        residency_policy, "percent", "residency_policy"
    )
    selected_residency_percent = (
        default_residency_percent
        if residency_percent is None
        else residency_percent
    )
    residency_floors = residency_policy.get("metrics")
    if not isinstance(residency_floors, dict):
        raise InvalidSummary(
            "performance contract has invalid residency_policy.metrics"
        )
    for key in require_contract_list(scene, "residency_metrics", "scene"):
        if key not in baseline_metrics or key not in residency_floors:
            raise InvalidSummary(
                f"performance contract has invalid residency metric '{key}'"
            )
        minimum = require_contract_number(
            residency_floors, key, "residency_policy.metrics"
        )
        allowed = allowed_delta(
            baseline_metrics[key], selected_residency_percent, minimum
        )
        delta = abs(candidate_metrics[key] - baseline_metrics[key])
        if delta > allowed:
            incomparable.append(
                f"{key} delta={delta:g} allowed={allowed:g} "
                f"baseline={baseline_metrics[key]:g} "
                f"candidate={candidate_metrics[key]:g}"
            )

    if incomparable:
        return ComparisonResult("INCOMPARABLE", tuple(incomparable))

    regressions: list[str] = []
    for key, limit in budget_limits.items():
        if candidate_metrics[key] > limit:
            regressions.append(
                f"{key} candidate={candidate_metrics[key]:g} "
                f"approved_limit={limit:g}"
            )

    if bool(scene.get("apply_frame_policy", False)):
        frame_policy = contract.get("frame_policy")
        if not isinstance(frame_policy, dict):
            raise InvalidSummary("performance contract has invalid frame_policy")
        selected_p95 = (
            require_contract_number(frame_policy, "p95_percent", "frame_policy")
            if frame_p95_percent is None
            else frame_p95_percent
        )
        selected_p99 = (
            require_contract_number(frame_policy, "p99_percent", "frame_policy")
            if frame_p99_percent is None
            else frame_p99_percent
        )
        selected_long = (
            require_contract_number(
                frame_policy, "long_frame_percent", "frame_policy"
            )
            if long_frame_percent is None
            else long_frame_percent
        )
        long_minimum = require_contract_number(
            frame_policy, "long_frame_minimum", "frame_policy"
        )
        for key, percent in (
            ("frame_p95_ms", selected_p95),
            ("frame_p99_ms", selected_p99),
        ):
            limit = baseline_metrics[key] * (1.0 + percent / 100.0)
            if candidate_metrics[key] > limit:
                regressions.append(
                    f"{key} candidate={candidate_metrics[key]:g} "
                    f"limit={limit:.3f}"
                )
        allowed_increase = allowed_delta(
            baseline_metrics["frames"], selected_long, long_minimum
        )
        increase = (
            candidate_metrics["frames_over_50ms"]
            - baseline_metrics["frames_over_50ms"]
        )
        if increase > allowed_increase:
            regressions.append(
                f"frames_over_50ms increase={increase:g} "
                f"allowed={allowed_increase:g}"
            )

    if regressions:
        return ComparisonResult("REGRESSION", tuple(regressions))
    return ComparisonResult("PASS", ())


def compare(
    baseline: dict[str, str],
    candidate: dict[str, str],
    frame_p95_percent: Optional[float] = None,
    frame_p99_percent: Optional[float] = None,
    residency_percent: Optional[float] = None,
    long_frame_percent: Optional[float] = None,
    contract: Optional[dict[str, Any]] = None,
) -> ComparisonResult:
    percentages = tuple(
        value
        for value in (
            frame_p95_percent,
            frame_p99_percent,
            residency_percent,
            long_frame_percent,
        )
        if value is not None
    )
    if any(value < 0.0 or not math.isfinite(value) for value in percentages):
        raise InvalidSummary("comparison percentages must be finite and non-negative")

    schema = require_text(baseline, "comparison_schema", "Baseline")
    candidate_schema = require_text(candidate, "comparison_schema", "Candidate")
    if schema != candidate_schema:
        return ComparisonResult(
            "INCOMPARABLE",
            (f"comparison_schema baseline='{schema}' candidate='{candidate_schema}'",),
        )
    if schema == "3":
        return compare_contract_scene(
            baseline,
            candidate,
            load_contract() if contract is None else contract,
            frame_p95_percent,
            frame_p99_percent,
            residency_percent,
            long_frame_percent,
        )
    if schema not in {"1", "2"}:
        raise InvalidSummary(f"unsupported comparison_schema={schema}")

    frame_p95_percent = 15.0 if frame_p95_percent is None else frame_p95_percent
    frame_p99_percent = 20.0 if frame_p99_percent is None else frame_p99_percent
    residency_percent = 5.0 if residency_percent is None else residency_percent
    long_frame_percent = 0.5 if long_frame_percent is None else long_frame_percent

    identity_keys = [
        "build_configuration",
        "comparison_scene_id",
        "comparison_vsync_regime",
        "comparison_window",
        "terrain_seed",
    ]
    if schema == "2":
        identity_keys.extend(
            [
                "comparison_platform",
                "comparison_architecture",
                "comparison_build_id",
                "comparison_gpu",
                "comparison_driver",
                "comparison_fullscreen",
                "comparison_fov",
                "comparison_resource_manifest_sha256",
                "comparison_resource_packs",
                "comparison_world_fixture",
                "comparison_save_format",
                "comparison_storage_class",
                "comparison_render_distance",
            ]
        )

    incomparable: list[str] = []
    for key in identity_keys:
        baseline_value = require_text(baseline, key, "Baseline")
        candidate_value = require_text(candidate, key, "Candidate")
        if baseline_value != candidate_value:
            incomparable.append(
                f"{key} baseline='{baseline_value}' candidate='{candidate_value}'"
            )

    for key, minimum in (("last_loaded_chunks", 2.0), ("last_sections", 8.0)):
        baseline_value = require_number(baseline, key, "Baseline")
        candidate_value = require_number(candidate, key, "Candidate")
        allowed = allowed_delta(baseline_value, residency_percent, minimum)
        delta = abs(candidate_value - baseline_value)
        if delta > allowed:
            incomparable.append(
                f"{key} delta={delta:g} allowed={allowed:g} "
                f"baseline={baseline_value:g} candidate={candidate_value:g}"
            )
    if incomparable:
        return ComparisonResult("INCOMPARABLE", tuple(incomparable))

    regressions: list[str] = []
    for key, percent in (
        ("frame_p95_ms", frame_p95_percent),
        ("frame_p99_ms", frame_p99_percent),
    ):
        baseline_value = require_number(baseline, key, "Baseline")
        candidate_value = require_number(candidate, key, "Candidate")
        limit = baseline_value * (1.0 + percent / 100.0)
        if candidate_value > limit:
            regressions.append(
                f"{key} candidate={candidate_value:g} limit={limit:.3f}"
            )

    baseline_frames = require_number(baseline, "frames", "Baseline")
    candidate_frames = require_number(candidate, "frames", "Candidate")
    if baseline_frames <= 0.0 or candidate_frames <= 0.0:
        raise InvalidSummary("both summaries must contain a positive frame count")
    baseline_long = require_number(baseline, "frames_over_50ms", "Baseline")
    candidate_long = require_number(candidate, "frames_over_50ms", "Candidate")
    allowed_increase = allowed_delta(
        baseline_frames, long_frame_percent, 2.0
    )
    increase = candidate_long - baseline_long
    if increase > allowed_increase:
        regressions.append(
            f"frames_over_50ms increase={increase:g} allowed={allowed_increase:g}"
        )

    if regressions:
        return ComparisonResult("REGRESSION", tuple(regressions))
    return ComparisonResult("PASS", ())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--contract", default=str(DEFAULT_CONTRACT_PATH))
    parser.add_argument("--frame-p95-percent", type=float)
    parser.add_argument("--frame-p99-percent", type=float)
    parser.add_argument("--residency-percent", type=float)
    parser.add_argument("--long-frame-percent", type=float)
    args = parser.parse_args()

    try:
        baseline_path = resolve_summary(args.baseline)
        candidate_path = resolve_summary(args.candidate)
        result = compare(
            read_summary(baseline_path),
            read_summary(candidate_path),
            args.frame_p95_percent,
            args.frame_p99_percent,
            args.residency_percent,
            args.long_frame_percent,
            load_contract(pathlib.Path(args.contract).expanduser().resolve()),
        )
        for reason in result.reasons:
            print(f"[PERF_COMPARE] {result.status.lower()}={reason}")
        print(
            f"[PERF_COMPARE] status={result.status} "
            f"baseline={baseline_path} candidate={candidate_path}"
        )
        return result.exit_code
    except (InvalidSummary, OSError) as error:
        print(f"[PERF_COMPARE] invalid={error}")
        print("[PERF_COMPARE] status=INVALID")
        return 4


if __name__ == "__main__":
    sys.exit(main())
