#!/usr/bin/env python3
"""Summarize E2 render spans from verified one-process client frame captures."""

import argparse
import csv
import hashlib
import json
import statistics
from pathlib import Path

from analyze_ecology_e2_tail import UNCHANGED_SCENE_FIELDS


GROUPS = ('forest-steady', 'forest-streaming',
          'shore-steady', 'shore-streaming')
SPANS = ('render_draw_ms', 'render_post_draw_ms', 'render_ended_ms')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def analyze(root):
    batch = json.loads((root / 'batch-status.json').read_text())
    comparison_path = root / 'comparison.json'
    comparison = json.loads(comparison_path.read_text())
    if (batch['result'] != 'CAPTURED' or batch['phase_count'] != 24 or
            not batch['render_phase_diagnostics'] or
            len(batch['batch_pid']) != 1 or
            comparison['identity_status'] != 'PASS' or
            comparison['batch_status'] != 'PASS'):
        raise ValueError('One-process diagnostic capture identity differs')
    groups = {}
    for group in GROUPS:
        versions = {}
        for version in (7, 8):
            phases = []
            for round_number in (1, 2, 3):
                phase = root / 'performance' / (
                    f'{group}-r{round_number}-v{version}')
                capture = json.loads((phase / 'capture.json').read_text())
                csv_path = phase / 'performance/frames.csv'
                summary_path = phase / 'performance/summary.txt'
                if (capture['result'] != 'CAPTURED' or
                        capture['batch_pid'] != batch['batch_pid'][0]):
                    raise ValueError(f'{phase}: phase identity differs')
                with csv_path.open(newline='') as stream:
                    frames = list(csv.DictReader(stream))
                if len(frames) != capture['render_phase_frames']:
                    raise ValueError(f'{phase}: frame count differs')
                stable_hot = []
                for index, frame in enumerate(frames):
                    spans = [float(frame[name]) for name in SPANS]
                    total = float(frame['render_ms'])
                    if (frame['render_phase_valid'] != '1' or
                            abs(sum(spans) - total) > .02):
                        raise ValueError(f'{phase}: render spans do not close')
                    if (index == 0 or
                            float(frame['measured_elapsed_ms']) < 12000 or
                            total < 15):
                        continue
                    previous = frames[index - 1]
                    if all(frame[name] == previous[name]
                           for name in UNCHANGED_SCENE_FIELDS):
                        stable_hot.append(frame)
                dominant = {name: sum(
                    max(SPANS, key=lambda key: float(frame[key])) == name
                    for frame in stable_hot) for name in SPANS}
                medians = {name: statistics.median(
                    float(frame[name]) for frame in stable_hot)
                    if stable_hot else None for name in SPANS}
                phases.append({
                    'phase': phase.name,
                    'frame_count': len(frames),
                    'frames_sha256': digest(csv_path),
                    'summary_sha256': digest(summary_path),
                    'stable_render_over_15_after_12s': len(stable_hot),
                    'dominant_span_counts': dominant,
                    'stable_hot_span_median_ms': medians,
                })
            versions[f'v{version}'] = {
                'stable_render_over_15_after_12s': sum(
                    phase['stable_render_over_15_after_12s']
                    for phase in phases),
                'dominant_span_counts': {name: sum(
                    phase['dominant_span_counts'][name]
                    for phase in phases) for name in SPANS},
                'phases': phases,
            }
        groups[group] = {
            'comparison_status': comparison['groups'][group]['status'],
            'frame_p99_ratio': comparison['groups'][group][
                'frame_metrics']['frame_p99_ms']['ratio'],
            'versions': versions,
        }
    return {
        'schema': 1,
        'source': 'E2_PRODUCTION_CPP_RENDER_PHASE_CSV',
        'diagnostic_binary_sha256': batch['package_identity'][
            'executable_sha256'],
        'batch_status_sha256': digest(root / 'batch-status.json'),
        'comparison_sha256': digest(comparison_path),
        'diagnostic_comparison_status': comparison['status'],
        'interpretation': ('draw span includes render-target update and any '
                           'driver wait there; post-draw includes final buffer '
                           'swap and LOD events. Neither span is pure GPU time.'),
        'groups': groups,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output file; preserve earlier analyses')
    result = analyze(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    for group, values in result['groups'].items():
        print(group, values['comparison_status'],
              f"p99={values['frame_p99_ratio']:.3f}x",
              'stable_hot=', {name: data['stable_render_over_15_after_12s']
                              for name, data in values['versions'].items()})


if __name__ == '__main__':
    main()
