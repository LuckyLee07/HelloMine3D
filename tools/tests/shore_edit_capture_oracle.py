#!/usr/bin/env python3
"""Independent V06c original storage, actual World columns and map PNG audit.

No production pack/builder/map helper is imported or run. Input is an outer
capture.json with SHA256 inventory, six original storage packets, six real
World/map packets and six original PNGs. Storage agreement, displayed map pixels
and the unobserved terrain draw/ordinary-input boundaries stay separate.
"""
import argparse
import array
import datetime
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import shutil
import struct
import sys
import time

AIR, SAND, WATER = 0, 6, 7
PHASES = ('baseline_flat', 'submerged_sand_flat', 'restored_depth_flat',
          'top_sand_flat', 'lowered_water_flat', 'restored_flat')
INVENTORY = (2, 1, 1, 0, 1, 1)
OP_LIMIT, INPUT_LIMIT = 16 * 1024**2, 512 * 1024**2
LAYOUT = ((0, 2, 1, 0, 3), (12, 1, 7, 0, 2), (20, 1, 7, 1, 2),
          (28, 2, 7, 2, 3), (40, 0, 7, 3, 1))
OPEN = ['ordinary_input', 'actual_mouse_selection', 'inner_draw_vao_fetch',
        'terrain_draw_pixel_attribution', 'World_incarnation_ABA',
        'save_reopen_validation', 'atomic_World_column_snapshot',
        'independent_World_surface_for_fine_neighbours_outside_3x3_columns']


class Rejected(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Rejected(message)


def integer(value, low=0, high=2**64 - 1):
    require(type(value) is int and low <= value <= high, 'integer outside bound')
    return value


def finite(value):
    require(type(value) in (float, int) and math.isfinite(value), 'nonfinite numeric fact')
    return value


def digest(data):
    return hashlib.sha256(data).hexdigest()


def same_surface(value, height, block):
    return value == {'known': True, 'height': height, 'block_id': block}


class Audit:
    def __init__(self, capture):
        self.capture_path = Path(capture).resolve(strict=True)
        self.root = self.capture_path.parent
        self.start = time.monotonic()
        self.checks, self.files, self.phase_results = [], {}, []
        self.record = self.load_json(self.capture_path, inventoried=False)
        self.inventory = self.record.get('artifacts', {})
        self.total = 0

    def deadline(self):
        require(time.monotonic() - self.start <= 60, 'oracle exceeded 60-second input/semantic bound')

    def check(self, name, condition, detail=None):
        self.deadline()
        self.checks.append({'name': name, 'status': 'PASS' if condition else 'FAIL',
                            **({'detail': detail} if detail is not None else {})})
        require(condition, name)

    def path(self, relative):
        require(isinstance(relative, str) and len(relative) <= 256, 'invalid input path')
        name = PurePosixPath(relative)
        require(not name.is_absolute() and name.parts and '..' not in name.parts,
                'input paths must remain relative to capture directory')
        path = self.root.joinpath(*name.parts)
        require(not path.is_symlink() and path.is_file() and
                path.resolve().is_relative_to(self.root), 'missing/unsafe capture file')
        return path

    def read(self, path, limit=64 * 1024**2, inventoried=True):
        self.deadline()
        path = Path(path)
        size = path.stat().st_size
        require(0 < size <= limit, 'input file outside size bound: ' + str(path))
        relative = path.relative_to(self.root).as_posix()
        data = path.read_bytes()
        actual = digest(data)
        if inventoried:
            require(relative in self.inventory and self.inventory[relative] == actual,
                    'SHA256 mismatch or uninventoried input: ' + relative)
        self.files[relative] = {'bytes': size, 'sha256': actual}
        return data

    def load_json(self, path, inventoried=True):
        data = self.read(path, 1024**2, inventoried)
        return json.loads(data, parse_constant=lambda x: (_ for _ in ()).throw(Rejected('JSON nonfinite: ' + x)))

    def verify_inventory(self):
        self.check('capture/completed-diagnostic', self.record.get('result') == 'CAPTURED' and
                   self.record.get('normal_input') is False and
                   self.record.get('diagnostic_fixture') == 'shore-edit-production-world-map' and
                   self.record.get('evidence_type') == 'DEVELOPER_DIAGNOSTIC' and
                   self.record.get('window_mode') == 'hidden' and self.record.get('launch_method') == 'direct' and
                   self.record.get('render_readback') is True)
        require(type(self.inventory) is dict and 19 <= len(self.inventory) <= 256,
                'capture inventory count outside bound')
        for relative, expected in self.inventory.items():
            require(isinstance(expected, str) and len(expected) == 64 and
                    all(c in '0123456789abcdef' for c in expected), 'invalid SHA256 fact')
            path = self.path(relative)
            self.total += path.stat().st_size
            require(self.total <= INPUT_LIMIT, 'capture exceeds512MiB total input bound')
            self.read(path)
        self.check('capture/all-inventoried-file-sha256', True, {'files': len(self.inventory), 'bytes': self.total})
        session = self.load_json(self.path('shore-edit/index.json'))
        self.check('capture/six-original-phase-images', session.get('schema') == 'hellomine3d-shore-edit-capture-v1' and
                   session.get('status') == 'CAPTURED' and session.get('normal_input') is False and
                   session.get('restored_save_succeeded') is True and
                   session.get('frames') == [f'phase-{i:03d}.png' for i in range(6)])

    def columns(self, facts, baseline=None):
        target = facts['target']
        require(type(target) is list and len(target) == 3, 'invalid target')
        x, y, z = (integer(c, -(2**31), 2**31 - 1) for c in target)
        require(y == 64 and x % 4 == 0 and z % 4 == 0, 'canonical4 Water64 target required')
        columns = facts['column']
        require(type(columns) is list and len(columns) == 9, 'exact3x3 World columns required')
        out = {}
        for column in columns:
            cx, cz = integer(column['x'], x - 1, x + 1), integer(column['z'], z - 1, z + 1)
            require((cx, cz) not in out and len(column['blocks']) == 10, 'duplicate/missing World column')
            values = {}
            for by, block, meta in column['blocks']:
                by, block, meta = integer(by, 56, 65), integer(block, 0, 32), integer(meta, 0, 255)
                require(by not in values, 'duplicate World column y')
                values[by] = (block, meta)
            require(set(values) == set(range(56, 66)), 'incomplete World56..65 column')
            out[(cx, cz)] = values
        require(set(out) == {(x + dx, z + dz) for dx in (-1, 0, 1) for dz in (-1, 0, 1)}, 'World column envelope differs')
        if baseline is not None:
            self.check(f'phase{facts["phase"]}/neighbour-World-columns-unchanged',
                       all(values == baseline[key] for key, values in out.items() if key != (x, z)))
        return out

    def expected_corners(self, columns, target, height):
        x, _, z = target
        def depth(cx, cz):
            count = 0
            for y in range(height, height - 8, -1):
                if columns[(cx, cz)][y][0] != WATER:
                    break
                count += 1
            return count
        return {(cx, cz): sum(depth(cx + dx, cz + dz) for dx in (-1, 0) for dz in (-1, 0)) / 4
                for cx in (x, x + 1) for cz in (z, z + 1)}

    def storage(self, packet, facts, columns):
        phase = facts['phase']
        require(packet.get('schema') == 'hellomine3d-shore-edit-original-storage-v1' and
                packet.get('status') == 'CAPTURED' and packet.get('phase') == PHASES[phase], 'invalid original storage phase')
        require(packet['frame_id'] == facts['ui_frame'] and packet['gl_errors'] == [] and
                packet.get('ordinary_input') == 'NOT_RUN', 'storage frame/errors/scope differ')
        mode = packet['mode']
        require(mode in ('standard', 'compatibility'), 'actual frozen material mode required')
        require(('visualdetail ' + mode) in self.record['settings'].splitlines(),
                'original frozen material mode differs from requested capture profile')
        operations = packet['operations']
        require(type(operations) is list and 1 <= len(operations) <= 8, 'original operation count outside bound')
        target = facts['target']; x, _, z = target
        height = 63 if phase == 4 else 64
        expected = self.expected_corners(columns, target, height)
        seen_objects, part_revisions, corners, water_area, sand_covers = set(), {}, {}, 0., []
        summary = []
        raw_written = 0
        for oi, op in enumerate(operations):
            self.deadline()
            prefix = f'phase{phase}/op{oi}'
            identity = integer(op['object_id'], 1), integer(op['upload_serial'], 1)
            require(identity not in seen_objects, 'duplicate original object lifetime')
            seen_objects.add(identity)
            origin = op['origin']; require(len(origin) == 3, 'invalid origin')
            origin = [integer(c, -(2**31), 2**31 - 1) for c in origin]
            require(op['owner_key'] == '_'.join(map(str, origin)), 'renderer owner key differs from constructor origin')
            layer = op['layer']; require(layer in ('water', 'solid'), 'invalid layer')
            require(op['material_name'] == ('HelloMine3D/Water' if layer == 'water' else 'HelloMine3D/Terrain'), 'material owner mismatch')
            require(op['operation_type'] == 4 and op['instances'] == 1 and
                    op['vertex_start'] == op['index_start'] == 0 and op['index_type'] == 'u32', 'original triangle operation differs')
            vc, ic = integer(op['vertex_count'], 1, OP_LIMIT // 44), integer(op['index_count'], 3, OP_LIMIT // 4)
            require(ic % 3 == 0 and len(op['vertex_buffers']) == 1, 'invalid original triangle/source count')
            vb, ib = op['vertex_buffers'][0], op['index_buffer']
            require(vb['source'] == 0 and vb['stride'] == 44 and vb['vertices'] == vc and
                    vb['bytes'] == vc * 44 and ib['bytes'] == ic * 4 and
                    0 < vb['bytes'] + ib['bytes'] <= OP_LIMIT, 'original44B/u32 byte extent differs')
            integer(vb['gl_id'], 1, 2**32 - 1); integer(ib['gl_id'], 1, 2**32 - 1)
            require(len(op['elements']) == 5 and tuple((e['offset'], e['type'], e['semantic'],
                    e['semantic_index'], e['components']) for e in op['elements']) == LAYOUT and
                    all(e['source'] == 0 and e['bytes'] == e['components'] * 4 for e in op['elements']), 'original declaration differs from44B offsets')
            self.check(prefix + '/declaration-and-extents', True)
            raw = self.read(self.path('shore-edit/' + vb['file']), OP_LIMIT)
            inds = self.read(self.path('shore-edit/' + ib['file']), OP_LIMIT)
            cpu = self.read(self.path('shore-edit/' + vb['cpu_file']), OP_LIMIT)
            cpu_inds = self.read(self.path('shore-edit/' + ib['cpu_file']), OP_LIMIT)
            raw_written += len(raw) + len(inds) + len(cpu) + len(cpu_inds)
            require(len(raw) == vc * 44 and len(inds) == ic * 4, 'actual raw byte length differs')
            self.check(prefix + '/original-native-uploader-CPU-bytes', raw == cpu and inds == cpu_inds and op['native_cpu_bytes_equal'] is True)
            self.check(prefix + '/finite-raw44B-components', all(math.isfinite(c) for row in struct.iter_unpack('<11f', raw) for c in row))
            indices = array.array('I'); indices.frombytes(inds)
            if sys.byteorder != 'little': indices.byteswap()
            self.check(prefix + '/native-u32-index-range', all(v < vc for v in indices))
            parts = op['parts']; require(1 <= len(parts) <= 4, 'invalid original part count')
            previous_y = origin[1] - 1
            for part in parts:
                sec = tuple(part['section']); require(len(sec) == 3, 'invalid original part coordinate')
                require(sec[0] == origin[0] and sec[2] == origin[2] and
                        previous_y < sec[1] < origin[1] + 4, 'original ordered batch part differs')
                previous_y = sec[1]
                require(part['live_known'] is True and part['gpu_resident'] is True and
                        part['still_cpu_ready'] is False and part['live_revision'] == part['upload_revision'] and
                        part.get('incarnation') is None and part.get('incarnation_known') is False, 'current upload/residency or unknown incarnation scope differs')
                rev = integer(part['live_revision'], 1, 2**32 - 1)
                require(sec not in part_revisions or part_revisions[sec] == rev, 'same section has conflicting uploaded revisions')
                part_revisions[sec] = rev
            require(layer != 'water' or len(parts) == 1 and tuple(parts[0]['section']) == tuple(origin), 'Water was batched or lost its section owner')
            self.check(prefix + '/parts-current-resident', True)
            maximum_local_y = (max(p['section'][1] for p in parts) - origin[1] + 1) * 16
            require(op['state_restored'] is True and op['context_before'] == op['context_after'], 'COPY_READ actual context was not restored')
            transform = op['node_world_row_major']
            wanted = [1, 0, 0, origin[0] * 16, 0, 1, 0, origin[1] * 16,
                      0, 0, 1, origin[2] * 16, 0, 0, 0, 1]
            require(len(transform) == 16 and all(finite(v) == w for v, w in zip(transform, wanted)), 'original node transform differs from origin')
            candidate = {}
            sand_plane = 64 if phase == 1 else 65
            plane = height + 1 if layer == 'water' else sand_plane
            for vi, row in enumerate(struct.iter_unpack('<11f', raw)):
                px, py, pz = row[0] + origin[0] * 16, row[1] + origin[1] * 16, row[2] + origin[2] * 16
                require(-.001 <= row[0] <= 16.001 and -.001 <= row[2] <= 16.001 and
                        -.001 <= row[1] <= maximum_local_y + .001 and 0 <= row[7] <= 1 and
                        0 <= row[9] <= 2 and -1 <= row[8] <= 1, 'original vertex attribute bounds differ')
                if abs(py - plane) <= 2e-5 and (layer == 'solid' or
                        x - 2e-5 <= px <= x + 1 + 2e-5 and z - 2e-5 <= pz <= z + 1 + 2e-5):
                    candidate[vi] = (px, py, pz, row)
            for ti in range(0, ic, 3):
                ids = indices[ti:ti + 3]
                if not all(v in candidate for v in ids): continue
                triangle = [candidate[v] for v in ids]
                a, b, c = triangle
                normal_y = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2])
                if normal_y <= 0: continue
                if layer == 'water':
                    water_area += normal_y / 2
                    for px, _, pz, row in triangle:
                        key = (round(px), round(pz))
                        require(key in expected and abs(px - key[0]) <= 2e-5 and abs(pz - key[1]) <= 2e-5, 'target water top has unexpected corner topology')
                        require(0 <= row[5] <= 8 and 0 <= row[6] <= 1, 'native water uv1 outside depth/shore range')
                        corners.setdefault(key, []).append(row[5])
                elif all(math.floor(p[3][3] * 16) == 7 and math.floor(p[3][4] * 16) == 0 for p in triangle):
                    # A greedy Sand top may extend past the source cube. Point
                    # containment supports its actual triangle without assuming
                    # one cube has four contiguous vertices or a private mesh.
                    signs = []
                    for v, w in zip(triangle, triangle[1:] + triangle[:1]):
                        signs.append((w[0] - v[0]) * (z + .5 - v[2]) -
                                     (w[2] - v[2]) * (x + .5 - v[0]))
                    if min(signs) >= -2e-5 or max(signs) <= 2e-5:
                        sand_covers.append({'operation': oi, 'triangle_first_index': ti, 'plane_y': plane})
            summary.append({'layer': layer, 'origin': origin, 'parts': len(parts), 'raw_bytes': len(raw) + len(inds),
                            'upload_serial': op['upload_serial']})
        top_sec = (x // 16, 4, z // 16); lower_sec = (x // 16, 3, z // 16)
        self.check(f'phase{phase}/both-production-vertical-section-revisions', top_sec in part_revisions and lower_sec in part_revisions)
        if phase == 3:
            self.check('phase3/no-water-top-over-actual-Sand64', water_area == 0 and not corners)
        else:
            self.check(f'phase{phase}/native-water-top-triangles', abs(water_area - 1) <= 2e-5 and set(corners) == set(expected),
                       {'triangle_area': water_area})
            self.check(f'phase{phase}/actual-World-four-corner-water-depth',
                       all(abs(v - expected[k]) <= 2e-5 for k, values in corners.items() for v in values),
                       {'expected_corner_depths': [{'xz': list(k), 'depth': v} for k, v in expected.items()],
                        'native_corner_depths': [{'xz': list(k), 'depths': sorted(set(v))} for k, v in corners.items()]})
        if phase in (1, 3):
            self.check(f'phase{phase}/original-Sand-top-triangle-support', bool(sand_covers), sand_covers[:8])
        return {'mode': mode, 'expected_depth': sorted(expected.items()), 'actual_corner_depth': corners,
                'part_revisions': part_revisions, 'raw_written': raw_written, 'operations': summary}

    def map_and_pixels(self, facts, png, baseline=None):
        from PIL import Image
        phase = facts['phase']; target = facts['target']; x, _, z = target
        expected_height, expected_block = (63 if phase == 4 else 64), (SAND if phase == 3 else WATER)
        require(facts['backend_submitted'] is True and integer(facts['backend_vertex_count'], 1) and
                integer(facts['backend_index_count'], 1), 'actual UI backend submission required')
        cells = {}
        for name, cx, cz in [('target', x, z), ('west', x - 2, z), ('north', x, z - 2)]:
            cell = facts['fine_' + name]
            require(cell['available'] is True and cell['world_x'] == cx and cell['world_z'] == cz and
                    cell['step'] == 2 and cell['surface']['known'] is True, 'actual fine-history source missing/wrong coordinate')
            integer(cell['surface']['height'], 0, 383); integer(cell['surface']['block_id'], 0, 32)
            cells[name] = cell['surface']
        self.check(f'phase{phase}/actual-fine-surface', same_surface(cells['target'], expected_height, expected_block))
        resolution = facts['production_resolution']
        self.check(f'phase{phase}/production-Flat-resolution', resolution['available'] is True and
                   resolution['world_x'] == x and resolution['world_z'] == z and resolution['step'] == 2 and
                   resolution['surface'] == cells['target'])
        live = facts['flat_live_target']
        require(live['available'] is True and live['step'] == 2 and
                live['world_x'] == x and live['world_z'] == z, 'actual RD1 Flat2m target ownership missing')
        if phase:
            hud = facts['hud_before_flat']; edit = integer(facts['edit_ui_frame'])
            self.check(f'phase{phase}/new-HUD256-reply-before-Flat', hud['view'] == 0 and hud['step'] == 4 and
                       integer(hud['target_observed_frame'], 1) > edit and
                       hud['target_observed_frame'] <= integer(hud['frame'], 1) < integer(facts['ui_frame'], 1) and
                       same_surface(hud['sample'], expected_height, expected_block))
            hf = hud['fine_target']
            self.check(f'phase{phase}/HUD-reply-preserved-in-fine-history', hf['available'] is True and
                       hf['world_x'] == x and hf['world_z'] == z and hf['step'] == 2 and
                       hf['surface'] == cells['target'])
            self.check(f'phase{phase}/Flat-did-not-query-target-in-captured-frame', facts['flat_target_queried_this_frame'] is False)
        if baseline:
            self.check(f'phase{phase}/fine-West-North-unchanged', all(cells[k] == baseline['cells'][k] for k in ('west', 'north')))
        data = self.read(png, 32 * 1024**2)
        require(data.startswith(b'\x89PNG\r\n\x1a\n') and len(data) > 33, 'original frame is not PNG')
        width, height = struct.unpack_from('>II', data, 16)
        scale = facts['framebuffer_scale']
        require(len(scale) == 2 and all(finite(v) in (1, 2) for v in scale), 'actual framebuffer scale differs')
        require((width, height) == (int(1280 * scale[0]), int(720 * scale[1])), 'original PNG size/actual UI framebuffer scale differs')
        require((width, height) == tuple(v * self.record['expected_pixel_ratio'] for v in self.record['window_size_points']), 'capture declared/actual PNG resolution differs')
        with Image.open(io.BytesIO(data)) as original:
            require(original.format == 'PNG' and original.size == (width, height), 'Pillow original PNG differs')
            image = original.convert('RGB')
        pixel_results = {}
        for name in ('target', 'west', 'north'):
            draw = facts['draw_' + name]
            require(draw['submitted'] is True and draw['layer'] == 2 and draw['step'] == 2 and draw['surface'] == cells[name], 'Flat display did not use actual fine source')
            vb, ve = draw['vertices']; ib, ie = draw['indices']
            require(0 <= vb < ve <= facts['backend_vertex_count'] and ve - vb == 4 and
                    0 <= ib < ie <= facts['backend_index_count'] and ie - ib == 6, 'actual submitted UI rectangle ranges differ')
            colour = integer(draw['colour'], 0, 2**32 - 1)
            rgb = [colour >> shift & 255 for shift in (0, 8, 16)]
            require(colour >> 24 == 255, 'map rectangle is not opaque')
            left, top, right, bottom = map(finite, draw['rect'])
            require(0 <= left < right <= 1280 and 0 <= top < bottom <= 720, 'map rect outside original window')
            # Pixel centres in the central half are separated from map-cell
            # seams and antialias edges. Do not search elsewhere for a colour.
            x0, x1 = (left + (right - left) * .25) * scale[0], (right - (right - left) * .25) * scale[0]
            y0, y1 = (top + (bottom - top) * .25) * scale[1], (bottom - (bottom - top) * .25) * scale[1]
            points = [(px, py) for py in range(math.ceil(y0 - .5), math.floor(y1 - .5) + 1)
                      for px in range(math.ceil(x0 - .5), math.floor(x1 - .5) + 1)]
            require(0 < len(points) <= 4096, 'central map rectangle has no bounded actual pixel sample')
            pixels = [image.getpixel(p) for p in points]
            errors = [max(abs(a - b) for a, b in zip(pixel, rgb)) for pixel in pixels]
            self.check(f'phase{phase}/original-PNG-{name}-central-rect-colour', max(errors) <= 3,
                       {'expected_rgb': rgb, 'max_channel_error': max(errors), 'samples': len(points),
                        'rect_points': draw['rect'], 'framebuffer_scale': scale})
            med = [sorted(p[c] for p in pixels)[len(pixels) // 2] for c in range(3)]
            pixel_results[name] = {'rgb': rgb, 'pixel_median': med}
        # Target colour identity is independently tied to its real block and
        # actual recorded neighbouring heights, rather than trusting draw.colour.
        slope = sum(cells[k]['height'] - expected_height for k in ('west', 'north'))
        shade = min(1.18, max(.72, 1 - slope * .055))
        base = (204, 186, 132) if expected_block == SAND else (58, 133, 158)
        expected_rgb = [min(255, max(0, int(c * shade))) for c in base]
        self.check(f'phase{phase}/displayed-colour-matches-actual-material-height',
                   all(abs(a - b) <= 1 for a, b in zip(expected_rgb, pixel_results['target']['rgb'])),
                   {'actual_World_material': expected_block, 'actual_height': expected_height, 'slope': slope, 'expected_rgb': expected_rgb})
        if baseline:
            self.check(f'phase{phase}/West-North-pixels-unchanged', all(
                pixel_results[k]['rgb'] == baseline['pixels'][k]['rgb'] and
                max(abs(a - b) for a, b in zip(pixel_results[k]['pixel_median'], baseline['pixels'][k]['pixel_median'])) <= 3
                for k in ('west', 'north')))
        return {'cells': cells, 'pixels': pixel_results, 'png': png.relative_to(self.root).as_posix()}

    def run(self):
        self.verify_inventory()
        baseline_columns, baseline_map, modes, frames, target = None, None, [], [], None
        expected_phase_blocks = ((WATER, WATER), (WATER, SAND), (WATER, WATER),
                                 (SAND, WATER), (AIR, WATER), (WATER, WATER))
        storage_results, map_results, write_bytes = [], [], 0
        for phase in range(6):
            packet = self.load_json(self.path(f'shore-edit/phase-{phase:03d}.json'))
            facts = self.load_json(self.path(f'shore-edit/phase-{phase:03d}-world-map.json'))
            require(facts['schema'] == 'hellomine3d-shore-edit-world-map-v1' and facts['phase'] == phase and
                    facts['normal_input'] is False, 'invalid actual World/map packet')
            self.check(f'phase{phase}/production-inventory-consumption-drop', facts['inventory_sand'] == INVENTORY[phase] and
                       facts['held_amount'] == INVENTORY[phase] and facts['held_material'] == (SAND if INVENTORY[phase] else 0))
            if target is None: target = facts['target']
            require(facts['target'] == target and self.record['shore_edit_target'] == target, 'target changed or differs from capture')
            columns = self.columns(facts, baseline_columns)
            x, _, z = target
            self.check(f'phase{phase}/actual-World-six-phase-block-semantics',
                       columns[(x, z)][64][0] == expected_phase_blocks[phase][0] and
                       columns[(x, z)][63][0] == expected_phase_blocks[phase][1] and columns[(x, z)][65][0] == AIR)
            if baseline_columns is None:
                baseline_columns = columns
                self.check('World/baseline-real-resident-water-depth-at-least2',
                           columns[(x, z)][64] == (WATER, 0) and columns[(x, z)][63] == (WATER, 0))
            else:
                expected_column = dict(baseline_columns[(x, z)])
                if phase == 1: expected_column[63] = (SAND, 0)
                elif phase == 3: expected_column[64] = (SAND, 0)
                elif phase == 4: expected_column[64] = (AIR, 0)
                self.check(f'phase{phase}/only-intended-World-cell-change-or-exact-restore', columns[(x, z)] == expected_column)
            storage = self.storage(packet, facts, columns)
            maps = self.map_and_pixels(facts, self.path(f'shore-edit/phase-{phase:03d}.png'), baseline_map)
            if baseline_map is None: baseline_map = maps
            storage_results.append(storage); map_results.append(maps); modes.append(storage['mode']); frames.append(facts['ui_frame'])
            write_bytes += storage['raw_written']
            self.phase_results.append({'phase': phase, 'name': PHASES[phase], 'frame': facts['ui_frame'],
                'inventory_sand': facts['inventory_sand'], 'surface': maps['cells']['target'],
                'target_pixel': maps['pixels']['target'], 'operations': storage['operations']})
        self.check('sequence/strictly-increasing-actual-frame-and-one-mode', all(a < b for a, b in zip(frames, frames[1:])) and len(set(modes)) == 1)
        edited_sections = ((target[0] // 16, 3, target[2] // 16), (target[0] // 16, 4, target[2] // 16))
        self.check('sequence/new-current-upload-revision-after-each-edit-and-restore', all(
            before['part_revisions'][section] < after['part_revisions'][section]
            for before, after in zip(storage_results, storage_results[1:]) for section in edited_sections))
        self.check('sequence/observer-raw-write-budget', write_bytes <= 256 * 1024**2)
        base_depth = storage_results[0]['expected_depth']
        self.check('sequence/submerged-edit-changes-independent-four-corner-depth', base_depth != storage_results[1]['expected_depth'])
        self.check('sequence/depth-restored-exactly', storage_results[2]['expected_depth'] == base_depth and storage_results[5]['expected_depth'] == base_depth)
        def target_rgb(i): return map_results[i]['pixels']['target']['pixel_median']
        self.check('sequence/actual-map-target-pixels-change-and-restore',
                   max(abs(a - b) for a, b in zip(target_rgb(0), target_rgb(3))) >= 20 and
                   max(abs(a - b) for a, b in zip(target_rgb(0), target_rgb(5))) <= 3 and
                   max(abs(a - b) for a, b in zip(target_rgb(0), target_rgb(2))) <= 3)
        if map_results[0]['pixels']['target']['rgb'] != map_results[4]['pixels']['target']['rgb']:
            self.check('sequence/visible-lowered-Water-height-colour-change',
                       max(abs(a - b) for a, b in zip(target_rgb(0), target_rgb(4))) > 0)

    def report(self, error=None):
        return {'schema': 'hellomine3d-shore-edit-independent-oracle-v1',
                'status': 'FAIL' if error else 'PASS_SCOPED_STORAGE_WORLD_MAP_PIXELS',
                'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'capture': str(self.capture_path), 'elapsed_seconds': time.monotonic() - self.start,
                'checks': self.checks, 'phases': self.phase_results, 'input_files': self.files,
                'scope': '44B/u32 original storage and current uploaded revision facts; independent actual World depth; real HUD-to-fine/Flat resolution facts and corresponding original map PNG pixels',
                'open': OPEN, **({'error': str(error)} if error else {})}


def audit_capture(capture):
    audit = None
    try:
        audit = Audit(capture); audit.run(); return audit.report()
    except Exception as error:
        if audit: return audit.report(error)
        return {'schema': 'hellomine3d-shore-edit-independent-oracle-v1', 'status': 'FAIL',
                'capture': str(capture), 'checks': [], 'error': str(error), 'open': OPEN}


def fresh_json(path, result):
    path = Path(path)
    require(not path.exists() and not path.is_symlink(), 'output must be new; retain previous failure')
    path.parent.mkdir(parents=True, exist_ok=True)
    data = json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + '\n'
    require(len(data.encode()) <= 1024**2, 'oracle report exceeds1MiB bound')
    path.write_text(data)


def fault_suite(capture, output):
    """Actual copies with refreshed SHA inventories, never mutate originals."""
    original = Path(capture).resolve(strict=True); root = original.parent
    source = json.loads(original.read_text()); artifacts = source['artifacts']
    destination = Path(output)
    require(not destination.exists() and not destination.is_symlink(), 'fault-suite directory must be new')
    require(sum((root / p).stat().st_size for p in artifacts) <= 96 * 1024**2,
            'five fault copies require original artifacts within96MiB')
    destination.mkdir(parents=True)
    results = []
    for fault in ('NaN44B', 'stride32', 'OOBindex', 'staleUV', 'missingHUDfine'):
        clone = destination / fault; clone.mkdir()
        for relative in artifacts:
            path = PurePosixPath(relative)
            require(not path.is_absolute() and '..' not in path.parts, 'unsafe fault input path')
            target = clone.joinpath(*path.parts); target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / relative, target)
        record = json.loads(original.read_text()); changed = []
        if fault == 'missingHUDfine':
            path = clone / 'shore-edit/phase-001-world-map.json'; facts = json.loads(path.read_text())
            facts['hud_before_flat']['fine_target']['available'] = False
            path.write_text(json.dumps(facts) + '\n'); changed.append(path)
        else:
            phase = 1 if fault == 'staleUV' else 0
            path = clone / f'shore-edit/phase-{phase:03d}.json'; packet = json.loads(path.read_text())
            op = next(o for o in packet['operations'] if o['layer'] == 'water' and o['origin'][1] == 4)
            vb, ib = op['vertex_buffers'][0], op['index_buffer']
            if fault == 'stride32':
                vb['stride'] = 32
                path.write_text(json.dumps(packet) + '\n'); changed.append(path)
            elif fault == 'OOBindex':
                for field in ('file', 'cpu_file'):
                    raw_path = clone / 'shore-edit' / ib[field]; data = bytearray(raw_path.read_bytes())
                    struct.pack_into('<I', data, 0, op['vertex_count']); raw_path.write_bytes(data); changed.append(raw_path)
            else:
                target = record['shore_edit_target']; x, _, z = target
                raw_path = clone / 'shore-edit' / vb['file']; data = raw_path.read_bytes()
                choices = [i for i, row in enumerate(struct.iter_unpack('<11f', data)) if
                           abs(row[0] + op['origin'][0] * 16 - x) < 2e-5 and
                           abs(row[1] + op['origin'][1] * 16 - 65) < 2e-5 and
                           abs(row[2] + op['origin'][2] * 16 - z) < 2e-5]
                require(choices, 'fault could not find actual selected Water top corner')
                baseline_op = None
                if fault == 'staleUV':
                    base = json.loads((clone / 'shore-edit/phase-000.json').read_text())
                    baseline_op = next(o for o in base['operations'] if o['layer'] == 'water' and o['origin'] == op['origin'])
                    base_data = (clone / 'shore-edit' / baseline_op['vertex_buffers'][0]['file']).read_bytes()
                    baseline_values = [row[5] for row in struct.iter_unpack('<11f', base_data) if
                                       abs(row[0] + op['origin'][0] * 16 - x) < 2e-5 and
                                       abs(row[1] + op['origin'][1] * 16 - 65) < 2e-5 and
                                       abs(row[2] + op['origin'][2] * 16 - z) < 2e-5]
                    require(baseline_values, 'no actual baseline corner for staleUV copy')
                    value = baseline_values[0]
                else: value = float('nan')
                for field in ('file', 'cpu_file'):
                    raw_path = clone / 'shore-edit' / vb[field]; mutated = bytearray(raw_path.read_bytes())
                    for vi in choices: struct.pack_into('<f', mutated, vi * 44 + 20, value)
                    raw_path.write_bytes(mutated); changed.append(raw_path)
        for path in changed:
            record['artifacts'][path.relative_to(clone).as_posix()] = digest(path.read_bytes())
        clone_capture = clone / 'capture.json'; clone_capture.write_text(json.dumps(record, indent=2) + '\n')
        report = audit_capture(clone_capture)
        rejected_semantic = report['status'] == 'FAIL' and any(c['name'] == 'capture/all-inventoried-file-sha256' and
                             c['status'] == 'PASS' for c in report['checks']) and 'SHA256' not in report.get('error', '')
        fresh_json(clone / 'oracle.json', report)
        results.append({'fault': fault, 'status': 'REJECTED_AFTER_REFRESHED_HASH' if rejected_semantic else 'FAIL',
                        'changed_files': [p.relative_to(clone).as_posix() for p in changed],
                        'oracle_error': report.get('error'), 'report': str(clone / 'oracle.json')})
    result = {'schema': 'hellomine3d-shore-edit-independent-faults-v1',
              'status': 'PASS' if all(r['status'] == 'REJECTED_AFTER_REFRESHED_HASH' for r in results) else 'FAIL',
              'source_capture': str(original), 'faults': results}
    fresh_json(destination / 'faults.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', action='append', required=True, type=Path, help='Actual outer capture.json (one or two modes)')
    parser.add_argument('--output', required=True, type=Path, help='New bounded JSON report')
    parser.add_argument('--fault-suite', type=Path, help='Optional new directory for five actual raw/metadata fault copies')
    args = parser.parse_args()
    require(1 <= len(args.capture) <= 2, 'one or two actual capture sessions required')
    reports = [audit_capture(capture) for capture in args.capture]
    ok = all(report['status'] == 'PASS_SCOPED_STORAGE_WORLD_MAP_PIXELS' for report in reports)
    faults = None
    if args.fault_suite and ok:
        try:
            faults = fault_suite(args.capture[0], args.fault_suite); ok &= faults['status'] == 'PASS'
        except Exception as error:
            faults = {'status': 'FAIL', 'error': str(error)}; ok = False
    fresh_json(args.output, {'schema': 'hellomine3d-shore-edit-independent-oracle-bundle-v1',
                            'status': 'PASS_SCOPED_STORAGE_WORLD_MAP_PIXELS' if ok else 'FAIL',
                            'sessions': reports, 'faults': faults, 'open': OPEN})
    print(args.output)
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
