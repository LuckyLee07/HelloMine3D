#!/usr/bin/env python3
"""Classify E2 forest-streaming tail frames from untouched client CSVs."""

import argparse
import csv
import hashlib
import json
import statistics
from pathlib import Path


UNCHANGED_SCENE_FIELDS = (
    'loaded_chunks', 'gpu_buffered_sections',
    'resident_terrain_buffer_bytes', 'solid_faces',
    'transparent_faces', 'water_faces', 'flora_faces',
    'mesh_rebuilds', 'mesh_build_total_ms',
)


def sha256(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def summarize_phase(root, round_number, version):
    phase = root / f'forest-streaming-r{round_number}-v{version}'
    frame_file = phase / 'performance' / 'frames.csv'
    summary_file = phase / 'performance' / 'summary.txt'
    with frame_file.open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    summary = dict(line.split('=', 1) for line in summary_file.read_text().splitlines()
                   if '=' in line)
    bins = [0] * 6
    render_bins = [0] * 6
    stable_hot = []
    stable_changed = 0
    for index, row in enumerate(rows):
        elapsed = float(row['measured_elapsed_ms'])
        bin_number = min(5, int(elapsed // 5000))
        if float(row['frame_ms']) >= 20:
            bins[bin_number] += 1
        if float(row['render_ms']) < 15:
            continue
        render_bins[bin_number] += 1
        if elapsed < 12000 or index == 0:
            continue
        previous = rows[index - 1]
        if all(row[field] == previous[field]
               for field in UNCHANGED_SCENE_FIELDS):
            stable_hot.append(row)
        else:
            stable_changed += 1
    return {
        'phase': str(phase),
        'frames_sha256': sha256(frame_file),
        'summary_sha256': sha256(summary_file),
        'frames': len(rows),
        'frame_p95_ms': float(summary['frame_p95_ms']),
        'frame_p99_ms': float(summary['frame_p99_ms']),
        'update_p99_ms': float(summary['update_p99_ms']),
        'render_p99_ms': float(summary['render_p99_ms']),
        'frame_over_20_by_five_seconds': bins,
        'render_over_15_by_five_seconds': render_bins,
        'render_over_15_after_12s_with_unchanged_scene': len(stable_hot),
        'render_over_15_after_12s_with_changed_scene': stable_changed,
        'unchanged_scene_field_names': list(UNCHANGED_SCENE_FIELDS),
        'unchanged_scene_hot_update_median_ms': (
            statistics.median(float(row['update_ms']) for row in stable_hot)
            if stable_hot else None),
        'unchanged_scene_hot_render_median_ms': (
            statistics.median(float(row['render_ms']) for row in stable_hot)
            if stable_hot else None),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--final', type=Path, required=True)
    parser.add_argument('--recheck', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output path; preserve earlier evidence')
    groups = {}
    for name, directory in (('final', args.final),
                            ('recheck', args.recheck)):
        root = directory / 'performance'
        comparison_file = root / 'comparison.json'
        comparison = json.loads(comparison_file.read_text())
        phases = [summarize_phase(root, round_number, version)
                  for round_number in (1, 2, 3) for version in (7, 8)]
        group = comparison['groups']['forest-streaming']
        groups[name] = {
            'comparison_sha256': sha256(comparison_file),
            'comparison_status': group['status'],
            'frame_p95_ratio': group['frame_metrics']['frame_p95_ms']['ratio'],
            'frame_p99_ratio': group['frame_metrics']['frame_p99_ms']['ratio'],
            'phases': phases,
        }
    result = {
        'schema': 1,
        'source': 'E2_RELEASE_CLIENT_RAW_FRAMES',
        'status': 'FAIL_UNRESOLVED',
        'scope': 'forest-streaming 12 phases; 30-second raw client captures',
        'classification': (
            'High render frames also occur after loaded chunks, mesh totals, '
            'visible face counts and resident buffer size stop changing. '
            'The CSV cannot separate GPU work, present waits or OS scheduling.'),
        'groups': groups,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({
        name: {'status': value['comparison_status'],
               'p99_ratio': value['frame_p99_ratio'],
               'stable_render_spikes': sum(
                   phase['render_over_15_after_12s_with_unchanged_scene']
                   for phase in value['phases'])}
        for name, value in groups.items()}, indent=2))


if __name__ == '__main__':
    main()
