#!/usr/bin/env python3
"""Compare the frozen three-run T0/T1 macOS client performance protocol.

Reads real capture records/summary/frames; update time is not pure upload time.
"""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path

METRICS = ('frame_p95_ms', 'frame_p99_ms', 'update_p95_ms', 'render_p95_ms')


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]


def read_run(directory, version):
    capture = json.loads((directory / 'capture.json').read_text())
    if capture['result'] != 'CAPTURED':
        raise ValueError(f'{directory}: capture incomplete')
    summary = dict(line.split('=', 1) for line in
                   (directory / 'performance/summary.txt').read_text().splitlines() if '=' in line)
    expected = {'terrain_generation_version': str(version), 'terrain_seed': '20260807',
                'build_configuration': 'Release'}
    if any(summary.get(k) != v for k, v in expected.items()):
        raise ValueError(f'{directory}: configuration/terrain identity mismatch')
    if float(summary['warmup_ms']) != 5000 or float(summary['duration_ms']) != 30000:
        raise ValueError(f'{directory}: measurement protocol mismatch')
    with (directory / 'performance/frames.csv').open() as source:
        frames = list(csv.DictReader(source))
    if not frames:
        raise ValueError(f'{directory}: no frames')
    mesh_delta = [max(0, float(b['mesh_build_total_ms']) - float(a['mesh_build_total_ms']))
                  for a, b in zip(frames, frames[1:])]
    rebuilds = int(frames[-1]['mesh_rebuilds']) - int(frames[0]['mesh_rebuilds'])
    result = {key: float(summary[key]) for key in METRICS}
    result.update({
        'measured_frames': len(frames),
        'mesh_rebuilds_during_capture': rebuilds,
        'mesh_cumulative_delta_ms': sum(mesh_delta),
        'mesh_delta_per_frame_p95_ms': percentile(mesh_delta, .95),
        'resident_buffer_max_bytes': max(int(f['resident_terrain_buffer_bytes']) for f in frames),
        'cpu_ready_p95': percentile([int(f['cpu_ready_sections']) for f in frames], .95),
        'cpu_ready_max': max(int(f['cpu_ready_sections']) for f in frames),
        'last_mesh_dirty_sections': int(frames[-1]['mesh_dirty_sections']),
        'last_gpu_buffered_sections': int(frames[-1]['gpu_buffered_sections']),
        'capture_directory': str(directory),
        'package_sha256': capture['package_identity']['executable_sha256'],
        'position': capture['environment']['HELLOMINE3D_PLAYER_POSITION'],
    })
    return result


def compare(root):
    errors = []
    scenes = {}
    for scene in ('steady', 'streaming'):
        groups = {}
        for prefix, version in (('baseline', 4), ('v5', 5)):
            runs = [read_run(root / f'{prefix}-{scene}{suffix}', version)
                    for suffix in ('', '-r2', '-r3')]
            if len({r['package_sha256'] for r in runs}) != 1:
                errors.append(f'{prefix}/{scene}: inconsistent packages')
            groups[prefix] = {'runs': runs, 'median': {
                key: statistics.median(run[key] for run in runs)
                for key in METRICS + ('resident_buffer_max_bytes', 'cpu_ready_p95',
                                     'mesh_cumulative_delta_ms', 'mesh_rebuilds_during_capture')}}
        old, new = groups['baseline']['median'], groups['v5']['median']
        limits = {'frame_p95_ms': old['frame_p95_ms'] * 1.20 + 2,
                  'frame_p99_ms': old['frame_p99_ms'] * 1.20 + 2,
                  'update_p95_ms': old['update_p95_ms'] * 1.25 + .5}
        for key, limit in limits.items():
            if new[key] > limit:
                errors.append(f'{scene}/{key}: {new[key]} > {limit}')
        groups['limits'] = limits
        scenes[scene] = groups
    return {'schema': 1, 'result': 'FAIL' if errors else 'PASS', 'errors': errors,
            'scope': 'fixed developer diagnostics; update includes upload and other main-thread work',
            'scenes': scenes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = compare(args.root)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'result': result['result'], 'errors': result['errors']}, indent=2))
    return bool(result['errors'])


if __name__ == '__main__':
    raise SystemExit(main())
