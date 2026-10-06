#!/usr/bin/env python3
"""Independent audit of opt-in shore original warm draw bindings.

First run the existing six-phase storage/World/map audit without changing its
gates, then verify actual draw packets and post-draw raw storage independently.
No builder, uploader, Ogre or production shader helper is imported or executed.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil

from shore_edit_capture_oracle import Audit, PHASES, OP_LIMIT, OPEN, integer, require, fresh_json

GL_FLOAT, GL_UNSIGNED_INT = 5126, 5125
GL_VERTEX_SHADER, GL_FRAGMENT_SHADER = 35633, 35632
GL_PRIMITIVES_GENERATED = 35975
# Actual production semantics, not GL attribute locations. The position shader
# accepts vec4 while its original source declaration supplies FLOAT3 (default w).
INPUTS = {'vertex': (0, 3, 1, 0, (35666,)),
          'uv0': (12, 2, 7, 0, (35664,)),
          'uv1': (20, 2, 7, 1, (35664,)),
          'uv2': (28, 3, 7, 2, (35665,)),
          'uv3': (40, 1, 7, 3, (GL_FLOAT,))}
REMAINING = [x for x in OPEN if x != 'inner_draw_vao_fetch'] + [
    'first-updateVAO-unbound-branch', 'unobserved-camera-or-shadow-passes',
    'shader-output-and-terrain-pixel-attribution']


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class NativeAudit(Audit):
    def run(self):
        super().run()
        self.base_check_count = len(self.checks)
        self.check('native/explicit-optin', self.record.get('shore_native_draw_requested') is True and
                   self.record.get('environment', {}).get('HELLOMINE3D_SHORE_NATIVE_DRAW') == '1')
        frames = []
        self.native_phases = []
        for phase in range(6):
            packet = self.load_json(self.path(f'shore-edit/phase-{phase:03d}.json'))
            observed = packet.get('native_draw_observation', {})
            prefix = f'native/phase{phase}'
            self.check(prefix + '/completed-observation',
                       observed.get('schema') == 'hellomine3d-shore-native-draw-v1' and
                       observed.get('status') == 'CAPTURED' and
                       packet.get('inner_draw_vertex_fetch') == 'SCOPED_ACTUAL_WARM_VAO_BINDINGS')
            frame = integer(observed['frame_id'], 0)
            camera = integer(observed['main_camera_id'], 1)
            self.check(prefix + '/separate-actual-render-and-UI-frame',
                       frame == integer(packet['native_render_frame_id'], 0) and
                       observed['ui_frame_id'] == packet['frame_id'])
            frames.append(frame)
            original = {(op['object_id'], op['upload_serial']): op for op in packet['operations']}
            operations = observed['operations']
            require(type(operations) is list and 1 <= len(operations) <= 8, 'native operation count outside bound')
            keys = [(integer(op['object_id'], 1), integer(op['upload_serial'], 1)) for op in operations]
            self.check(prefix + '/same-frame-complete-original-object-set',
                       len(keys) == len(set(keys)) and set(keys) == set(original))
            attempts = observed['draw_attempts']
            require(type(attempts) is list and 1 <= len(attempts) <= 128, 'bounded actual draw attempts required')
            self.check(prefix + '/bounded-current-frame-observation',
                       integer(observed['attempted_frames'], 1) >= 1 and
                       type(observed['open_reasons']) is list and len(observed['open_reasons']) <= 32 and
                       type(observed['prior_and_current_open_history']) is list and
                       len(observed['prior_and_current_open_history']) <= 32)
            summaries = []
            for i, op in enumerate(operations):
                p = prefix + f'/op{i}'
                source = original[keys[i]]
                vc, ic = source['vertex_count'], source['index_count']
                vbo, ebo = source['vertex_buffers'][0]['gl_id'], source['index_buffer']['gl_id']
                self.check(p + '/original-instance-pass-camera',
                           op['object_name'] == source['object_name'] and
                           op['material_name'] == source['material_name'] and
                           op['camera_id'] == camera and
                           integer(op['pass']['id'], 1) > 0 and op['pass']['index'] == 0 and
                           op['pass']['iterations'] == 1 and
                           op['pass']['main_camera'] is True and
                           isinstance(op['pass']['vertex_program'], str) and bool(op['pass']['vertex_program']) and
                           isinstance(op['pass']['fragment_program'], str) and bool(op['pass']['fragment_program']))
                self.check(p + '/supported-linked-native-program',
                           integer(op['program'], 1, 2**32-1) > 0 and op['pipeline'] == 0 and
                           op['program_linked'] is True and
                           op['render_system_stages'] == {'geometry': False, 'tessellation_hull': False,
                                                          'tessellation_domain': False, 'compute': False} and
                           all(type(value) is bool for value in op['render_system_stages'].values()))
                self.check(p + '/actual-single-instance-source0',
                           op['effective_instances'] == 1 and op['source0_instance_data'] is False and
                           op['global_instance_buffer_present'] is False and
                           type(op['global_instancing_available']) is bool and
                           (not op['global_instancing_available'] or op['global_instance_count'] == 1))
                shaders = op['attached_shaders']
                require(type(shaders) is list and len(shaders) == 2, 'native program requires exactly VS and FS')
                shader_by_stage = {integer(s['type'], 0, 2**32-1): integer(s['id'], 1, 2**32-1) for s in shaders}
                self.check(p + '/native-attached-stages-match-production-pass',
                           set(shader_by_stage) == {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER} and
                           shader_by_stage[GL_VERTEX_SHADER] == op['pass']['vertex_shader_id'] and
                           shader_by_stage[GL_FRAGMENT_SHADER] == op['pass']['fragment_shader_id'] and
                           all(s['compiled'] is True and s['production_pass_match'] is True for s in shaders))
                paired = [a for a in attempts if a['object_id'] == op['object_id'] and a['main_camera'] is True]
                self.check(p + '/one-paired-actual-main-camera-draw',
                           len(paired) == 1 and paired[0]['pre_called'] is True and
                           paired[0]['post_called'] is True and paired[0]['status'] == 'CAPTURED' and
                           paired[0]['camera_id'] == camera and paired[0]['pass_id'] == op['pass']['id'])
                self.check(p + '/original-triangle-range-and-storage-ids',
                           op['vertex_start'] == source['vertex_start'] == 0 and
                           op['index_start'] == source['index_start'] == 0 and
                           op['vertex_count'] == vc and op['index_count'] == ic and
                           op['index_type'] == 'u32' and op['instances'] == 1 and
                           integer(op['vao'], 1, 2**32-1) > 0 and op['vbo'] == vbo and
                           op['ebo'] == op['expected_ebo'] == ebo)
                query = op['primitive_query']
                self.check(p + '/owned-paired-actual-whole-operation-query',
                           query['target'] == GL_PRIMITIVES_GENERATED and
                           integer(query['id'], 1, 2**32-1) > 0 and
                           query['began'] is True and query['ended'] is True and
                           query['conflicting_query'] == 0 and query['current_query_after_end'] == 0 and
                           query['result'] == ic // 3 and query['expected_whole_operation'] == ic // 3)
                attrs = op['active_attributes']
                require(type(attrs) is list and 1 <= len(attrs) <= 5, 'active input count outside production bound')
                version = op['gl_version']
                require(type(version) is list and len(version) == 2, 'actual GL major/minor required')
                major, minor = (integer(value, 0, 99) for value in version)
                self.check(p + '/supported-actual-GL-version', (major, minor) >= (4, 1))
                long_mode = ('QUERY_GL43_ARRAY_LONG' if (major, minor) >= (4, 3)
                             else 'DERIVED_GL_FLOAT_EXCLUDES_DOUBLE')
                self.check(p + '/complete-actual-program-active-inputs', op['active_attribute_count'] == len(attrs))
                names, locations = set(), set()
                for ai, a in enumerate(attrs):
                    name = a['name']; require(name in INPUTS, 'unrecognised active production input')
                    offset, size, semantic, semantic_index, shader_types = INPUTS[name]
                    location = integer(a['location'], 0, 255)
                    require(name not in names and location not in locations, 'duplicate active input or location')
                    names.add(name); locations.add(location)
                    self.check(p + f'/active{ai}-{name}-original-source0-binding',
                               a['shader_type'] in shader_types and a['shader_array_size'] == 1 and
                               a['enabled'] is True and a['buffer'] == vbo and a['type'] == GL_FLOAT and
                               a['size'] == size and a['stride'] == 44 and a['normalized'] is False and
                               a['integer'] is False and a['long'] is False and a['divisor'] == 0 and
                               a['long_observation_mode'] == long_mode and
                               a['pointer_offset'] == offset and a['expected_offset'] == offset and
                               a['semantic'] == semantic and a['semantic_index'] == semantic_index)
                self.check(p + '/actual-position-input-and-no-invented-water-root',
                           'vertex' in names and (source['layer'] != 'water' or 'uv3' not in names))
                raw = op['source0']
                raw_names = [raw[f] for f in ('file', 'index_file', 'cpu_file', 'cpu_index_file')]
                old_names = [source['vertex_buffers'][0][f] for f in ('file', 'cpu_file')] + [
                    source['index_buffer'][f] for f in ('file', 'cpu_file')]
                self.check(p + '/four-distinct-post-raws-separate-from-legacy-storage',
                           len(set(raw_names)) == 4 and not set(raw_names).intersection(old_names) and
                           all(name.startswith(f'phase-{phase:03d}-native-op-') for name in raw_names))
                self.check(p + '/original-post-storage-extents', raw['stride'] == 44 and raw['vertices'] == vc and
                           raw['vertex_bytes'] == vc * 44 and raw['index_bytes'] == ic * 4 and
                           0 < raw['vertex_bytes'] + raw['index_bytes'] <= OP_LIMIT)
                native_v = self.read(self.path('shore-edit/' + raw['file']), OP_LIMIT)
                native_i = self.read(self.path('shore-edit/' + raw['index_file']), OP_LIMIT)
                cpu_v = self.read(self.path('shore-edit/' + raw['cpu_file']), OP_LIMIT)
                cpu_i = self.read(self.path('shore-edit/' + raw['cpu_index_file']), OP_LIMIT)
                original_cpu_v = self.read(self.path('shore-edit/' + source['vertex_buffers'][0]['cpu_file']), OP_LIMIT)
                original_cpu_i = self.read(self.path('shore-edit/' + source['index_buffer']['cpu_file']), OP_LIMIT)
                self.check(p + '/actual-post-native-and-same-frame-original-CPU-bytes',
                           len(native_v) == vc * 44 and len(native_i) == ic * 4 and
                           native_v == cpu_v == original_cpu_v and native_i == cpu_i == original_cpu_i and
                           raw['native_cpu_bytes_equal'] is True)
                # Base audit decodes the byte-identical original CPU/native
                # arrays independently; equality above applies those extents
                # and finite/index gates to these actual post-draw raw files.
                self.check(p + '/inspection-state-restored-and-GL0',
                           op['state_restored'] is True and op['context_before'] == op['context_after'] and
                           op['context_before']['vao'] == op['vao'] and
                           op['context_before']['program'] == op['program'] and
                           op['context_before']['pipeline'] == op['pipeline'] == 0 and
                           op['context_before']['vao_element_buffer'] == ebo and op['gl_errors'] == [])
                summaries.append({'object': op['object_name'], 'upload_serial': op['upload_serial'],
                                  'vao': op['vao'], 'vbo': vbo, 'ebo': ebo, 'program': op['program'],
                                  'generated_triangles': query['result'], 'inputs': sorted(names)})
            self.check(prefix + '/phase-GL0', observed['gl_errors'] == [])
            self.native_phases.append({'phase': PHASES[phase], 'render_frame': frame,
                                       'UI_frame': observed['ui_frame_id'], 'operations': summaries})
        self.check('native/strictly-increasing-real-render-frames', all(a < b for a, b in zip(frames, frames[1:])))
        observer_paths = {name: fact for name, fact in self.files.items() if name.endswith('.bin') or
                          name in {f'shore-edit/phase-{phase:03d}.json' for phase in range(6)}}
        self.check('native/shared-observer-raw-and-phase-metadata-output-budget',
                   sum(v['bytes'] for v in observer_paths.values()) <= 256 * 1024**2)

    def report(self, error=None):
        result = super().report(error)
        result.update(schema='hellomine3d-shore-native-draw-independent-oracle-v1',
                      status='FAIL' if error else 'PASS_SCOPED_STORAGE_WORLD_MAP_PIXELS_AND_WARM_DRAW_BINDINGS',
                      base_check_count=getattr(self, 'base_check_count', 0),
                      native_phases=getattr(self, 'native_phases', []),
                      scope='Original six-phase storage/World/map gates plus sampled actual main-camera warm draw native bindings and original post storage.',
                      open=REMAINING)
        return result


def audit(capture):
    instance = None
    try:
        instance = NativeAudit(capture); instance.run(); return instance.report()
    except Exception as error:
        if instance: return instance.report(error)
        return {'schema': 'hellomine3d-shore-native-draw-independent-oracle-v1',
                'status': 'FAIL', 'capture': str(capture), 'checks': [], 'error': str(error), 'open': REMAINING}


def fault_suite(capture, output):
    """Fresh copies of real packets/raws; keep original six-phase gates intact."""
    original = Path(capture).resolve(strict=True)
    root = original.parent
    source = json.loads(original.read_text())
    artifacts = source['artifacts']
    destination = Path(output)
    require(not destination.exists() and not destination.is_symlink(), 'fault-suite directory must be new')
    require(sum((root / p).stat().st_size for p in artifacts) <= 48 * 1024**2,
            'nine bounded fault copies require original artifacts within48MiB')
    original_hashes = {relative: digest(root / relative) for relative in artifacts}
    original_capture_sha = digest(original)
    require(original_hashes == artifacts, 'original source inventory differs before copy')
    destination.mkdir(parents=True)
    results = []
    faults = ('zeroVAO', 'wrongVBO', 'wrongEBO', 'stride32', 'zeroPrimitives',
              'computeStage', 'missingActiveInput', 'postRawMismatch', 'unpairedCamera')
    for fault in faults:
        clone = destination / fault
        clone.mkdir()
        for relative in artifacts:
            path = PurePosixPath(relative)
            require(not path.is_absolute() and '..' not in path.parts, 'unsafe fault input path')
            target = clone.joinpath(*path.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / relative, target)
        record = json.loads(original.read_text())
        packet_path = clone / 'shore-edit/phase-000.json'
        packet = json.loads(packet_path.read_text())
        op = packet['native_draw_observation']['operations'][0]
        changed = []
        if fault == 'zeroVAO': op['vao'] = 0
        elif fault == 'wrongVBO': op['vbo'] += 1
        elif fault == 'wrongEBO': op['ebo'] += 1
        elif fault == 'stride32': op['active_attributes'][0]['stride'] = 32
        elif fault == 'zeroPrimitives': op['primitive_query']['result'] = 0
        elif fault == 'computeStage': op['render_system_stages']['compute'] = True
        elif fault == 'missingActiveInput': op['active_attributes'].pop()
        elif fault == 'unpairedCamera': op['camera_id'] += 1
        elif fault == 'postRawMismatch':
            raw_path = clone / 'shore-edit' / op['source0']['file']
            data = bytearray(raw_path.read_bytes())
            data[0] ^= 1
            raw_path.write_bytes(data)
            changed.append(raw_path)
        if fault != 'postRawMismatch':
            packet_path.write_text(json.dumps(packet, allow_nan=False) + '\n')
            changed.append(packet_path)
        for path in changed:
            record['artifacts'][path.relative_to(clone).as_posix()] = digest(path)
        clone_capture = clone / 'capture.json'
        clone_capture.write_text(json.dumps(record, indent=2, allow_nan=False) + '\n')
        report = audit(clone_capture)
        rejected = (report['status'] == 'FAIL' and report.get('base_check_count') >= 208 and
                    all(c['status'] == 'PASS' for c in report['checks'][:report['base_check_count']]) and
                    any(c['name'] == 'capture/all-inventoried-file-sha256' and c['status'] == 'PASS'
                        for c in report['checks']) and 'SHA256' not in report.get('error', ''))
        fresh_json(clone / 'oracle.json', report)
        results.append({'fault': fault, 'status': 'REJECTED_BY_NATIVE_SEMANTICS_AFTER_BASE_PASS' if rejected else 'FAIL',
                        'base_check_count': report.get('base_check_count'),
                        'changed_files': [p.relative_to(clone).as_posix() for p in changed],
                        'oracle_error': report.get('error'), 'report': str(clone / 'oracle.json')})
    held = digest(original) == original_capture_sha and all(digest(root / p) == h for p, h in original_hashes.items())
    result = {'schema': 'hellomine3d-shore-native-draw-independent-faults-v1',
              'status': 'PASS_SCOPED_NATIVE_ORACLE_CALIBRATION' if held and all(
                  r['status'] == 'REJECTED_BY_NATIVE_SEMANTICS_AFTER_BASE_PASS' for r in results) else 'FAIL',
              'source_capture': str(original), 'source_capture_sha256': original_capture_sha,
              'source_originals_held': held, 'faults': results,
              'scope': 'Metadata/raw-copy oracle calibration only; no native source, VAO or GPU fault was injected.'}
    fresh_json(destination / 'faults.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', action='append', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--fault-suite', type=Path, help='New directory for nine real metadata/raw fault copies')
    args = parser.parse_args()
    require(1 <= len(args.capture) <= 2, 'one or two original captures required')
    reports = [audit(path) for path in args.capture]
    okay = all(r['status'] == 'PASS_SCOPED_STORAGE_WORLD_MAP_PIXELS_AND_WARM_DRAW_BINDINGS' for r in reports)
    faults = None
    if args.fault_suite and okay:
        try:
            faults = fault_suite(args.capture[0], args.fault_suite)
            okay = faults['status'] == 'PASS_SCOPED_NATIVE_ORACLE_CALIBRATION'
        except Exception as error:
            faults = {'status': 'FAIL', 'error': str(error)}
            okay = False
    result = {'schema': 'hellomine3d-shore-native-draw-independent-oracle-bundle-v1',
              'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'status': 'PASS_SCOPED_STORAGE_WORLD_MAP_PIXELS_AND_WARM_DRAW_BINDINGS' if okay else 'FAIL',
              'sessions': reports, 'fault_suite': faults, 'open': REMAINING}
    fresh_json(args.output, result)
    print(args.output)
    return 0 if okay else 1


if __name__ == '__main__':
    raise SystemExit(main())
