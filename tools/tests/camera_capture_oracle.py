#!/usr/bin/env python3
"""CPU-only independent checks of captured ordinary-client camera observations.

No Ogre/production camera algorithm import. Matrix inversion and triangle/AABB
SAT operate on observed data. Original GPU geometry, hand pixels and residency
ABA remain OPEN. Synthetic --self-test calibrates math, never client evidence.
"""
import argparse
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import struct
import sys
import zlib

SCHEMA = 'hellomine3d-actual-client-camera-capture-v1'
REPORT_SCHEMA = 'hellomine3d-actual-client-camera-cpu-oracle-v1'
PHASES = ('clear_rear', 'wide_sidewall', 'low_ceiling', 'explicit_first',
          'corner_fallback', 'release_clear')
EFFECTIVE = {'clear_rear': 'third', 'wide_sidewall': 'third',
             'explicit_first': 'first', 'corner_fallback': 'first',
             'release_clear': 'third'}
SAT_EPS = 0.00003
CORNER_EPS = 0.0003
MATRIX_EPS = 0.001
BYTE_LIMIT = 64 * 1024 * 1024


def vector(value, count):
    if not isinstance(value, list) or len(value) != count:
        raise ValueError('invalid vector/matrix size')
    if any(type(x) not in (int, float) or not math.isfinite(x) for x in value):
        raise ValueError('non-finite or non-numeric vector/matrix')
    return [float(x) for x in value]


def integer(value):
    if type(value) is not int:
        raise ValueError('expected integer')
    return value


def mul(a, b):
    a, b = vector(a, 16), vector(b, 16)
    return [sum(a[4*r+k]*b[4*k+c] for k in range(4))
            for r in range(4) for c in range(4)]


def transform(a, x):
    return [sum(a[4*r+c]*x[c] for c in range(4)) for r in range(4)]


def inverse(a):
    a = vector(a, 16)
    rows = [[a[4*r+c] for c in range(4)] + [float(r == c) for c in range(4)]
            for r in range(4)]
    for col in range(4):
        pivot = max(range(col, 4), key=lambda r: abs(rows[r][col]))
        if abs(rows[pivot][col]) <= 1e-14:
            raise ValueError('singular projection/view matrix')
        rows[col], rows[pivot] = rows[pivot], rows[col]
        scale = rows[col][col]
        rows[col] = [x/scale for x in rows[col]]
        for r in range(4):
            if r != col:
                scale = rows[r][col]
                rows[r] = [x-scale*y for x, y in zip(rows[r], rows[col])]
    return [rows[r][c+4] for r in range(4) for c in range(4)]


def near_quad(projection, view):
    inv = inverse(mul(projection, view))
    result = []
    # Canonical GL near plane is z=-1. Order agrees with Ogre world corners.
    for x, y in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        point = transform(inv, [x, y, -1, 1])
        if abs(point[3]) <= 1e-14:
            raise ValueError('near point at infinity')
        result.append([point[k]/point[3] for k in range(3)])
    return result


def sub(a, b): return [x-y for x, y in zip(a, b)]
def dot(a, b): return sum(x*y for x, y in zip(a, b))
def cross(a, b): return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def triangle_box(triangle, minimum, maximum, epsilon=SAT_EPS):
    """13-axis triangle/AABB SAT; mere touching does not count as penetration."""
    points = [vector(p, 3) for p in triangle]
    minimum, maximum = vector(minimum, 3), vector(maximum, 3)
    if any(lo >= hi for lo, hi in zip(minimum, maximum)):
        raise ValueError('degenerate box')
    centre = [(lo+hi)*0.5 for lo, hi in zip(minimum, maximum)]
    half = [(hi-lo)*0.5 for lo, hi in zip(minimum, maximum)]
    points = [sub(p, centre) for p in points]
    edges = [sub(points[(i+1)%3], points[i]) for i in range(3)]
    bases = ([1, 0, 0], [0, 1, 0], [0, 0, 1])
    normal = cross(edges[0], edges[1])
    if dot(normal, normal) <= 1e-24:
        raise ValueError('degenerate near triangle')
    axes = list(bases) + [normal] + [cross(edge, basis) for edge in edges for basis in bases]
    for axis in axes:
        length = math.sqrt(dot(axis, axis))
        if length <= 1e-12:
            continue
        axis = [x/length for x in axis]
        extent = sum(abs(x)*h for x, h in zip(axis, half))
        projected = [dot(p, axis) for p in points]
        if max(projected) <= -extent+epsilon or min(projected) >= extent-epsilon:
            return False
    return True


def quad_box(quad, minimum, maximum):
    return [i for i, corners in enumerate(((0, 1, 2), (0, 2, 3)))
            if triangle_box([quad[k] for k in corners], minimum, maximum)]


def matrix_close(a, b):
    a, b = vector(a, 16), vector(b, 16)
    return all(abs(x-y) <= MATRIX_EPS + 1e-5*max(abs(x), abs(y)) for x, y in zip(a, b))


def corner_set_distance(a, b):
    a, b = [vector(p, 3) for p in a], [vector(p, 3) for p in b]
    if len(a) != 4 or len(b) != 4:
        raise ValueError('need four near corners')
    return min(max(math.dist(a[i], b[j]) for i, j in enumerate(order))
               for order in itertools.permutations(range(4)))


def evaluate_frame(frame, require_nominal=False):
    checks, opens = [], []
    def check(name, ok, details=None):
        checks.append({'name': name, 'ok': bool(ok), 'details': details})
    def unresolved(name, details):
        opens.append({'name': name, 'details': details})
    phase = frame['phase']
    cam, player, world = frame['camera'], frame['player'], frame['world']
    projection, view = vector(cam['projection_RS_depth_row_major'], 16), vector(cam['view_row_major'], 16)
    quad = near_quad(projection, view)
    distance = corner_set_distance(quad, cam['near_world_corners'])
    check('actual_projection_unprojects_actual_near_four', distance <= CORNER_EPS,
          {'max_set_distance': distance, 'tolerance': CORNER_EPS})
    check('perspective_and_finite_projection_settings', cam['projection_type'] == 1 and
          0 < cam['near'] < cam['far'] and 0 < cam['fov_y_degrees'] < 180 and cam['aspect'] > 0)
    position = vector(cam['position'], 3)
    eye = transform(inverse(view), [0, 0, 0, 1])
    check('actual_view_origin_matches_actual_camera_position',
          math.dist([eye[k]/eye[3] for k in range(3)], position) <= CORNER_EPS)
    orientation = vector(cam['orientation_wxyz'], 4)
    w, x, y, z = orientation
    rotation = [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w),
                2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w),
                2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]
    camera_world = inverse(view)
    check('actual_view_orientation_matches_actual_camera_orientation',
          abs(sum(c*c for c in orientation)-1) <= 1e-5 and
          all(abs(camera_world[4*r+c]-rotation[3*r+c]) <= 1e-5 for r in range(3) for c in range(3)))
    view_points = [transform(view, p+[1]) for p in quad]
    half_height = cam['near']*math.tan(math.radians(cam['fov_y_degrees'])*.5)
    half_width = half_height*cam['aspect']
    check('actual_projection_near_FOV_aspect_semantics',
          all(abs(p[2]+cam['near']) <= 1e-6 for p in view_points) and
          abs(max(p[1] for p in view_points)-min(p[1] for p in view_points)-2*half_height) <= 1e-6 and
          abs(max(p[0] for p in view_points)-min(p[0] for p in view_points)-2*half_width) <= 1e-6)
    effective = cam['effective']
    if effective == 'first':
        check('first_actual_camera_position_preserves_logic_eye',
              math.dist(position, vector(cam['logic_position'], 3)) <= CORNER_EPS)
    expected_requested = 'first' if phase == 'explicit_first' else 'third'
    check('phase_requested_and_effective_mode', cam['requested'] == expected_requested and
          effective in ('first', 'third') and (phase not in EFFECTIVE or effective == EFFECTIVE[phase]),
          {'requested': cam['requested'], 'effective': effective,
           'expected_requested': expected_requested, 'expected_effective': EFFECTIVE.get(phase, 'either_valid_mode')})
    if 'nominal_near' in cam:
        nominal = cam['nominal_near']
        check('positive_nominal_near_and_same_frame_third_restoration',
              type(nominal) in (int, float) and math.isfinite(nominal) and nominal > 0 and
              (effective != 'third' or abs(cam['near']-nominal) <= 1e-6),
              {'actual_near': cam['near'], 'nominal_near': nominal, 'effective': effective})
    elif require_nominal:
        check('required_actual_nominal_near_field', False)
    else:
        unresolved('nominal_near_not_observed', 'Old Before observer did not record nominal; no new historical failure invented.')
    safety_fields = ('near_clip_queries', 'near_clip_safety_unresolved', 'near_clip_status')
    if all(name in cam for name in safety_fields):
        check('actual_near_safety_resolved_with_bounded_queries',
              type(cam['near_clip_queries']) is int and 0 <= cam['near_clip_queries'] <= 128 and
              cam['near_clip_safety_unresolved'] is False and type(cam['near_clip_status']) is int,
              {name: cam[name] for name in safety_fields})
    elif require_nominal:
        check('required_actual_near_safety_fields', False, {'missing': [name for name in safety_fields if name not in cam]})
    else:
        unresolved('old_observer_near_safety_not_recorded', 'Before lacks later policy status/query fields; collision measured independently.')
    scan_min, scan_max = [integer(x) for x in world['scan_minimum']], [integer(x) for x in world['scan_maximum']]
    if len(scan_min) != 3 or len(scan_max) != 3 or any(a > b for a, b in zip(scan_min, scan_max)):
        raise ValueError('invalid voxel scan bounds')
    scan_count = math.prod(b-a+1 for a, b in zip(scan_min, scan_max))
    voxels = world['voxels']
    positions = [tuple(integer(x) for x in v['position']) for v in voxels]
    expected = set(itertools.product(*(range(a, b+1) for a, b in zip(scan_min, scan_max)))) if scan_count <= 512 else set()
    stable = all(v['known'] is True and v['before_known'] is True and v['after_known'] is True and
                 type(v['before_block_revision']) is int and v['before_block_revision'] >= 0 and
                 v['before_block_revision'] == v['after_block_revision'] and type(v['collidable']) is bool for v in voxels)
    complete = (world['scan_truncated'] is False and scan_count <= 512 and
                scan_count == world['requested_voxels'] == world['copied_voxels'] == len(voxels) and
                len(set(positions)) == len(positions) and set(positions) == expected and
                world['unknown_voxels'] == 0 and world['changed_sections_at_voxels'] == 0 and stable)
    check('complete_loaded_revision_stable_voxel_envelope', complete,
          {'scan_count': scan_count, 'records': len(voxels), 'truncated': world['scan_truncated'],
           'unknown': world['unknown_voxels'], 'changed': world['changed_sections_at_voxels']})
    centre, half = vector(player['position'], 3), vector(player['box_half_extents'], 3)
    check('actual_player_collider_positive_half_extents', all(x > 0 for x in half))
    player_min, player_max = sub(centre, half), [x+y for x, y in zip(centre, half)]
    quad_min = [min(p[a] for p in quad) for a in range(3)]
    quad_max = [max(p[a] for p in quad) for a in range(3)]
    bounds_min = [min(p[a] for p in quad+[player_min]) for a in range(3)]
    bounds_max = [max(p[a] for p in quad+[player_max]) for a in range(3)]
    envelope_contains = all(scan_min[a] <= math.floor(bounds_min[a]) and
                            math.floor(bounds_max[a]) <= scan_max[a] for a in range(3))
    check('independent_near_quad_and_player_envelope_in_scan', envelope_contains)
    hits = []
    if complete and envelope_contains:
        for voxel in voxels:
            if voxel['collidable']:
                lo = list(voxel['position']); hi = [x+1 for x in lo]
                triangles = quad_box(quad, lo, hi)
                if triangles:
                    hits.append({'voxel': lo, 'block_id': voxel['block_id'], 'triangles': triangles,
                                 'near_bbox_overlap': [max(0, min(quad_max[a], hi[a])-max(quad_min[a], lo[a])) for a in range(3)]})
        check('actual_near_quad_has_no_known_solid_penetration', not hits,
              {'hits': hits, 'SAT_world_epsilon': SAT_EPS, 'method': 'two_triangles_each_13_axes'})
    else:
        unresolved('near_quad_solid_SAT_NOT_MEASURED', 'Incomplete resident/revision-stable envelope cannot close collision safety.')
    player_hits = []
    if effective == 'third' and all(x > 0 for x in half):
        player_hits = quad_box(quad, player_min, player_max)
        check('third_near_quad_does_not_cut_actual_player_box', not player_hits, {'triangles': player_hits})
    objects = frame['player_objects']
    part_objects = [o for o in objects if o['role'] == 'part']
    held_objects = [o for o in objects if o['role'] == 'held']
    check('exact_eight_unique_actual_parts_and_one_held', len(objects) == 9 and len(part_objects) == 8 and
          {o['part_index'] for o in part_objects} == set(range(8)) and len(held_objects) == 1 and
          len({o['name'] for o in objects}) == 9 and frame['player_part_indices_observed'] == 8 and
          frame['held_objects_observed'] == 1)
    expected_visible = effective == 'third'
    check('actual_objects_visible_coherent_with_effective_mode',
          all(o['visible'] is expected_visible and o['is_visible'] is expected_visible for o in objects))
    check('actual_UI_first_hand_flag_coherent_with_effective_mode',
          frame['first_person_hand']['actual_presentation_flag'] is (not expected_visible))
    draw_count, queued, queue_integrity, matrices = 0, [], True, True
    for obj in objects:
        parent = vector(obj['parent_world_row_major'], 16)
        for section in obj['sections']:
            draws = section['main_camera_draws']
            queue_integrity &= section['main_camera_queue_membership'] is bool(draws)
            if draws:
                queued.append({'object': obj['name'], 'section': section['index']})
            elif expected_visible:
                unresolved('third_original_section_not_observed_in_main_queue',
                           {'object': obj['name'], 'section': section['index'],
                            'cause': 'Frustum/cull reason and original geometry not captured; no fabricated visibility PASS.'})
            for draw in draws:
                draw_count += 1
                world_matrix = vector(draw['world_row_major'], 16)
                source_view, source_projection = vector(draw['view_row_major'], 16), vector(draw['projection_row_major'], 16)
                matrices &= (draw['camera_identity'] == cam['identity'] and
                             draw['renderable_identity'] == section['renderable_identity'] and
                             draw['provenance'] == 'ACTUAL_MAIN_CAMERA_AUTOPARAM_SOURCE_PRE_GPU_BIND' and
                             matrix_close(world_matrix, parent) and matrix_close(source_view, view) and
                             matrix_close(source_projection, projection) and
                             matrix_close(draw['WVP_row_major'], mul(mul(source_projection, source_view), world_matrix)))
    queue = frame['queue']
    check('queue_membership_counts_not_truncated', queue_integrity and
          queue['selected_notifications_truncated'] is False and
          draw_count == queue['selected_notifications'] and draw_count <= 64 and
          queue['main_camera_notifications'] >= draw_count and queue['main_camera_notifications'] > 0)
    check('queued_actual_camera_world_view_projection_WVP_consistent', matrices,
          {'source': 'lazy AutoParamDataSource before actual GPU parameter binding', 'draw_count': draw_count})
    if effective == 'first':
        check('first_has_no_actual_player_or_held_main_queue_draws', not queued)
    elif not objects or any(not o['sections'] for o in objects):
        unresolved('third_object_section_coverage', 'Empty actual object/section cannot close original draw coverage.')
    for name in ('original_GPU_geometry_and_triangle_visibility', 'actual_hand_raw_and_backend_pixel_attribution',
                 'atomic_voxel_copy_and_residency_ABA', 'actual_bound_GL_uniforms'):
        unresolved(name, 'OPEN: unavailable in observer; metadata/matrix/PNG file does not replace this evidence.')
    return {'phase': phase, 'frame_id': frame['frame_id'], 'checks': checks,
            'failures': [c for c in checks if not c['ok']], 'open': opens,
            'unprojected_near_quad': quad, 'world_intersections': hits,
            'third_player_box_intersection_triangles': player_hits, 'actual_queued_sections': queued}


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for data in iter(lambda: source.read(1024*1024), b''):
            value.update(data)
    return value.hexdigest()


def local_file(base, name):
    if not isinstance(name, str) or not name or Path(name).is_absolute():
        raise ValueError('sidecar must be a relative path')
    path = base/Path(name)
    resolved = path.resolve(strict=True)
    if path.is_symlink() or not resolved.is_relative_to(base.resolve()):
        raise ValueError('sidecar escapes capture directory')
    return resolved


def inspect_png(path, descriptor):
    if path.stat().st_size > 16*1024*1024: raise ValueError('PNG byte bound before read')
    data = path.read_bytes()
    if len(data) > 16*1024*1024 or len(data) != descriptor['bytes'] or data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('PNG signature/byte count/bound mismatch')
    offset, ihdr, idat, ended = 8, None, [], False
    while offset < len(data):
        if offset+12 > len(data): raise ValueError('truncated PNG chunk')
        length = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset+4:offset+8]; end = offset+12+length
        if end > len(data): raise ValueError('PNG chunk outside file')
        payload = data[offset+8:offset+8+length]
        if zlib.crc32(kind+payload) & 0xffffffff != struct.unpack_from('>I', data, offset+8+length)[0]:
            raise ValueError('PNG CRC mismatch')
        if kind == b'IHDR':
            if offset != 8 or ihdr is not None or length != 13: raise ValueError('PNG IHDR order/size')
            ihdr = struct.unpack('>IIBBBBB', payload)
        elif kind == b'IDAT': idat.append(payload)
        elif kind == b'IEND':
            if length or end != len(data): raise ValueError('PNG end/trailing data')
            ended = True
        offset = end
    if not ihdr or not idat or not ended: raise ValueError('PNG incomplete')
    width, height, bits, colour, compression, filtering, interlace = ihdr
    if width != descriptor['width'] or height != descriptor['height'] or not (0 < width <= 4096 and 0 < height <= 4096):
        raise ValueError('PNG dimension mismatch/bound')
    if bits != 8 or colour not in (2, 6) or compression or filtering or interlace:
        raise ValueError('unexpected actual backend PNG layout')
    row_bytes = width*(3 if colour == 2 else 4)+1
    expected = row_bytes*height
    if expected > 64*1024*1024: raise ValueError('decoded PNG size bound')
    inflater = zlib.decompressobj()
    decoded = inflater.decompress(b''.join(idat), expected+1)
    if len(decoded) != expected or not inflater.eof or inflater.unused_data or inflater.unconsumed_tail:
        raise ValueError('PNG deflate byte count/end')
    if any(decoded[y*row_bytes] > 4 for y in range(height)): raise ValueError('PNG invalid row filter')
    return {'path': str(path), 'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data),
            'width': width, 'height': height, 'decoded_scanline_bytes': expected,
            'pixels': 'OPEN_NO_GEOMETRY_OR_HAND_ATTRIBUTION'}


def run_capture(index_path, console_path, require_nominal):
    base = index_path.parent
    tracked = {Path(__file__).resolve(), index_path.resolve(), console_path.resolve()}
    before = {str(p): sha(p) for p in tracked}
    if index_path.stat().st_size > 2*1024*1024 or console_path.stat().st_size > 16*1024*1024:
        raise ValueError('index/console byte bounds')
    index = json.loads(index_path.read_text())
    if index.get('schema') != SCHEMA or index.get('status') != 'CAPTURED_WITH_OPEN_FIELDS' or len(index.get('frames', [])) != 6:
        raise ValueError('CAPTURE_INCOMPLETE: require terminal captured index with six actual frames; collision NOT_MEASURED')
    frame_paths = [local_file(base, name) for name in index['frames']]
    if len(set(frame_paths)) != 6: raise ValueError('duplicate frame sidecars')
    frames = []
    for path in frame_paths:
        if path.stat().st_size > 2*1024*1024: raise ValueError('frame JSON bound')
        tracked.add(path);before[str(path)] = sha(path);frames.append(json.loads(path.read_text()))
    if tuple(f['phase'] for f in frames) != PHASES or len({f['frame_id'] for f in frames}) != 6:
        raise ValueError('actual six phase order/unique frame identity mismatch')
    if any(f.get('schema') != SCHEMA or f.get('status') != 'CAPTURED_WITH_OPEN_GEOMETRY_AND_HAND_ATTRIBUTION' for f in frames):
        raise ValueError('actual frame schema/status mismatch')
    matches = re.findall(r'^\[CAMERA_DIAGNOSTICS\] phase=(\d+) gl_error=(\d+)\s*$', console_path.read_text(), re.MULTILINE)
    expected = [(str(i), '0') for i in range(6)]
    gl_ok = matches == expected
    results, pngs = [], []
    for i, frame in enumerate(frames):
        descriptor = frame['actual_backend_frame'];path = local_file(base, descriptor['file'])
        tracked.add(path);before[str(path)] = sha(path);pngs.append(inspect_png(path, descriptor))
        result = evaluate_frame(frame, require_nominal)
        result['checks'].append({'name': 'actual_external_console_frame_GL0', 'ok': gl_ok,
                                 'details': {'console': str(console_path), 'phase_index': i, 'actual_markers': matches}})
        result['checks'].append({'name': 'actual_camera_aspect_matches_backend_frame',
                                 'ok': abs(frame['camera']['aspect']-descriptor['width']/descriptor['height']) <= 1e-5,
                                 'details': {'camera_aspect': frame['camera']['aspect'], 'PNG': [descriptor['width'], descriptor['height']]}})
        result['failures'] = [c for c in result['checks'] if not c['ok']]
        results.append(result)
    if sum(p.stat().st_size for p in tracked if p != Path(__file__).resolve() and p != console_path.resolve()) > BYTE_LIMIT:
        raise ValueError('capture stored evidence byte bound')
    after = {str(p): sha(p) for p in tracked}
    unchanged = before == after
    failures = [{'phase': r['phase'], **f} for r in results for f in r['failures']]
    if not unchanged: failures.append({'phase': 'packet', 'name': 'source_and_packet_unchanged', 'ok': False})
    nominal = [f['camera'].get('nominal_near') for f in frames]
    if all(type(x) in (int, float) for x in nominal) and max(nominal)-min(nominal) > 1e-6:
        failures.append({'phase': 'packet', 'name': 'nominal_near_identity_same_across_phase_sequence', 'ok': False, 'details': nominal})
    return {'schema': REPORT_SCHEMA, 'status': 'CAMERA_PACKET_FAIL_WITH_OPEN_FIELDS' if failures else 'CAMERA_ENGINEERING_CLEAR_WITH_OPEN_FIELDS',
            'capture': str(index_path), 'console_log': str(console_path), 'tool_sha256': sha(Path(__file__).resolve()),
            'input_count': len(before), 'input_sha256_before': before, 'input_sha256_after': after, 'inputs_unchanged': unchanged,
            'frame_count': len(results), 'measured_check_count': sum(len(r['checks']) for r in results),
            'failure_count': len(failures), 'failures': failures, 'frames': results, 'PNG_identities': pngs,
            'scope': 'Actual captured client matrix/near-plane/resident voxel/visibility metadata and external per-frame GL marker checks. PNG integrity is not pixel attribution.',
            'required_open': ['original_GPU_geometry_visibility', 'actual_UI_hand_raw_and_pixel_attribution', 'atomic_voxel_snapshot_residency_ABA',
                              'actual_GPU_bound_uniforms', 'normal_desktop_input_and_continuous_route'],
            'overall_V07_acceptance': 'OPEN', 'execution': {'normal_builds': 0, 'links': 0, 'GPU_runs': 0, 'UI_actions': 0}}


def self_test():
    # Fixed mathematical references, not Ogre helper or copied camera policy.
    p = [.5, 0, 0, 0, 0, 1, 0, 0, 0, 0, -100.1/99.9, -20/99.9, 0, 0, -1, 0]
    identity = [float(r == c) for r in range(4) for c in range(4)]
    translated = list(identity);translated[3], translated[7], translated[11] = -.5, -200.6, -.5
    expected = [[.2, .1, -.1], [-.2, .1, -.1], [-.2, -.1, -.1], [.2, -.1, -.1]]
    checks = []
    def record(name, ok): checks.append({'name': name, 'ok': bool(ok)})
    record('independent_inverse_identity', matrix_close(mul(p, inverse(p)), identity))
    record('GL_negative_one_fixed_near_corners', corner_set_distance(near_quad(p, identity), expected) < 1e-12)
    moved = [[x+.5, y+200.6, z+.5] for x, y, z in expected]
    record('actual_style_large_world_translation', corner_set_distance(near_quad(p, translated), moved) < 1e-9)
    box_min, box_max = [-.25]*3, [.25]*3
    crossing = [[-2, 0, -2], [2, 0, -2], [0, 0, 2]]
    record('all_corners_outside_interior_crossing_detected', triangle_box(crossing, box_min, box_max))
    record('separated_triangle_rejected', not triangle_box([[x, y+1, z] for x, y, z in crossing], box_min, box_max))
    record('touch_only_not_penetration', not triangle_box([[x, y+.25, z] for x, y, z in crossing], box_min, box_max))
    record('edge_cross_axis_separation', not triangle_box([[.4, 1.1, 0], [1.1, .4, 0], [1.1, 1.1, 0]], [-.5]*3, [.5]*3))
    quad = [[-2, 0, -2], [2, 0, -2], [2, 0, 2], [-2, 0, 2]]
    record('full_two_triangle_quad_interior', len(quad_box(quad, box_min, box_max)) == 2)
    wrong = list(identity);wrong[3] = 2
    record('wrong_WVP_matrix_rejected', not matrix_close(identity, wrong))
    return {'schema': REPORT_SCHEMA, 'status': 'CPU_MATH_CALIBRATION_CLEAR' if all(c['ok'] for c in checks) else 'CPU_MATH_CALIBRATION_FAIL',
            'checks': checks, 'tool_sha256': sha(Path(__file__).resolve()),
            'scope': 'Synthetic independent math calibration only; zero actual-client captures/GPU/UI.', 'actual_client_acceptance': 'NOT_RUN'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--console-log', type=Path)
    parser.add_argument('--require-nominal', action='store_true')
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists(): parser.error('report must be fresh; preserve previous evidence')
    if args.self_test:
        if args.capture or args.console_log: parser.error('self-test is not an actual capture')
        report = self_test();code = 0 if report['status'] == 'CPU_MATH_CALIBRATION_CLEAR' else 1
    else:
        if not args.capture or not args.console_log: parser.error('actual capture requires --capture and original --console-log')
        try:
            report = run_capture(args.capture.resolve(), args.console_log.resolve(), args.require_nominal)
            code = 1 if report['failure_count'] else 0
        except (ValueError, KeyError, TypeError, OSError, ZeroDivisionError) as error:
            report = {'schema': REPORT_SCHEMA, 'status': 'CAMERA_CAPTURE_INCOMPLETE_OR_INPUT_INVALID',
                      'failure': str(error), 'collision_measurement': 'NOT_MEASURED', 'overall_V07_acceptance': 'OPEN',
                      'tool_sha256': sha(Path(__file__).resolve()), 'GPU_runs': 0, 'UI_actions': 0}
            code = 2
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({'status': report['status'], 'exit': code, 'failure_count': report.get('failure_count'),
                      'report': str(args.report.resolve()), 'report_sha256': sha(args.report)}, ensure_ascii=False))
    return code


if __name__ == '__main__':
    sys.exit(main())
