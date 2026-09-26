#!/usr/bin/env python3
"""Validate and compare the frozen B7 v22/v23 headless generation captures."""

import argparse
import csv
import hashlib
import io
import json
import math
import statistics
import sys
from pathlib import Path


CONFIGURATIONS = ("Debug", "Release")
SEEDS = (42, 20260807, 239701883)
VERSIONS = (22, 23)
ROUND_ORDER = ((22, 23), (23, 22), (22, 23))
GUARDRAIL = 1.10
GEOMETRY_BUDGETS = {
    "changed_blocks": 12000,
    "changed_to_air": 9000,
    "changed_to_water": 384,
    "changed_to_structure": 1500,
}
NEW_VISIBLE_FACE_LIMIT = 72000

IDENTITY_FIELDS = (
    "schema", "configuration", "seed", "terrain_version", "round",
    "clock", "peak_rss_unit", "executable_sha256", "warmup_chunk_x",
    "warmup_chunk_z",
)
CHUNK_FIELDS = (
    "seed", "terrain_version", "round", "chunk_order", "chunk_x",
    "chunk_z", "generate_ns", "block_hash", "section_count",
    "block_entity_count",
)
RUN_FIELDS = (
    "seed", "terrain_version", "round", "chunk_count",
    "generation_sum_ns", "region_wall_ns", "peak_rss_before_bytes",
    "peak_rss_after_bytes", "started_unix_ns", "ended_unix_ns",
)
GEOMETRY_IDENTITY_FIELDS = (
    "schema", "configuration", "baseline_version", "candidate_version",
    "executable_sha256", "vertex_stride_bytes", "index_stride_bytes",
)
GEOMETRY_FIELDS = (
    "seed", "target_chunks", "changed_chunks", "changed_blocks",
    "changed_to_air", "changed_to_water", "changed_to_structure",
    "changed_sections", "block_entity_delta", "halo_chunks",
    "v22_sections", "v23_sections", "v22_emitting_sections",
    "v23_emitting_sections", "v22_solid_faces", "v23_solid_faces",
    "v22_water_faces", "v23_water_faces", "v22_transparent_faces",
    "v23_transparent_faces", "v22_flora_faces", "v23_flora_faces",
    "v22_vertices", "v23_vertices", "v22_indices", "v23_indices",
    "v22_renderables", "v23_renderables", "v22_buffer_bytes",
    "v23_buffer_bytes",
)
GEOMETRY_CHUNK_FIELDS = (
    "seed", "chunk_order", "chunk_x", "chunk_z", "changed_blocks",
    "changed_to_air", "changed_to_water", "changed_to_structure",
    "v22_block_hash", "v23_block_hash",
)
CAPTURE_ORDER_FIELDS = (
    "sequence", "configuration", "seed", "round", "terrain_version",
    "executable_sha256",
)
ENVIRONMENT_FIELDS = (
    "schema", "source_fingerprint", "platform_system", "platform_release",
    "platform_machine", "host_architecture", "compiler_path",
    "compiler_version", "compiler_default_target", "premake_path",
    "premake_version", "make_path", "make_version",
    "configured_architecture", "debug_binary_architecture",
    "release_binary_architecture", "debug_binary_sha256",
    "release_binary_sha256",
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_integer(value: str, label: str) -> int:
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise ValueError(f"{label}: expected integer, got {value!r}") from error
    if str(parsed) != value:
        raise ValueError(f"{label}: integer is not canonical: {value!r}")
    return parsed


def exact_rows(path: Path, fields, expected_count=None):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != tuple(fields):
            raise ValueError(
                f"{path}: CSV schema changed: {reader.fieldnames!r}")
        rows = list(reader)
    if expected_count is not None and len(rows) != expected_count:
        raise ValueError(
            f"{path}: expected {expected_count} rows, found {len(rows)}")
    if any(None in row or any(value is None for value in row.values())
           for row in rows):
        raise ValueError(f"{path}: malformed CSV row")
    return rows


def nearest_rank(values, fraction):
    if not values:
        raise ValueError("Cannot calculate a percentile of no samples")
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]


def percentile_sample_shape(sample_count):
    if sample_count <= 0:
        raise ValueError("Percentile sample count must be positive")
    p95_rank = math.ceil(sample_count * .95)
    p99_rank = math.ceil(sample_count * .99)
    return {
        "samples_per_run": sample_count,
        "p95_rank_one_based": p95_rank,
        "p99_rank_one_based": p99_rank,
        "p95_is_run_max": p95_rank == sample_count,
        "p99_is_run_max": p99_rank == sample_count,
    }


def expected_capture_order():
    result = []
    sequence = 0
    for configuration in CONFIGURATIONS:
        for seed in SEEDS:
            for round_number, versions in enumerate(ROUND_ORDER, 1):
                for version in versions:
                    sequence += 1
                    result.append((sequence, configuration, seed,
                                   round_number, version))
    return result


def geometry_budget_evaluation(values):
    measured = {key: values[key] for key in GEOMETRY_BUDGETS}
    positive_face_deltas = {
        category: max(0, values[f"v23_{category}_faces"] -
                      values[f"v22_{category}_faces"])
        for category in ("solid", "water", "transparent", "flora")
    }
    measured["new_visible_faces"] = sum(positive_face_deltas.values())
    checks = {
        key: measured[key] <= limit
        for key, limit in GEOMETRY_BUDGETS.items()
    }
    checks["new_visible_faces"] = \
        measured["new_visible_faces"] < NEW_VISIBLE_FACE_LIMIT
    limits = dict(GEOMETRY_BUDGETS)
    limits["new_visible_faces_strictly_less_than"] = NEW_VISIBLE_FACE_LIMIT
    return {
        "status": "PASS" if all(checks.values()) else "FAIL",
        "measured": measured,
        "limits": limits,
        "checks": checks,
        "positive_face_deltas": positive_face_deltas,
        "visible_face_formula":
            "sum(max(0, v23_category_faces - v22_category_faces)) "
            "for solid, water, transparent, flora",
    }


def require_sha256(value: str, label: str) -> str:
    normalized = value.lower()
    if len(normalized) != 64 or any(
            character not in "0123456789abcdef" for character in normalized):
        raise ValueError(f"{label}: invalid SHA-256")
    return normalized


def read_capture_order(root: Path, binary_hashes):
    path = root / "capture-order.csv"
    expected = expected_capture_order()
    rows = exact_rows(path, CAPTURE_ORDER_FIELDS, len(expected))
    normalized = []
    for row, expected_row in zip(rows, expected):
        sequence = canonical_integer(row["sequence"], f"{path}:sequence")
        seed = canonical_integer(row["seed"], f"{path}:seed")
        round_number = canonical_integer(row["round"], f"{path}:round")
        version = canonical_integer(
            row["terrain_version"], f"{path}:terrain_version")
        actual = (sequence, row["configuration"], seed,
                  round_number, version)
        if actual != expected_row:
            raise ValueError(
                f"{path}: frozen capture order changed at sequence "
                f"{expected_row[0]}: {actual!r} != {expected_row!r}")
        executable_hash = require_sha256(
            row["executable_sha256"], f"{path}:executable_sha256")
        if executable_hash != binary_hashes[row["configuration"]]:
            raise ValueError(
                f"{path}: sequence {sequence} binary identity mismatch")
        normalized.append({
            "sequence": sequence,
            "configuration": row["configuration"],
            "seed": seed,
            "round": round_number,
            "terrain_version": version,
            "executable_sha256": executable_hash,
        })
    return {
        "status": "PASS",
        "sha256": digest(path),
        "count": len(normalized),
        "rows": normalized,
    }


def read_environment_identity(root: Path, source_fingerprint: str,
                              binary_hashes):
    path = root / "environment.csv"
    row = exact_rows(path, ENVIRONMENT_FIELDS, 1)[0]
    if row["schema"] != "1":
        raise ValueError(f"{path}: unsupported environment schema")
    if require_sha256(row["source_fingerprint"], str(path)) != \
            source_fingerprint:
        raise ValueError(f"{path}: source fingerprint mismatch")
    required_text_fields = (
        "platform_system", "platform_release", "platform_machine",
        "host_architecture", "compiler_path", "compiler_version",
        "compiler_default_target", "premake_path", "premake_version",
        "make_path", "make_version",
    )
    for field in required_text_fields:
        if not row[field].strip() or "\n" in row[field] or "\r" in row[field]:
            raise ValueError(f"{path}: invalid or empty {field}")
    for field in ("compiler_path", "premake_path", "make_path"):
        if not Path(row[field]).is_absolute():
            raise ValueError(f"{path}: {field} must be an absolute path")
    if row["host_architecture"] != row["platform_machine"]:
        raise ValueError(f"{path}: host architecture identity disagrees")
    if row["configured_architecture"] != "x86_64" or \
            row["debug_binary_architecture"] != "x86_64" or \
            row["release_binary_architecture"] != "x86_64":
        raise ValueError(f"{path}: frozen x86_64 architecture identity changed")
    recorded_hashes = {
        "Debug": require_sha256(
            row["debug_binary_sha256"], f"{path}:debug_binary_sha256"),
        "Release": require_sha256(
            row["release_binary_sha256"], f"{path}:release_binary_sha256"),
    }
    if recorded_hashes != binary_hashes:
        raise ValueError(f"{path}: binary hashes do not match frozen binaries")
    return {
        "status": "PASS",
        "sha256": digest(path),
        "platform": {
            "system": row["platform_system"],
            "release": row["platform_release"],
            "machine": row["platform_machine"],
            "host_architecture": row["host_architecture"],
        },
        "toolchain": {
            "compiler_path": row["compiler_path"],
            "compiler_version": row["compiler_version"],
            "compiler_default_target": row["compiler_default_target"],
            "premake_path": row["premake_path"],
            "premake_version": row["premake_version"],
            "make_path": row["make_path"],
            "make_version": row["make_version"],
        },
        "architecture": {
            "configured": row["configured_architecture"],
            "Debug": row["debug_binary_architecture"],
            "Release": row["release_binary_architecture"],
        },
        "binary_sha256": recorded_hashes,
    }


def read_capture(directory: Path, configuration: str, seed: int,
                 version: int, round_number: int, executable_hash: str):
    identity = exact_rows(
        directory / "identity.csv", IDENTITY_FIELDS, 1)[0]
    expected_identity = {
        "schema": "1",
        "configuration": configuration,
        "seed": str(seed),
        "terrain_version": str(version),
        "round": str(round_number),
        "clock": "steady_clock",
        "peak_rss_unit": "bytes",
        "warmup_chunk_x": "0",
        "warmup_chunk_z": "0",
    }
    for key, expected in expected_identity.items():
        if identity[key] != expected:
            raise ValueError(
                f"{directory}: identity {key}={identity[key]!r}, "
                f"expected {expected!r}")
    if require_sha256(identity["executable_sha256"], str(directory)) != \
            executable_hash:
        raise ValueError(f"{directory}: executable identity mismatch")

    run_row = exact_rows(directory / "run.csv", RUN_FIELDS, 1)[0]
    chunks = exact_rows(directory / "chunks.csv", CHUNK_FIELDS)
    expected_triplet = (seed, version, round_number)
    for label, row in (("run", run_row), *[("chunk", row) for row in chunks]):
        actual = tuple(canonical_integer(row[key], f"{directory}:{key}")
                       for key in ("seed", "terrain_version", "round"))
        if actual != expected_triplet:
            raise ValueError(
                f"{directory}: {label} identity is {actual}, "
                f"expected {expected_triplet}")

    chunk_count = canonical_integer(run_row["chunk_count"],
                                    f"{directory}:chunk_count")
    if chunk_count != len(chunks) or not 1 <= chunk_count <= 64:
        raise ValueError(f"{directory}: invalid chunk count")
    samples = []
    coordinates = []
    hashes = {}
    for index, row in enumerate(chunks):
        if canonical_integer(row["chunk_order"],
                             f"{directory}:chunk_order") != index:
            raise ValueError(f"{directory}: non-contiguous chunk order")
        coordinate = (
            canonical_integer(row["chunk_x"], f"{directory}:chunk_x"),
            canonical_integer(row["chunk_z"], f"{directory}:chunk_z"),
        )
        if coordinate in hashes:
            raise ValueError(f"{directory}: duplicate chunk coordinate")
        elapsed = canonical_integer(
            row["generate_ns"], f"{directory}:generate_ns")
        block_hash = canonical_integer(
            row["block_hash"], f"{directory}:block_hash")
        sections = canonical_integer(
            row["section_count"], f"{directory}:section_count")
        entities = canonical_integer(
            row["block_entity_count"],
            f"{directory}:block_entity_count")
        if elapsed <= 0 or block_hash < 0 or sections <= 0 or entities < 0:
            raise ValueError(f"{directory}: invalid chunk measurement")
        samples.append(elapsed)
        coordinates.append(coordinate)
        hashes[coordinate] = block_hash

    generation_sum = canonical_integer(
        run_row["generation_sum_ns"], f"{directory}:generation_sum_ns")
    region_wall = canonical_integer(
        run_row["region_wall_ns"], f"{directory}:region_wall_ns")
    peak_before = canonical_integer(
        run_row["peak_rss_before_bytes"],
        f"{directory}:peak_rss_before_bytes")
    peak_after = canonical_integer(
        run_row["peak_rss_after_bytes"],
        f"{directory}:peak_rss_after_bytes")
    started = canonical_integer(
        run_row["started_unix_ns"], f"{directory}:started_unix_ns")
    ended = canonical_integer(
        run_row["ended_unix_ns"], f"{directory}:ended_unix_ns")
    if generation_sum != sum(samples):
        raise ValueError(f"{directory}: generation sum does not match samples")
    if region_wall < generation_sum:
        raise ValueError(f"{directory}: region wall time is too small")
    if peak_before <= 0 or peak_after < peak_before:
        raise ValueError(f"{directory}: invalid process peak RSS")
    if started <= 0 or ended <= started:
        raise ValueError(f"{directory}: invalid capture timestamps")

    return {
        "path": str(directory),
        "configuration": configuration,
        "seed": seed,
        "version": version,
        "round": round_number,
        "coordinates": coordinates,
        "hashes": hashes,
        "samples_ns": samples,
        "chunk_p95_ns": nearest_rank(samples, .95),
        "chunk_p99_ns": nearest_rank(samples, .99),
        "generation_sum_ns": generation_sum,
        "region_wall_ns": region_wall,
        "peak_rss_before_bytes": peak_before,
        "peak_rss_after_bytes": peak_after,
        "started_unix_ns": started,
        "ended_unix_ns": ended,
        "identity_sha256": digest(directory / "identity.csv"),
        "chunks_sha256": digest(directory / "chunks.csv"),
        "run_sha256": digest(directory / "run.csv"),
    }


def metric_comparison(baseline, candidate):
    old = statistics.median(baseline)
    new = statistics.median(candidate)
    ratio = new / old if old > 0 else None
    return {
        "v22_runs_ns": baseline,
        "v23_runs_ns": candidate,
        "v22_median_ns": old,
        "v23_median_ns": new,
        "ratio": ratio,
        "status": "PASS" if ratio is not None and ratio <= GUARDRAIL
                  else "FAIL",
    }


def total_summary(runs_by_version, key):
    result = {}
    for version in VERSIONS:
        values = [run[key] for run in runs_by_version[version]]
        result[f"v{version}_runs_ns"] = values
        result[f"v{version}_median_ns"] = statistics.median(values)
        result[f"v{version}_p95_ns"] = nearest_rank(values, .95)
        result[f"v{version}_p99_ns"] = nearest_rank(values, .99)
    old = result["v22_median_ns"]
    result["median_ratio"] = result["v23_median_ns"] / old if old else None
    return result


def read_geometry(root: Path, release_hash: str):
    directory = root / "geometry" / "Release"
    identity = exact_rows(
        directory / "identity.csv", GEOMETRY_IDENTITY_FIELDS, 1)[0]
    expected = {
        "schema": "1",
        "configuration": "Release",
        "baseline_version": "22",
        "candidate_version": "23",
        "vertex_stride_bytes": "32",
        "index_stride_bytes": "4",
    }
    for key, value in expected.items():
        if identity[key] != value:
            raise ValueError(f"{directory}: geometry identity mismatch: {key}")
    if require_sha256(identity["executable_sha256"], str(directory)) != \
            release_hash:
        raise ValueError(f"{directory}: geometry executable mismatch")

    rows = exact_rows(directory / "geometry.csv", GEOMETRY_FIELDS, len(SEEDS))
    chunk_rows = exact_rows(
        directory / "geometry_chunks.csv", GEOMETRY_CHUNK_FIELDS)
    by_seed = {}
    budgets_by_seed = {}
    for expected_seed, row in zip(SEEDS, rows):
        seed = canonical_integer(row["seed"], f"{directory}:seed")
        if seed != expected_seed or seed in by_seed:
            raise ValueError(f"{directory}: geometry seed order changed")
        values = {key: canonical_integer(value, f"{directory}:{key}")
                  for key, value in row.items() if key != "seed"}
        if values["target_chunks"] <= 0 or \
                values["halo_chunks"] < values["target_chunks"] or \
                not 0 < values["changed_chunks"] <= values["target_chunks"] or \
                values["changed_blocks"] <= 0 or \
                values["changed_sections"] <= 0:
            raise ValueError(f"{directory}: incomplete geometry for seed {seed}")
        if values["changed_blocks"] != values["changed_to_air"] + \
                values["changed_to_water"] + \
                values["changed_to_structure"]:
            raise ValueError(f"{directory}: changed block categories disagree")
        for version in VERSIONS:
            if values[f"v{version}_sections"] <= 0 or \
                    values[f"v{version}_emitting_sections"] <= 0 or \
                    values[f"v{version}_vertices"] <= 0 or \
                    values[f"v{version}_indices"] <= 0:
                raise ValueError(f"{directory}: empty v{version} geometry")
            expected_bytes = values[f"v{version}_vertices"] * 32 + \
                values[f"v{version}_indices"] * 4
            if values[f"v{version}_buffer_bytes"] != expected_bytes:
                raise ValueError(f"{directory}: v{version} buffer mismatch")
        by_seed[seed] = values
        budgets_by_seed[seed] = geometry_budget_evaluation(values)

    grouped_chunks = {seed: [] for seed in SEEDS}
    for row in chunk_rows:
        seed = canonical_integer(row["seed"], f"{directory}:chunk-seed")
        if seed not in grouped_chunks:
            raise ValueError(f"{directory}: unknown geometry chunk seed")
        grouped_chunks[seed].append(row)
    for seed in SEEDS:
        rows_for_seed = grouped_chunks[seed]
        expected = by_seed[seed]
        if len(rows_for_seed) != expected["target_chunks"]:
            raise ValueError(f"{directory}: geometry chunk count mismatch")
        changed_blocks = 0
        changed_chunks = 0
        for index, row in enumerate(rows_for_seed):
            if canonical_integer(row["chunk_order"], "geometry:chunk_order") != index:
                raise ValueError(f"{directory}: geometry chunk order changed")
            changed = canonical_integer(row["changed_blocks"],
                                        "geometry:changed_blocks")
            categories = sum(canonical_integer(row[key], f"geometry:{key}")
                             for key in ("changed_to_air", "changed_to_water",
                                         "changed_to_structure"))
            if changed != categories:
                raise ValueError(f"{directory}: geometry chunk categories disagree")
            old_hash = canonical_integer(row["v22_block_hash"],
                                         "geometry:v22_block_hash")
            new_hash = canonical_integer(row["v23_block_hash"],
                                         "geometry:v23_block_hash")
            if (changed > 0) != (old_hash != new_hash):
                raise ValueError(f"{directory}: block hash/change mismatch")
            changed_blocks += changed
            changed_chunks += changed > 0
        if changed_blocks != expected["changed_blocks"] or \
                changed_chunks != expected["changed_chunks"]:
            raise ValueError(f"{directory}: geometry aggregate mismatch")

    budget_status = "PASS" if all(
        budget["status"] == "PASS"
        for budget in budgets_by_seed.values()) else "FAIL"
    return {
        "path": str(directory),
        "identity_sha256": digest(directory / "identity.csv"),
        "geometry_sha256": digest(directory / "geometry.csv"),
        "geometry_chunks_sha256": digest(directory / "geometry_chunks.csv"),
        "seeds": {str(seed): by_seed[seed] for seed in SEEDS},
        "contract_budgets": {
            "status": budget_status,
            "limits": {
                **GEOMETRY_BUDGETS,
                "new_visible_faces_strictly_less_than":
                    NEW_VISIBLE_FACE_LIMIT,
            },
            "seeds": {
                str(seed): budgets_by_seed[seed] for seed in SEEDS
            },
        },
    }


def compare(root: Path):
    if not root.is_dir():
        raise ValueError(f"Evidence root does not exist: {root}")
    before = require_sha256(
        (root / "source-fingerprint-before.txt").read_text().strip(),
        "source fingerprint before")
    after = require_sha256(
        (root / "source-fingerprint-after.txt").read_text().strip(),
        "source fingerprint after")
    if before != after:
        raise ValueError("Source changed while performance evidence was captured")

    binary_hashes = {}
    for configuration in CONFIGURATIONS:
        binary = root / "binaries" / (
            "HelloMine3DWorldRuntimeSmoke-" + configuration)
        recorded = require_sha256(
            (binary.with_suffix(binary.suffix + ".sha256")).read_text().strip(),
            f"{configuration} recorded binary hash")
        actual = digest(binary)
        if recorded != actual:
            raise ValueError(f"{configuration} frozen binary hash mismatch")
        binary_hashes[configuration] = actual
    if len(set(binary_hashes.values())) != len(CONFIGURATIONS):
        raise ValueError("Debug and Release binaries unexpectedly share one identity")

    environment = read_environment_identity(root, before, binary_hashes)
    capture_order = read_capture_order(root, binary_hashes)

    captures = {configuration: {seed: {22: [], 23: []}
                                for seed in SEEDS}
                for configuration in CONFIGURATIONS}
    chronological = []
    for configuration in CONFIGURATIONS:
        for seed in SEEDS:
            for round_number, order in enumerate(ROUND_ORDER, 1):
                for version in order:
                    directory = root / "runs" / configuration / \
                        f"seed-{seed}" / f"r{round_number}-v{version}"
                    capture = read_capture(
                        directory, configuration, seed, version,
                        round_number, binary_hashes[configuration])
                    captures[configuration][seed][version].append(capture)
                    chronological.append(capture)
    for left, right in zip(chronological, chronological[1:]):
        if right["started_unix_ns"] < left["ended_unix_ns"]:
            raise ValueError("Captures overlap or differ from the frozen order")

    results = {}
    overall_pass = True
    cross_configuration_hashes = {}
    for configuration in CONFIGURATIONS:
        configuration_result = {"seeds": {}, "aggregate": {}}
        aggregate_runs = {22: [[], [], []], 23: [[], [], []]}
        for seed in SEEDS:
            versions = captures[configuration][seed]
            reference_coordinates = versions[22][0]["coordinates"]
            for version in VERSIONS:
                for run in versions[version]:
                    if run["coordinates"] != reference_coordinates:
                        raise ValueError(
                            f"{configuration}/{seed}: target coordinates changed")
                reference_hashes = versions[version][0]["hashes"]
                if any(run["hashes"] != reference_hashes
                       for run in versions[version][1:]):
                    raise ValueError(
                        f"{configuration}/{seed}/v{version}: hashes drifted")
                key = (seed, version)
                if key in cross_configuration_hashes and \
                        cross_configuration_hashes[key] != reference_hashes:
                    raise ValueError(
                        f"{seed}/v{version}: Debug and Release hashes differ")
                cross_configuration_hashes[key] = reference_hashes
            changed = sum(
                versions[22][0]["hashes"][coordinate] !=
                versions[23][0]["hashes"][coordinate]
                for coordinate in reference_coordinates)
            if changed <= 0:
                raise ValueError(
                    f"{configuration}/{seed}: workload did not hit v23")

            metrics = {}
            for key in ("chunk_p95_ns", "chunk_p99_ns"):
                metrics[key] = metric_comparison(
                    [run[key] for run in versions[22]],
                    [run[key] for run in versions[23]])
            sample_count = len(reference_coordinates)
            status = "PASS" if all(
                metric["status"] == "PASS" for metric in metrics.values()) \
                else "FAIL"
            overall_pass = overall_pass and status == "PASS"
            configuration_result["seeds"][str(seed)] = {
                "status": status,
                "changed_chunk_count": changed,
                "target_chunk_count": sample_count,
                "percentile_sample_shape":
                    percentile_sample_shape(sample_count),
                "metrics": metrics,
                "generation_totals": total_summary(
                    versions, "generation_sum_ns"),
                "region_wall_totals": total_summary(
                    versions, "region_wall_ns"),
                "peak_rss_bytes": {
                    f"v{version}": [run["peak_rss_after_bytes"]
                                     for run in versions[version]]
                    for version in VERSIONS
                },
                "runs": {
                    f"v{version}": [{key: value for key, value in run.items()
                                      if key not in ("coordinates", "hashes",
                                                     "samples_ns")}
                                     for run in versions[version]]
                    for version in VERSIONS
                },
            }
            for version in VERSIONS:
                for round_index, run in enumerate(versions[version]):
                    aggregate_runs[version][round_index].extend(
                        run["samples_ns"])

        aggregate_metrics = {}
        for percentile, label in ((.95, "chunk_p95_ns"),
                                  (.99, "chunk_p99_ns")):
            aggregate_metrics[label] = metric_comparison(
                [nearest_rank(values, percentile)
                 for values in aggregate_runs[22]],
                [nearest_rank(values, percentile)
                 for values in aggregate_runs[23]])
        aggregate_status = "PASS" if all(
            metric["status"] == "PASS"
            for metric in aggregate_metrics.values()) else "FAIL"
        overall_pass = overall_pass and aggregate_status == "PASS"
        configuration_result["aggregate"] = {
            "status": aggregate_status,
            "percentile_sample_shape": percentile_sample_shape(
                len(aggregate_runs[22][0])),
            "metrics": aggregate_metrics,
        }
        configuration_result["status"] = "PASS" if all(
            seed["status"] == "PASS"
            for seed in configuration_result["seeds"].values()) and \
            aggregate_status == "PASS" else "FAIL"
        results[configuration] = configuration_result

    geometry = read_geometry(root, binary_hashes["Release"])
    overall_pass = overall_pass and \
        geometry["contract_budgets"]["status"] == "PASS"
    return {
        "schema": 1,
        "source": "PRODUCTION_CPP_B7_HEADLESS_GENERATION",
        "source_fingerprint": before,
        "round_order": ROUND_ORDER,
        "seeds": SEEDS,
        "versions": VERSIONS,
        "guardrail": GUARDRAIL,
        "percentile_method":
            "nearest-rank: one-based rank ceil(n*p), zero-based index rank-1",
        "small_sample_percentile_note":
            "Per-seed rank metadata states when P95 or P99 equals the "
            "per-run maximum; the 1.10 guardrail remains unchanged.",
        "binary_sha256": binary_hashes,
        "identity_status": "PASS",
        "environment": environment,
        "capture_order": capture_order,
        "geometry": geometry,
        "configurations": results,
        "status": "PASS" if overall_pass else "FAIL",
    }


def self_test():
    assert nearest_rank([9, 1, 3, 2], .50) == 2
    assert nearest_rank(list(range(1, 101)), .95) == 95
    assert nearest_rank(list(range(1, 101)), .99) == 99
    assert metric_comparison([100, 120, 110], [105, 121, 109])["status"] == "PASS"
    assert metric_comparison([100, 100, 100], [111, 111, 111])["status"] == "FAIL"
    for invalid in ("", "01", "+1", "-0", "1.0"):
        try:
            canonical_integer(invalid, "self-test")
        except ValueError:
            pass
        else:
            raise AssertionError(f"Accepted non-canonical integer: {invalid!r}")
    quoted = io.StringIO('a,b\n"x,y","q""r"\n')
    rows = list(csv.DictReader(quoted))
    assert rows == [{"a": "x,y", "b": 'q"r'}]
    assert require_sha256("a" * 64, "self-test") == "a" * 64
    order = expected_capture_order()
    assert len(order) == 36
    assert order[0] == (1, "Debug", 42, 1, 22)
    assert order[-1] == (36, "Release", 239701883, 3, 23)
    geometry = {
        **GEOMETRY_BUDGETS,
        **{f"v{version}_{category}_faces": 0
           for version in VERSIONS
           for category in ("solid", "water", "transparent", "flora")},
    }
    geometry["v23_solid_faces"] = NEW_VISIBLE_FACE_LIMIT - 1
    budget = geometry_budget_evaluation(geometry)
    assert budget["status"] == "PASS"
    assert budget["measured"]["new_visible_faces"] == 71999
    geometry["v23_solid_faces"] = NEW_VISIBLE_FACE_LIMIT
    assert geometry_budget_evaluation(geometry)["status"] == "FAIL"
    geometry["v23_solid_faces"] = 0
    geometry["changed_to_air"] += 1
    assert geometry_budget_evaluation(geometry)["status"] == "FAIL"
    print("[B7_GENERATION_COMPARE_SELF_TEST] status=PASS checks=17")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        if args.root is not None or args.output is not None:
            parser.error("--self-test does not accept evidence paths")
        self_test()
        return 0
    if args.root is None or args.output is None:
        parser.error("--root and --output are required")
    if args.output.exists():
        parser.error("Preserve previous comparisons; choose a new output")
    try:
        report = compare(args.root)
    except Exception as error:  # Preserve an auditable comparison failure.
        report = {"schema": 1, "status": "FAIL", "error": str(error)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8")
    print("[B7_GENERATION_COMPARE] status=" + report["status"])
    if "configurations" in report:
        for configuration, result in report["configurations"].items():
            print(configuration, result["status"])
            for seed, seed_result in result["seeds"].items():
                ratios = " ".join(
                    f"{name}={metric['ratio']:.4f}x"
                    for name, metric in seed_result["metrics"].items())
                print(f"  seed={seed} {seed_result['status']} {ratios}")
        budget = report["geometry"]["contract_budgets"]
        print("geometry_budgets", budget["status"])
        for seed, seed_budget in budget["seeds"].items():
            measured = seed_budget["measured"]
            print(
                f"  seed={seed} {seed_budget['status']} "
                f"changed_blocks={measured['changed_blocks']}/12000 "
                f"to_air={measured['changed_to_air']}/9000 "
                f"to_water={measured['changed_to_water']}/384 "
                f"to_structure={measured['changed_to_structure']}/1500 "
                f"new_visible_faces={measured['new_visible_faces']}/<72000")
    elif "error" in report:
        print(report["error"], file=sys.stderr)
    return report["status"] != "PASS"


if __name__ == "__main__":
    raise SystemExit(main())
