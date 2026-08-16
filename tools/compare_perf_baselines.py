#!/usr/bin/env python3
"""Cross-platform R1/Q1 performance-summary comparator."""

from __future__ import annotations

import argparse
import math
import pathlib
import sys
from dataclasses import dataclass


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


def compare(
    baseline: dict[str, str],
    candidate: dict[str, str],
    frame_p95_percent: float = 15.0,
    frame_p99_percent: float = 20.0,
    residency_percent: float = 5.0,
    long_frame_percent: float = 0.5,
) -> ComparisonResult:
    percentages = (
        frame_p95_percent,
        frame_p99_percent,
        residency_percent,
        long_frame_percent,
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
    if schema not in {"1", "2"}:
        raise InvalidSummary(f"unsupported comparison_schema={schema}")

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
    parser.add_argument("--frame-p95-percent", type=float, default=15.0)
    parser.add_argument("--frame-p99-percent", type=float, default=20.0)
    parser.add_argument("--residency-percent", type=float, default=5.0)
    parser.add_argument("--long-frame-percent", type=float, default=0.5)
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
