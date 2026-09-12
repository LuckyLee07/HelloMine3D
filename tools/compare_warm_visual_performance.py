#!/usr/bin/env python3
"""Compare the predeclared three-run Warm Wilderness frame-time guardrail."""
import argparse
import csv
import json
from pathlib import Path
import statistics


def read_run(path):
    record = json.loads((path / 'capture.json').read_text())
    if record['result'] != 'CAPTURED':
        raise ValueError(f'Incomplete capture: {path}')
    summary = dict(line.split('=', 1) for line in
                   (path / 'performance/summary.txt').read_text().splitlines())
    for key, expected in {'build_configuration': 'Release', 'warmup_ms': '5000.000',
                          'duration_ms': '30000.000', 'terrain_seed': '20260807',
                          'terrain_generation_version': '5', 'terrain_vertex_stride_bytes': '32',
                          'terrain_index_stride_bytes': '4', 'startup_success': '1', 'entry_success': '1'}.items():
        if summary[key] != expected:
            raise ValueError(f'{path}: {key}={summary[key]} expected={expected}')
    with (path / 'performance/frames.csv').open() as stream:
        samples = [float(frame['frame_ms']) for frame in csv.DictReader(stream)]
    if not samples:
        raise ValueError(f'No measured frames: {path}')
    for threshold in (33, 50, 100):
        summary[f'frames_over_{threshold}_ms'] = str(sum(value > threshold for value in samples))
    return record, summary


def compare(root, candidate, dense=False):
    rows = []
    passed = True
    for quality in ('off', 'medium', 'high'):
        baseline_runs, candidate_runs = [], []
        for run in range(1, 4):
            old_record, old = read_run(root / f'baseline-perf-{quality}-r{run}')
            new_record, new = read_run(root / f'{candidate}-perf-{quality}-r{run}')
            if old_record['settings'] != new_record['settings'] or old_record['scene'] != new_record['scene']:
                raise ValueError('Scene/settings mismatch')
            for key in ('HELLOMINE3D_SEED', 'HELLOMINE3D_PLAYER_POSITION',
                        'HELLOMINE3D_PLAYER_ROTATION', 'HELLOMINE3D_WORLD_TIME', 'HELLOMINE3D_HUD_FIXTURE'):
                if old_record['environment'].get(key) != new_record['environment'].get(key):
                    raise ValueError(f'Diagnostic mismatch: {key}')
            baseline_runs.append(old)
            candidate_runs.append(new)
        row = {'quality': quality, 'frame_metrics': {}, 'geometry_metrics': {}}
        def metric(key):
            old = [float(run[key]) for run in baseline_runs]
            new = [float(run[key]) for run in candidate_runs]
            a, b = statistics.median(old), statistics.median(new)
            return {'baseline_runs': old, 'candidate_runs': new, 'baseline_median': a,
                    'candidate_median': b, 'ratio': b / a if a else None}
        for key in ('frame_p95_ms', 'frame_p99_ms'):
            result = metric(key)
            result['pass'] = result['ratio'] <= 1.10
            passed &= result['pass']
            row['frame_metrics'][key] = result
        for key in ('last_existing_chunks', 'last_loaded_chunks', 'last_solid_faces',
                    'last_resident_terrain_vertices', 'last_resident_terrain_indices',
                    'last_resident_terrain_buffer_bytes', 'last_mesh_build_avg_ms',
                    'last_queued_chunk_updates'):
            row['geometry_metrics'][key] = metric(key)
        if dense:
            geometry = row['geometry_metrics']['last_resident_terrain_buffer_bytes']
            geometry['pass'] = geometry['ratio'] <= 1.35
            passed &= geometry['pass']
        row['tail_metrics'] = {key: metric(key) for key in (
            'frame_max_ms', 'frames_over_33_ms', 'frames_over_50_ms', 'frames_over_100_ms')}
        rows.append(row)
    return {'guardrail': 'median of 3, P95 and P99 <= 110% of baseline in each quality',
            'dense_mesh_guardrail': '<= 135%' if dense else 'recorded',
            'status': 'PASS' if passed else 'FAIL', 'rows': rows}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--candidate', default='candidate-r3')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dense', action='store_true')
    args = parser.parse_args()
    result = compare(args.root, args.candidate, args.dense)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    for row in result['rows']:
        print(row['quality'], ' '.join(f'{key}={value["ratio"]:.3f}x' for key, value in row['frame_metrics'].items()))
    print('[WARM_PERF]', result['status'])
    raise SystemExit(0 if result['status'] == 'PASS' else 1)
