#!/usr/bin/env python3
"""Read E2 candidate-23 production frame CSV for render-tail covariates."""

import argparse
import csv
import hashlib
import json
import statistics
from pathlib import Path


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def median(rows, field):
    return statistics.median(float(row[field]) for row in rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tail-analysis', type=Path, required=True)
    parser.add_argument('--project-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output; preserve previous analyses')
    source = json.loads(args.tail_analysis.read_text())
    scene_fields = source['groups']['final']['phases'][0][
        'unchanged_scene_field_names']
    phases = []
    for group in ('final', 'recheck'):
        for entry in source['groups'][group]['phases']:
            if entry['unchanged_scene_field_names'] != scene_fields:
                raise ValueError('Stable-scene field list differs')
            directory = args.project_root / entry['phase']
            frames = directory / 'performance/frames.csv'
            actual_hash = sha256(frames)
            if actual_hash != entry['frames_sha256']:
                raise ValueError(f'Frame CSV changed: {frames}')
            with frames.open(newline='') as stream:
                rows = list(csv.DictReader(stream))
            hot, control, stable = [], [], []
            for previous, row in zip(rows, rows[1:]):
                if float(row['measured_elapsed_ms']) < 12000 or any(
                        row[field] != previous[field] for field in scene_fields):
                    continue
                stable.append(row)
                signal = {
                    'simulation_tick_changed': row['simulation_ticks'] !=
                        previous['simulation_ticks'],
                    'actor_count_changed': row['actor_count'] !=
                        previous['actor_count'],
                    'daylight_changed': row['daylight'] != previous['daylight'],
                    'fog_changed': row['fog_density'] !=
                        previous['fog_density'],
                    'world_time_changed': row['world_time'] !=
                        previous['world_time'],
                    'debug_gui_nonzero': float(row['debug_gui_ms']) > 0,
                    'render_capture_nonzero':
                        float(row['render_capture_ms']) > 0,
                    'display_nonzero': float(row['display_ms']) > 0,
                }
                (hot if float(row['render_ms']) >= 15 else control).append(
                    signal)
            def counts(signals):
                return {key: sum(item[key] for item in signals)
                        for key in (signals[0] if signals else [])}
            phases.append({
                'group': group,
                'phase': entry['phase'],
                'frames_sha256': actual_hash,
                'stable_frames_after_12s': len(stable),
                'hot_render_frames': len(hot),
                'hot_signals': counts(hot),
                'control_signals': counts(control),
                'control_frames': len(control),
                'median_solid_faces': median(stable, 'solid_faces'),
                'median_resident_terrain_buffer_bytes': median(
                    stable, 'resident_terrain_buffer_bytes'),
                'median_actor_count': median(stable, 'actor_count'),
                'median_render_ms': median(stable, 'render_ms'),
            })
    if len(phases) != 12:
        raise ValueError('Expected 12 forest-streaming phases')
    pairs = []
    for group in ('final', 'recheck'):
        for round_number in (1, 2, 3):
            prefix = f'forest-streaming-r{round_number}-'
            v7 = next(row for row in phases if row['group'] == group and
                      row['phase'].endswith(prefix + 'v7'))
            v8 = next(row for row in phases if row['group'] == group and
                      row['phase'].endswith(prefix + 'v8'))
            pairs.append({
                'group': group, 'round': round_number,
                'v7_solid_faces': v7['median_solid_faces'],
                'v8_solid_faces': v8['median_solid_faces'],
                'v7_resident_terrain_buffer_bytes':
                    v7['median_resident_terrain_buffer_bytes'],
                'v8_resident_terrain_buffer_bytes':
                    v8['median_resident_terrain_buffer_bytes'],
                'v7_actor_count': v7['median_actor_count'],
                'v8_actor_count': v8['median_actor_count'],
                'v7_render_ms': v7['median_render_ms'],
                'v8_render_ms': v8['median_render_ms'],
            })
    result = {
        'schema': 1,
        'source': 'READ_ONLY_CANDIDATE23_PRODUCTION_FRAME_CSV',
        'tail_analysis_sha256': sha256(args.tail_analysis),
        'classification': 'CPU_COVARIATE_ANALYSIS_NOT_CAUSE_OR_PASS',
        'stable_scene_fields': scene_fields,
        'phases': phases,
        'pairs': pairs,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Validated {len(phases)} source CSVs and {len(pairs)} v7/v8 pairs')


if __name__ == '__main__':
    main()
