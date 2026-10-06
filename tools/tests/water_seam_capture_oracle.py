#!/usr/bin/env python3
"""Independent, bounded consumer for the natural Z=-224 water seam session.

Reads only an inventoried capture, native draw facts, three original raw
checkpoints and lossless framebuffer PNGs. No production shader, water, mesh,
Ogre or prior shore-edit audit helper is imported or executed. Projecting the
original indexed surface with actual native matrices is an input/projection
check; it is not a CPU simulation of shader displacement or GPU output.
Shared-corner depth is independently counted from the captured resident World
columns; equal native/CPU bytes or equal inputs across owners do not establish
that the depth input is correct.
"""
import argparse
import array
from collections import Counter
import datetime
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import struct
import sys
import time
import zlib


SESSION_LIMIT = 256 * 1024**2
OP_LIMIT = 16 * 1024**2
MAX_FRAMES = 480
OWNERS = {(13, 4, -14), (13, 4, -15)}
GL_FLOAT, GL_FLOAT_VEC3, GL_FLOAT_MAT4 = 5126, 35665, 35676
LAYOUT = ((0, 2, 1, 0, 3), (12, 1, 7, 0, 2), (20, 1, 7, 1, 2),
          (28, 2, 7, 2, 3), (40, 0, 7, 3, 1))
INPUTS = {'vertex': (0, 3, 1, 0, 35666),
          'uv0': (12, 2, 7, 0, 35664),
          'uv1': (20, 2, 7, 1, 35664),
          'uv2': (28, 3, 7, 2, GL_FLOAT_VEC3)}
BOUNDARIES = {
    'visual_no_holes_or_objectionable_sparkle': 'OPEN_ORIGINAL_IMAGE_REVIEW_REQUIRED',
    'shader_displaced_position_or_per_primitive_pixel_attribution': 'OPEN_NOT_OBSERVED',
    'World_incarnation_ABA': 'OPEN_PUBLIC_ENDPOINTS_HAVE_NO_INCARNATION',
    'atomic_World_column_copy': 'OPEN_NONATOMIC_PUBLIC_READS',
    'ordinary_input': 'NOT_RUN',
    'all_four_water_families_near_far_day_dusk': 'OPEN_OUTSIDE_THIS_ONE_NATURAL_RIVER_SEAM',
    'codec_internal_live_memory': 'OPEN_NOT_EXPOSED',
    'independent_raw_decode_every_intermediate_frame': 'OPEN_ONLY_THREE_RAW_CHECKPOINTS',
}


class Rejected(ValueError):
    pass


class ObservedOpen(ValueError):
    """A genuine observation is insufficient, rather than structurally false."""
    pass


def require(ok, reason):
    if not ok:
        raise Rejected(reason)


def integer(value, low=0, high=2**64 - 1):
    require(type(value) is int and low <= value <= high, 'integer outside bound')
    return value


def finite(value):
    require(type(value) in (int, float) and math.isfinite(value), 'nonfinite numeric fact')
    return value


def vector(value, count):
    require(type(value) is list and len(value) == count, 'numeric vector extent differs')
    return tuple(finite(x) for x in value)


def unique_object(items):
    result = {}
    for key, value in items:
        require(key not in result, 'duplicate JSON object key: ' + key)
        result[key] = value
    return result


def sha(data):
    return hashlib.sha256(data).hexdigest()


def close(a, b, tolerance=2e-5):
    return abs(a - b) <= tolerance


def matvec(column_major, value):
    return tuple(sum(column_major[c * 4 + r] * value[c] for c in range(4)) for r in range(4))


def quantile(histogram, fraction):
    count = sum(histogram.values())
    rank = max(1, math.ceil(count * fraction))
    seen = 0
    for value in sorted(histogram):
        seen += histogram[value]
        if seen >= rank:
            return value
    return 0


class PNG:
    """Strict 8-bit noninterlaced RGB/RGBA PNG reader, using only stdlib."""
    def __init__(self, data):
        require(data.startswith(b'\x89PNG\r\n\x1a\n'), 'missing PNG signature')
        position, compressed, header, ended = 8, bytearray(), None, False
        idat_seen, idat_closed = False, False
        while position < len(data):
            require(position + 12 <= len(data), 'truncated PNG chunk')
            size = struct.unpack_from('>I', data, position)[0]
            kind = data[position + 4:position + 8]
            require(size <= 64 * 1024**2 and position + size + 12 <= len(data), 'PNG chunk extent differs')
            body = data[position + 8:position + 8 + size]
            crc = struct.unpack_from('>I', data, position + 8 + size)[0]
            require(zlib.crc32(kind + body) & 0xffffffff == crc, 'PNG chunk CRC mismatch')
            require(all(65 <= x <= 90 or 97 <= x <= 122 for x in kind), 'invalid PNG chunk type')
            require(65 <= kind[2] <= 90, 'PNG reserved chunk bit differs')
            if header is None:
                require(kind == b'IHDR' and size == 13, 'PNG must begin with one IHDR')
                header = struct.unpack('>IIBBBBB', body)
                self.width, self.height, bits, colour, compression, filtering, interlace = header
                require(1 <= self.width <= 8192 and 1 <= self.height <= 8192 and bits == 8 and
                        colour in (2, 6) and compression == filtering == interlace == 0,
                        'unsupported actual PNG dimensions, colour or codec')
                self.channels = 3 if colour == 2 else 4
                self.colour_type = colour
            elif kind == b'IHDR':
                raise Rejected('duplicate PNG IHDR')
            elif kind == b'IDAT':
                require(not idat_closed, 'noncontiguous PNG IDAT sequence')
                idat_seen = True
                compressed.extend(body)
            elif kind == b'IEND':
                require(size == 0 and idat_seen, 'invalid PNG IEND or missing image data')
                ended = True
                position += size + 12
                break
            else:
                if idat_seen:
                    idat_closed = True
                require(kind[0] >= 97 or kind == b'PLTE', 'unsupported critical PNG chunk')
                if kind == b'PLTE':
                    require(not idat_seen and 0 < size <= 768 and size % 3 == 0,
                            'invalid optional truecolour PNG palette')
            position += size + 12
        require(ended and position == len(data), 'missing PNG IEND or trailing bytes')
        stride = self.width * self.channels
        extent = (stride + 1) * self.height
        require(extent <= 64 * 1024**2, 'PNG decoded size exceeds bound')
        decoder = zlib.decompressobj()
        filtered = decoder.decompress(bytes(compressed), extent + 1)
        require(len(filtered) == extent and decoder.eof and not decoder.unused_data and
                not decoder.unconsumed_tail, 'PNG zlib end, exact scanlines or trailing stream differs')
        rows, previous, filters = [], bytearray(stride), Counter()
        for y in range(self.height):
            offset = y * (stride + 1)
            mode = filtered[offset]
            require(mode <= 4, 'invalid PNG filter')
            filters[mode] += 1
            row = bytearray(filtered[offset + 1:offset + 1 + stride])
            if mode == 1:
                for i in range(self.channels, stride):
                    row[i] = (row[i] + row[i - self.channels]) & 255
            elif mode in (2, 3, 4):
                for i in range(stride):
                    left = row[i - self.channels] if i >= self.channels else 0
                    up = previous[i]
                    corner = previous[i - self.channels] if i >= self.channels else 0
                    if mode == 2:
                        predictor = up
                    elif mode == 3:
                        predictor = (left + up) // 2
                    else:
                        p = left + up - corner
                        a, b, c = abs(p - left), abs(p - up), abs(p - corner)
                        predictor = left if a <= b and a <= c else up if b <= c else corner
                    row[i] = (row[i] + predictor) & 255
            rows.append(bytes(row))
            previous = row
        self.rows = rows
        self.filters = dict(sorted(filters.items()))

    def rgb(self):
        if self.channels == 3:
            return b''.join(self.rows)
        result = bytearray(self.width * self.height * 3)
        position = 0
        for row in self.rows:
            for channel in range(3):
                result[position + channel:position + self.width * 3:3] = row[channel::4]
            position += self.width * 3
        return bytes(result)

    def crop_rgb(self, x, bottom_y, width, height):
        require(0 <= x < self.width and 0 <= bottom_y < self.height and
                x + width <= self.width and bottom_y + height <= self.height,
                'ROI outside original full PNG')
        top_y = self.height - bottom_y - height
        rows = self.rows[top_y:top_y + height]
        if self.channels == 3:
            return b''.join(row[x * 3:(x + width) * 3] for row in rows)
        result = bytearray(width * height * 3)
        for y, row in enumerate(rows):
            crop = row[x * 4:(x + width) * 4]
            for channel in range(3):
                result[y * width * 3 + channel:(y + 1) * width * 3:3] = crop[channel::4]
        return bytes(result)


class Audit:
    def __init__(self, capture):
        self.capture = Path(capture).resolve(strict=True)
        self.root = self.capture.parent
        self.start = time.monotonic()
        self.checks, self.files, self.frames, self.checkpoints = [], {}, [], {}
        self.inventory = {}
        self.record = self.load_json(self.capture, inventoried=False)
        self.inventory = self.record.get('artifacts', {})
        self.raw_geometry = None
        self.identities = None
        self.transitions = []
        self.transition_histogram = Counter()
        self.source_biomes = {}

    def deadline(self):
        require(time.monotonic() - self.start <= 180, 'oracle exceeded bounded 180-second decode/audit time')

    def check(self, name, condition, detail=None):
        self.deadline()
        self.checks.append({'name': name, 'status': 'PASS' if condition else 'FAIL',
                            **({'detail': detail} if detail is not None else {})})
        require(condition, name)

    def observed(self, name, condition, detail=None):
        self.deadline()
        if not condition:
            self.checks.append({'name': name, 'status': 'OPEN',
                                **({'detail': detail} if detail is not None else {})})
        return bool(condition)

    def path(self, relative):
        require(type(relative) is str and 0 < len(relative) <= 256, 'invalid capture path')
        name = PurePosixPath(relative)
        require(not name.is_absolute() and name.parts and '..' not in name.parts and
                str(name) == relative, 'capture paths must be canonical and relative')
        path = self.root.joinpath(*name.parts)
        require(path.is_file() and not path.is_symlink() and path.resolve().is_relative_to(self.root),
                'capture path is missing, a symlink or outside its directory')
        return path

    def session_path(self, relative):
        require(type(relative) is str and '/' not in relative and relative not in ('.', '..'),
                'water session filenames must be direct children')
        return self.path('water-seam/' + relative)

    def read(self, path, limit=64 * 1024**2, inventoried=True):
        self.deadline()
        size = path.stat().st_size
        require(0 < size <= limit, 'capture file outside byte bound: ' + path.name)
        data = path.read_bytes()
        relative, digest = path.relative_to(self.root).as_posix(), sha(data)
        if inventoried:
            require(self.inventory.get(relative) == digest, 'uninventoried file or SHA256 mismatch: ' + relative)
        self.files[relative] = {'bytes': size, 'sha256': digest}
        return data

    def load_json(self, path, inventoried=True):
        data = self.read(path, 1024**2, inventoried)
        result = json.loads(data, object_pairs_hook=unique_object,
                            parse_constant=lambda value: (_ for _ in ()).throw(Rejected('JSON nonfinite: ' + value)))
        require(type(result) is dict, 'JSON packet must be an object')
        return result

    def inventory_check(self):
        r = self.record
        self.check('capture/actual-completed-direct-native-child',
                   r.get('result') in ('CAPTURED', 'OPEN') and r.get('launch_method') == 'direct' and
                   type(r.get('child_pid')) is int and r['child_pid'] > 0 and
                   type(r.get('child_returncode')) is int and r['child_returncode'] == 0 and
                   r.get('child_signal') is None and r.get('child_timed_out') is False and
                   r.get('child_reaped') is True)
        self.check('capture/diagnostic-readback-scope', r.get('window_mode') == 'hidden' and
                   r.get('render_readback') is True and r.get('normal_input') is False and
                   r.get('evidence_type') == 'DEVELOPER_DIAGNOSTIC' and
                   type(r.get('environment')) is dict and
                   type(r['environment'].get('HELLOMINE3D_WATER_SEAM_CAPTURE_DIR')) is str and
                   Path(r['environment']['HELLOMINE3D_WATER_SEAM_CAPTURE_DIR']).name == 'water-seam')
        require(type(self.inventory) is dict and 1 <= len(self.inventory) <= 1024,
                'capture inventory count outside bound')
        total, session_total = 0, 0
        for relative, digest in self.inventory.items():
            require(type(digest) is str and len(digest) == 64 and
                    all(x in '0123456789abcdef' for x in digest), 'invalid SHA256 inventory value')
            path = self.path(relative)
            total += path.stat().st_size
            if relative.startswith('water-seam/'):
                session_total += path.stat().st_size
            require(total <= SESSION_LIMIT + 16 * 1024**2 and session_total <= SESSION_LIMIT,
                    'capture or water session exceeds byte bound')
            self.read(path)
        self.check('capture/complete-inventory-sha256-and-byte-bound', True,
                   {'files': len(self.inventory), 'total_bytes': total, 'water_session_bytes': session_total})

    def source(self, facts, label):
        self.check('source/' + label + '/natural-current-nonatomic-endpoint',
                   facts.get('seed') == 42 and facts.get('terrain_generation_version') == 31 and
                   facts.get('seam') == {'axis': 'z', 'coordinate': -224, 'surface_y': 65} and
                   facts.get('atomic_copy') is False)
        owners = facts['owners']
        require(type(owners) is list and len(owners) == 2, 'source must identify exactly two natural owners')
        revisions = {}
        for owner in owners:
            section = tuple(vector(owner['section'], 3))
            require(all(type(x) is int for x in owner['section']) and section in OWNERS and
                    section not in revisions, 'source owner missing or duplicate')
            require(type(owner.get('known')) is bool, 'source known flag must be an actual boolean')
            known = owner['known']
            self.observed('source/' + label + '/actual-owner-known', known)
            before = integer(owner['version_before'], 0 if not known else 1, 2**32 - 1)
            after = integer(owner['version_after'], 0 if not known else 1, 2**32 - 1)
            self.observed('source/' + label + '/stable-read-revision-endpoints', before == after)
            revisions[section] = before
        columns = facts['columns']
        require(type(columns) is list and len(columns) == 36, 'source requires the exact 36-column envelope')
        result = {}
        for column in columns:
            x, z = integer(column['x'], 207, 224), integer(column['z'], -225, -224)
            require((x, z) not in result, 'duplicate source column')
            blocks = column['blocks']
            require(type(blocks) is list and len(blocks) == 10, 'source column must include y56..65')
            values = {}
            for block in blocks:
                y = integer(block['y'], 56, 65)
                require(y not in values, 'duplicate source column height')
                values[y] = (integer(block['id'], 0, 65535), integer(block['metadata'], 0, 255))
            require(set(values) == set(range(56, 66)), 'source column height envelope differs')
            biome = integer(column['biome'], 0, 9)
            require(column['biome_name'] == ('River' if biome == 8 else 'other'),
                    'source numeric biome and actual classification name differ')
            values['biome'], values['biome_name'] = biome, column['biome_name']
            result[(x, z)] = values
        require(set(result) == {(x, z) for x in range(207, 225) for z in (-225, -224)},
                'source x207..224/z-225,-224 envelope differs')
        self.source_biomes[label] = [{'x': x, 'z': z, 'biome': v['biome'], 'biome_name': v['biome_name']}
                                    for (x, z), v in sorted(result.items())]
        self.check('source/' + label + '/complete-columns-and-actual-biome-classifications', True)
        return result, revisions

    def operation(self, op, observed, prefix):
        origin = tuple(vector(op['origin'], 3))
        require(all(type(x) is int for x in op['origin']) and origin in OWNERS, 'unexpected water owner')
        oid, serial = integer(op['object_id'], 1), integer(op['upload_serial'], 1)
        vc = integer(op['vertex_count'], 1, OP_LIMIT // 44)
        ic = integer(op['index_count'], 3, OP_LIMIT // 4)
        require(ic % 3 == 0 and vc * 44 + ic * 4 <= OP_LIMIT, 'native triangle extent outside bound')
        require(op['layer'] == 'water' and op['owner_key'] == '_'.join(map(str, origin)) and
                op['material_name'] == 'HelloMine3D/Water' and
                op['vertex_start'] == op['index_start'] == 0 and op['index_type'] == 'u32' and
                op['instances'] == op['effective_instances'] == 1 and
                op['source0_instance_data'] is False and op['global_instance_buffer_present'] is False and
                type(op['global_instancing_available']) is bool and
                (not op['global_instancing_available'] or op['global_instance_count'] == 1),
                'original direct water operation or single-instance scope differs')
        parts = op['parts']
        require(type(parts) is list and len(parts) == 1, 'water must retain one direct section owner')
        part = parts[0]
        revision = integer(part['upload_revision'], 1, 2**32 - 1)
        require(tuple(part['section']) == origin and part['live_revision'] == revision and
                part['live_known'] is True and part['gpu_resident'] is True and
                part['still_cpu_ready'] is False and part.get('incarnation') is None and
                part.get('incarnation_known') is False, 'actual current water upload/residency differs')
        elements = op['elements']
        require(type(elements) is list and len(elements) == 5 and
                tuple((e['offset'], e['type'], e['semantic'], e['semantic_index'], e['components'])
                      for e in elements) == LAYOUT and
                all(e['source'] == 0 and e['bytes'] == e['components'] * 4 for e in elements),
                'actual original 44-byte declaration differs')
        node = vector(op['node_world_row_major'], 16)
        expected = (1, 0, 0, origin[0] * 16, 0, 1, 0, origin[1] * 16,
                    0, 0, 1, origin[2] * 16, 0, 0, 0, 1)
        require(all(close(x, y) for x, y in zip(node, expected)), 'actual node differs from constructor origin')
        camera = integer(observed['main_camera_id'], 1)
        p = op['pass']
        require(op['camera_id'] == camera and integer(p['id'], 1) and p['index'] == 0 and
                p['iterations'] == 1 and p['main_camera'] is True and
                type(p['vertex_program']) is str and p['vertex_program'] and
                type(p['fragment_program']) is str and p['fragment_program'], 'original main-camera pass differs')
        program, vao, vbo, ebo = (integer(op[name], 1, 2**32 - 1) for name in ('program', 'vao', 'vbo', 'ebo'))
        require(op['expected_ebo'] == ebo and op['pipeline'] == 0 and op['program_linked'] is True and
                op['render_system_stages'] == {'geometry': False, 'tessellation_hull': False,
                                              'tessellation_domain': False, 'compute': False} and
                all(type(value) is bool for value in op['render_system_stages'].values()),
                'unsupported native program, pipeline or stage chain')
        shaders = op['attached_shaders']
        require(type(shaders) is list and len(shaders) == 2, 'exactly two native attached shaders required')
        stages = {integer(s['type'], 1): integer(s['id'], 1, 2**32 - 1) for s in shaders}
        require(stages == {35633: p['vertex_shader_id'], 35632: p['fragment_shader_id']} and
                all(s['compiled'] is True and s['production_pass_match'] is True for s in shaders),
                'actual native VS/FS differs from original pass')
        attempts = [a for a in observed['draw_attempts'] if a['object_id'] == oid and a['main_camera'] is True]
        require(len(attempts) == 1 and attempts[0]['pre_called'] is True and
                attempts[0]['post_called'] is True and attempts[0]['status'] == 'CAPTURED' and
                attempts[0]['camera_id'] == camera and attempts[0]['pass_id'] == p['id'],
                'original object lacks exactly one paired actual same-frame main draw')
        query = op['primitive_query']
        require(query['target'] == 35975 and integer(query['id'], 1, 2**32 - 1) and
                query['began'] is True and query['ended'] is True and query['conflicting_query'] == 0 and
                query['current_query_after_end'] == 0 and query['result'] == ic // 3 and
                query['expected_whole_operation'] == ic // 3,
                'paired original whole-operation primitive query differs')
        gl_version = tuple(vector(op['gl_version'], 2))
        require(all(type(x) is int for x in op['gl_version']) and gl_version >= (4, 1), 'unsupported native GL version')
        attrs = op['active_attributes']
        require(type(attrs) is list and len(attrs) == op['active_attribute_count'] == 4,
                'water requires exactly actual vertex/uv0/uv1/uv2 active inputs')
        names, locations = set(), set()
        for attr in attrs:
            name, location = attr['name'], integer(attr['location'], 0, 255)
            require(name in INPUTS and name not in names and location not in locations, 'unknown or duplicate native input')
            names.add(name); locations.add(location)
            offset, size, semantic, semantic_index, shader_type = INPUTS[name]
            require(attr['shader_type'] == shader_type and attr['shader_array_size'] == 1 and
                    attr['enabled'] is True and attr['buffer'] == vbo and attr['type'] == GL_FLOAT and
                    attr['size'] == size and attr['stride'] == 44 and attr['normalized'] is False and
                    attr['integer'] is False and attr['long'] is False and attr['divisor'] == 0 and
                    attr['long_observation_mode'] == ('QUERY_GL43_ARRAY_LONG' if gl_version >= (4, 3)
                                                     else 'DERIVED_GL_FLOAT_EXCLUDES_DOUBLE') and
                    attr['pointer_offset'] == attr['expected_offset'] == offset and
                    attr['semantic'] == semantic and attr['semantic_index'] == semantic_index and
                    attr['matches_declaration'] is True, 'actual original VAO input differs')
        require(names == set(INPUTS), 'missing actual active water input')
        raw = op['source0']
        require(raw['stride'] == 44 and raw['vertices'] == vc and raw['vertex_bytes'] == vc * 44 and
                raw['index_bytes'] == ic * 4 and raw['native_cpu_bytes_equal'] is True and
                all(raw[key] is None for key in ('file', 'cpu_file', 'index_file', 'cpu_index_file')),
                'per-frame original native/CPU storage assertion or no-raw scope differs')
        require(op['state_restored'] is True and op['context_before'] == op['context_after'] and
                op['context_before']['vao'] == vao and op['context_before']['program'] == program and
                op['context_before']['pipeline'] == 0 and
                op['context_before']['vao_element_buffer'] == ebo and op['gl_errors'] == [],
                'native observation state restoration or GL errors differ')
        uniforms = op['uniforms']
        require(type(uniforms) is dict and set(uniforms) ==
                {'globalTime', 'waterDetailStrength', 'world', 'worldView', 'worldViewProj', 'cameraPosition'},
                'exact six actual water uniforms required')
        values = {}
        for name, uniform in uniforms.items():
            integer(uniform['index'], 0, 2**32 - 2); integer(uniform['location'], 0, 2**31 - 1)
            require(uniform['size'] == 1, 'native uniform array extent differs')
            if name in ('globalTime', 'waterDetailStrength'):
                require(uniform['type'] == GL_FLOAT, 'native scalar uniform type differs')
                values[name] = finite(uniform['value'])
            elif name == 'cameraPosition':
                require(uniform['type'] == GL_FLOAT_VEC3, 'native camera uniform type differs')
                values[name] = vector(uniform['values'], 3)
            else:
                require(uniform['type'] == GL_FLOAT_MAT4 and uniform['layout'] == 'column_major',
                        'native matrix type/layout differs')
                values[name] = vector(uniform['values'], 16)
        native_world_row = tuple(values['world'][c * 4 + r] for r in range(4) for c in range(4))
        require(all(close(a, b, 1e-4) for a, b in zip(native_world_row, node)), 'actual native world uniform differs from node')
        self.check(prefix + '/original-native-draw-inputs-uniforms-and-state', True)
        return origin, {'object_id': oid, 'upload_serial': serial, 'revision': revision,
                        'vertex_count': vc, 'index_count': ic, 'vbo': vbo, 'ebo': ebo}, values

    def raw_checkpoint(self, packet, operations, columns, label):
        raws = packet['raw_checkpoint_operations']
        require(type(raws) is list and len(raws) == len(operations) <= 2,
                'checkpoint raw operation count differs from actual captured operations')
        by_identity = {(op['object_id'], op['upload_serial']): op for op in operations.values()}
        decoded, used = {}, set()
        for raw in raws:
            key = (integer(raw['object_id'], 1), integer(raw['upload_serial'], 1))
            require(key in by_identity and key not in used, 'checkpoint raw identity differs from same-frame draw')
            used.add(key)
            op = by_identity[key]
            require(raw['origin'] == op['origin'] and raw['parts'] == op['parts'],
                    'checkpoint raw owner/revision metadata differs from actual draw')
            files = [raw[name] for name in ('native_file', 'cpu_file', 'native_index_file', 'cpu_index_file')]
            require(len(set(files)) == 4 and all(name.startswith('frame-' + str(packet['frame_id']) + '-native-op-')
                                               for name in files), 'raw checkpoint paths are reused or from another frame')
            native, cpu, indices, cpu_indices = [self.read(self.session_path(name), OP_LIMIT) for name in files]
            vc, ic = op['vertex_count'], op['index_count']
            self.check('checkpoint/' + label + '/' + op['owner_key'] + '/actual-raw-native-CPU-and-extents',
                       len(native) == vc * 44 and len(indices) == ic * 4 and native == cpu and indices == cpu_indices)
            ids = array.array('I')
            require(ids.itemsize == 4, 'consumer platform u32 array ABI differs')
            ids.frombytes(indices)
            if sys.byteorder != 'little':
                ids.byteswap()
            self.check('checkpoint/' + label + '/' + op['owner_key'] + '/finite44B-and-referenced-u32-range',
                       all(math.isfinite(v) for row in struct.iter_unpack('<11f', native) for v in row) and
                       all(v < vc for v in ids))
            origin = tuple(op['origin'])
            corners, light_inputs, cells = {}, {}, set()
            for start in range(0, ic, 3):
                triangle = [struct.unpack_from('<11f', native, ids[i] * 44) for i in range(start, start + 3)]
                world = [(v[0] + origin[0] * 16, v[1] + origin[1] * 16, v[2] + origin[2] * 16)
                         for v in triangle]
                if not all(close(p[1], 65) for p in world):
                    continue
                a, b, c = world
                normal_y = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2])
                if normal_y <= 0 or not any(close(p[2], -224) for p in world):
                    continue
                cell = (math.floor(sum(p[0] for p in world) / 3), math.floor(sum(p[2] for p in world) / 3))
                require(cell in columns, 'referenced seam top is outside captured source columns')
                self.observed('checkpoint/' + label + '/actual-natural-Water64-Air65-support',
                              columns[cell][64][0] == 7 and columns[cell][65][0] == 0)
                cells.add(cell)
                for p, vertex in zip(world, triangle):
                    if not close(p[2], -224):
                        continue
                    x = round(p[0])
                    require(208 <= x <= 224 and close(p[0], x), 'referenced seam top corner outside natural grid')
                    require(0 <= vertex[5] <= 8 and 0 <= vertex[6] <= 1, 'referenced water depth/shore input outside bound')
                    corners.setdefault(x, set()).add(tuple(vertex[3:7]))
                    light_inputs.setdefault(x, set()).add(tuple(vertex[7:10]))
            self.observed('checkpoint/' + label + '/owner-referenced-upward-natural-water-top', bool(corners and cells))
            decoded[origin] = {'corners': corners, 'uv2_inputs': light_inputs, 'cells': cells, 'raw_sha256': sha(native),
                               'index_sha256': sha(indices)}
        if not self.observed('checkpoint/' + label + '/two-actual-original-raw-owners', set(decoded) == OWNERS):
            return {'status': 'OPEN_MISSING_ORIGINAL_PAIR', 'decoded_owner_count': len(decoded)}
        expected_cells = [x for x in range(208, 224) if all(columns[(x, z)][64][0] == 7 and
                                                           columns[(x, z)][65][0] == 0 for z in (-225, -224))]
        if not self.observed('checkpoint/' + label + '/natural-Water64-Air65-crosses-seam', bool(expected_cells)):
            return {'status': 'OPEN_NO_NATURAL_SHARED_WATER_CELL', 'decoded_owner_count': len(decoded)}
        river_cells = [x for x in expected_cells if all(columns[(x, z)]['biome'] == 8 for z in (-225, -224))]
        self.observed('checkpoint/' + label + '/actual-referenced-River-cell-crosses-seam', bool(river_cells),
                      {'numeric_River_classification': 8, 'shared_River_x': river_cells})
        near, far = decoded[(13, 4, -14)], decoded[(13, 4, -15)]
        require(all((x, -224) in near['cells'] and (x, -225) in far['cells'] for x in expected_cells),
                'actual shared Water source cell lacks indexed upward surface on one side')
        expected_corners = {v for x in expected_cells for v in (x, x + 1)}
        common = set(near['corners']).intersection(far['corners'])
        require(expected_corners <= common and common, 'actual shared referenced corners are missing')
        for x in common:
            require(len(near['corners'][x]) == len(far['corners'][x]) == 1 and
                    near['corners'][x] == far['corners'][x], 'shared referenced uv0/uv1 inputs differ')
        self.check('checkpoint/' + label + '/actual-natural-shared-surface-and-equal-uv0-uv1', True,
                   {'shared_x': sorted(common), 'natural_shared_cells': expected_cells})
        # The top input at World y65 samples resident Water starting at y64.
        # Independently count at most eight consecutive Water blocks in each
        # of the four captured columns touching this World corner. uv1.x is
        # the original vertex's repeatU/depth slot, not a shader-wave result.
        world_depths = []
        for x in sorted(common):
            samples = []
            for cx in (x - 1, x):
                for cz in (-225, -224):
                    depth = 0
                    for y in range(64, 56, -1):
                        if columns[(cx, cz)][y][0] != 7:
                            break
                        depth += 1
                    samples.append({'x': cx, 'z': cz, 'consecutive_Water64_depth': depth})
            expected_depth = sum(v['consecutive_Water64_depth'] for v in samples) / 4
            world_depths.append({'world_x': x, 'world_y': 65, 'world_z': -224,
                                 'source_columns': samples, 'expected_uv1_depth_repeatU': expected_depth,
                                 'near_native_uv1_depth_repeatU': sorted(v[2] for v in near['corners'][x]),
                                 'far_native_uv1_depth_repeatU': sorted(v[2] for v in far['corners'][x])})
        self.check('checkpoint/' + label + '/actual-World-four-corner-water-depth',
                   all(close(v, corner['expected_uv1_depth_repeatU']) for corner in world_depths
                       for key in ('near_native_uv1_depth_repeatU', 'far_native_uv1_depth_repeatU')
                       for v in corner[key]),
                   {'count_scope': 'four captured resident columns; Water64 down to57, saturating at8',
                    'absolute_tolerance': 2e-5, 'corners': world_depths})
        uv2_observations = [{'world_x': x, 'near_uv2': [list(v) for v in sorted(near['uv2_inputs'][x])],
                             'far_uv2': [list(v) for v in sorted(far['uv2_inputs'][x])],
                             'same': near['uv2_inputs'][x] == far['uv2_inputs'][x]}
                            for x in sorted(common)]
        result = {'shared_x': sorted(common), 'shared_River_cells_x': river_cells,
                  'World_four_corner_water_depth': world_depths,
                  'raws': {str(k): {'vertices': v['raw_sha256'], 'indices': v['index_sha256']}
                                                     for k, v in decoded.items()},
                  'uv2_observations': uv2_observations,
                  'uv2_scope': 'descriptive actual shared-corner lighting inputs; no new gate inferred from unknown AO semantics'}
        if self.raw_geometry is not None:
            require(result == self.raw_geometry, 'three original raw checkpoints changed geometry or shared inputs')
        self.raw_geometry = result
        return result

    def projection(self, values, framebuffer, label):
        width, height = framebuffer['width'], framebuffer['height']
        roi = framebuffer['roi']
        in_roi, points = [], []
        for x in self.raw_geometry['shared_x']:
            projected, view_points = [], []
            for origin in sorted(OWNERS):
                local = (x - origin[0] * 16, 65 - origin[1] * 16, -224 - origin[2] * 16, 1)
                projected.append(matvec(values[origin]['worldViewProj'], local))
                view_points.append(matvec(values[origin]['worldView'], local))
            require(all(close(a, b, max(2e-4, abs(a) * 2e-5)) for a, b in zip(view_points[0], view_points[1])) and
                    all(close(a, b, max(2e-4, abs(a) * 2e-5)) for a, b in zip(projected[0], projected[1])),
                    'actual native view/projection transforms disagree at shared original input')
            clip = projected[0]
            if clip[3] <= 0:
                continue
            px, py = ((clip[0] / clip[3] + 1) * width / 2, (clip[1] / clip[3] + 1) * height / 2)
            visible = all(-clip[3] <= clip[i] <= clip[3] for i in range(3))
            inside = visible and roi['x'] <= px <= roi['x'] + roi['width'] and roi['y'] <= py <= roi['y'] + roi['height']
            if inside:
                in_roi.append(x)
            points.append({'world_x': x, 'pixel_bottom_left': [px, py], 'inside_frustum_and_roi': inside})
        if self.observed('frame/' + str(label) + '/shared-original-input-projection-in-frozen-ROI', bool(in_roi)):
            self.check('frame/' + str(label) + '/shared-original-input-projection-in-frozen-ROI', True)
        return {'inside_roi_x': in_roi, 'original_input_projection': points,
                'scope': 'native matrices applied to original indexed positions; displaced shader outputs remain OPEN'}

    def framebuffer(self, packet, previous_rgb, frozen):
        fb = packet['framebuffer']
        width, height = integer(fb['width'], 1, 8192), integer(fb['height'], 1, 8192)
        roi = fb['roi']
        x, y = integer(roi['x'], 0, width - 1), integer(roi['y'], 0, height - 1)
        rw, rh = integer(roi['width'], 1, width), integer(roi['height'], 1, height)
        require(x + rw <= width and y + rh <= height and rw * rh * 4 <= OP_LIMIT // 2,
                'actual frozen framebuffer ROI outside bound')
        ratio = finite(self.record['expected_pixel_ratio'])
        points = vector(self.record['window_size_points'], 2)
        require((width, height) == tuple(p * ratio for p in points), 'actual framebuffer and declared window size differ')
        geometry = (width, height, x, y, rw, rh)
        require(frozen is None or geometry == frozen, 'actual framebuffer or frozen ROI changed during window')
        require(fb['origin'] == 'bottom_left' and fb['png_origin'] == 'top_left' and fb['format'] == 'RGBA8' and
                type(fb['framebuffer_srgb']) is bool and integer(fb['read_fbo'], 0, 2**32 - 1) >= 0 and
                integer(fb['read_buffer'], 0, 2**32 - 1) >= 0 and fb['state_restored'] is True and
                fb['pack_context_before'] == fb['pack_context_after'] and packet['state_restored'] is True and
                packet['context_before'] == packet['context_after'] and packet['gl_errors'] == [],
                'actual pre-swap ROI format/context restoration or GL errors differ')
        roi_file = fb['file']
        require(roi_file == 'frame-' + str(packet['frame_id']) + '-roi.png', 'ROI filename is stale or from another frame')
        image = PNG(self.read(self.session_path(roi_file), OP_LIMIT))
        # RGBA8 describes the actual glReadPixels input. FreeImage's PNG writer
        # detects an entirely opaque 32-bit bitmap as FIC_RGB and emits RGB.
        # The independent PNG decoder, rather than that readback descriptor,
        # determines the file's actual layout; comparison always uses RGB.
        require((image.width, image.height) == (rw, rh) and image.channels in (3, 4),
                'ROI PNG actual dimensions or RGB/RGBA header differs')
        roi_header = {'width': image.width, 'height': image.height, 'bit_depth': 8,
                      'colour_type': image.colour_type, 'channels': image.channels}
        rgb = image.rgb()
        if previous_rgb is not None:
            require(len(previous_rgb) == len(rgb), 'consecutive actual RGB ROI extent differs')
            histogram = Counter(abs(a - b) for a, b in zip(rgb, previous_rgb))
            count = len(rgb)
            transition = {'frame_id': packet['frame_id'], 'rgb_channel_samples': count,
                          'changed_channel_fraction': (count - histogram[0]) / count,
                          'absolute_rgb_channel_difference_max': max(histogram),
                          'absolute_rgb_channel_difference_p50': quantile(histogram, .5),
                          'absolute_rgb_channel_difference_p95': quantile(histogram, .95),
                          'absolute_rgb_channel_difference_p99': quantile(histogram, .99)}
            self.transitions.append(transition)
            self.transition_histogram.update(histogram)
        full_file = packet['full_png']
        if packet['checkpoint']:
            require(type(full_file) is str and full_file == 'frame-' + str(packet['frame_id']) + '-full.png',
                    'checkpoint full PNG must belong to this actual frame')
            full = PNG(self.read(self.session_path(full_file)))
            require((full.width, full.height) == (width, height), 'full original PNG actual size differs')
            self.checkpoints[packet['checkpoint']]['full_png_header'] = {
                'width': full.width, 'height': full.height, 'bit_depth': 8,
                'colour_type': full.colour_type, 'channels': full.channels}
            self.check('checkpoint/' + packet['checkpoint'] + '/exact-ROI-versus-full-original-RGB',
                       full.crop_rgb(x, y, rw, rh) == rgb,
                       {'full_colour_type': full.colour_type, 'roi_colour_type': image.colour_type,
                        'full_filters': full.filters, 'roi_filters': image.filters})
        else:
            require(full_file is None, 'intermediate frame must not invent an original full PNG checkpoint')
        self.check('frame/' + str(packet['frame_id']) + '/actual-lossless-ROI-and-restored-pre-swap-state', True)
        return rgb, geometry, fb, roi_header

    def run(self):
        self.inventory_check()
        index = self.load_json(self.path('water-seam/index.json'))
        self.index = index
        self.check('session/new-water-mode-and-scope',
                   index.get('schema') == 'hellomine3d-water-seam-capture-v1' and
                   index.get('status') in ('CAPTURED', 'OPEN', 'FAIL') and index.get('mode') in ('standard', 'compatibility') and
                   index.get('ordinary_input') == 'NOT_RUN')
        limits = index['limits']
        self.check('session/frozen-observer-bounds-and-codec-memory-scope',
                   limits == {'max_frames': 480, 'max_duration_seconds': 10, 'max_session_seconds': 60,
                              'max_session_bytes': SESSION_LIMIT, 'max_owned_live_bytes': OP_LIMIT,
                              'codec_internal_live_bound': 'OPEN_NOT_EXPOSED'})
        require(index['status'] != 'FAIL', 'native water observer retained FAIL')
        self.observed('session/complete-supported-observation', index['status'] == 'CAPTURED',
                      {'completion_reason': index.get('completion_reason'), 'frame_count': index.get('frame_count'),
                       'duration_seconds': index.get('duration_seconds')})
        self.check('session/observed-owned-memory-and-write-budget',
                   integer(index['observed_owned_buffer_peak_bytes'], 0, OP_LIMIT) <= OP_LIMIT and
                   integer(index['bytes_written_before_index'], 0, SESSION_LIMIT) <= SESSION_LIMIT)
        before, before_revisions = self.source(index['source_before'], 'before')
        after, after_revisions = self.source(index['source_after'], 'after')
        if self.observed('source/unchanged-public-endpoint-columns-and-revisions',
                         before == after and before_revisions == after_revisions):
            self.check('source/unchanged-public-endpoint-columns-and-revisions', True)
        filenames = index['frames']
        require(type(filenames) is list and len(filenames) <= MAX_FRAMES and
                len(filenames) == integer(index['frame_count'], 0, MAX_FRAMES) and
                len(set(filenames)) == len(filenames), 'frame list count or uniqueness differs')
        if index['status'] == 'CAPTURED':
            require(len(filenames) >= 3, 'CAPTURED session lacks three actual checkpoint frames')
        self.observed('session/retained-actual-frames-present', bool(filenames))
        previous_id, previous_elapsed, previous_time = None, None, None
        first_elapsed, first_time, previous_rgb, frozen = None, None, None, None
        time_advanced, phase_wrapped = False, False
        for ordinal, filename in enumerate(filenames):
            packet = self.load_json(self.session_path(filename))
            fid = integer(packet['frame_id'], 0)
            elapsed, delta = finite(packet['elapsed_seconds']), finite(packet['delta_seconds'])
            require(filename == 'frame-' + str(fid) + '.json', 'frame packet filename differs from actual ID')
            require(packet['schema'] == 'hellomine3d-water-seam-frame-v1' and packet['mode'] == index['mode'] and
                    packet['status'] in ('CAPTURED', 'OPEN') and 0 <= elapsed <= 60 and
                    0 <= delta <= 60 and packet['ordinary_input'] == 'NOT_RUN',
                    'invalid actual frame/timing packet or scope')
            if index['status'] == 'CAPTURED':
                require(packet['status'] == 'CAPTURED', 'CAPTURED session contains an OPEN actual frame')
            self.observed('frame/' + str(fid) + '/supported-actual-frame', packet['status'] == 'CAPTURED')
            require(previous_id is None or fid > previous_id, 'actual frame ID duplicated or reversed')
            self.observed('frame/' + str(fid) + '/contiguous-actual-frame-and-clock',
                          previous_id is None or fid == previous_id + 1 and elapsed > previous_elapsed and delta > 0)
            if first_elapsed is None:
                first_elapsed = elapsed
                require(close(elapsed, 0, 1e-8), 'continuous water window must start at actual elapsed zero')
            observed = packet['native_draw_observation']
            require(observed['schema'] == 'hellomine3d-shore-native-draw-v1' and observed['status'] in ('CAPTURED', 'OPEN'),
                    'invalid native observation schema/status')
            reasons, attempts, history = (observed[k] for k in
                                          ('open_reasons', 'draw_attempts', 'prior_and_current_open_history'))
            require(observed['frame_id'] == fid and integer(observed['main_camera_id'], 1) and
                    integer(observed['attempted_frames'], 1) >= ordinal + 1 and
                    observed['gl_errors'] == [] and type(reasons) is list and len(reasons) <= 32 and
                    all(type(r) is str and 0 < len(r) <= 2048 for r in reasons) and
                    len(set(reasons)) == len(reasons) and type(attempts) is list and len(attempts) <= 128 and
                    type(history) is list and len(history) <= 32,
                    'invalid or mismatched original same-frame native observation metadata')
            for attempt in attempts:
                require(integer(attempt['object_id'], 1) and integer(attempt['camera_id'], 0) >= 0 and
                        integer(attempt['pass_id'], 0) >= 0 and
                        all(type(attempt[k]) is bool for k in ('main_camera', 'pre_called', 'post_called')) and
                        attempt['status'] in ('CAPTURED', 'OPEN', 'OPEN_NO_PAIRED_NATIVE_CALLBACK',
                                              'EXCLUDED_SCOPE_NON_MAIN_CAMERA_OR_SUPPRESSED') and
                        type(attempt['reason']) is str and len(attempt['reason']) <= 2048,
                        'invalid actual native draw attempt metadata')
            for entry in history:
                require(type(entry['reason']) is str and 0 < len(entry['reason']) <= 2048 and
                        integer(entry['frames'], 1) <= observed['attempted_frames'] and
                        integer(entry['first_frame'], 0) <= integer(entry['last_frame'], 0) <= fid,
                        'invalid bounded native open-history metadata')
            operations = observed['operations']
            require(type(operations) is list and len(operations) <= 2, 'native water operation count outside bound')
            if observed['status'] == 'CAPTURED':
                require(len(operations) == 2, 'CAPTURED native observation lacks two original water owners')
            if packet['status'] == 'CAPTURED':
                require(observed['status'] == 'CAPTURED' and not reasons and len(operations) == 2 and len(attempts) >= 2,
                        'CAPTURED frame lacks the complete supported original draw chain')
            self.observed('frame/' + str(fid) + '/complete-native-observation',
                          observed['status'] == 'CAPTURED' and not reasons and len(operations) == 2)
            identities, native_values, by_owner = {}, {}, {}
            for op in operations:
                origin, identity, values = self.operation(op, observed, 'frame/' + str(fid) + '/' + op['owner_key'])
                require(origin not in identities, 'duplicate same-frame original owner')
                identities[origin], native_values[origin], by_owner[origin] = identity, values, op
                self.observed('frame/' + str(fid) + '/' + op['owner_key'] + '/draw-revision-matches-public-source',
                              identity['revision'] == before_revisions[origin] and before_revisions[origin] > 0)
            require(len({v['object_id'] for v in identities.values()}) == len(identities),
                    'actual original object identity is reused across owners')
            pair = set(identities) == OWNERS
            self.observed('frame/' + str(fid) + '/two-actual-original-water-owners', pair)
            if pair:
                self.observed('frame/' + str(fid) + '/stable-original-owner-upload-buffer-revision',
                              self.identities is None or identities == self.identities)
                if self.identities is None:
                    self.identities = identities
                near, far = native_values[(13, 4, -14)], native_values[(13, 4, -15)]
                self.observed('frame/' + str(fid) + '/same-frame-actual-water-time-detail-camera',
                              near['globalTime'] == far['globalTime'] and
                              near['waterDetailStrength'] == far['waterDetailStrength'] and
                              near['cameraPosition'] == far['cameraPosition'])
            native_time = None
            if pair and observed['status'] == 'CAPTURED':
                native_time = near['globalTime']
                self.observed('frame/' + str(fid) + '/actual-native-time-progress',
                              previous_time is None or native_time > previous_time)
                if first_time is None:
                    first_time = native_time
                if previous_time is not None:
                    time_advanced = time_advanced or native_time > previous_time
                    phase_wrapped = phase_wrapped or math.floor(native_time * .15) > math.floor(previous_time * .15)
                previous_time = native_time
            checkpoint = packet['checkpoint']
            require(checkpoint in ('', 'first', 'middle', 'last'), 'unexpected raw/full-image checkpoint')
            if checkpoint:
                require(checkpoint not in self.checkpoints, 'duplicate actual checkpoint')
                require((checkpoint != 'first' or ordinal == 0) and
                        (checkpoint != 'last' or ordinal == len(filenames) - 1), 'first/last checkpoint ordinal differs')
                geometry = self.raw_checkpoint(packet, by_owner, before, checkpoint)
                self.checkpoints[checkpoint] = {'frame_id': fid, 'frame_file': filename, 'elapsed_seconds': elapsed,
                                                'full_png': packet['full_png'], **geometry}
            else:
                require(packet['raw_checkpoint_operations'] == [], 'intermediate frame falsely labels raw checkpoint files')
            # Missing native evidence does not excuse skipping actual framebuffer
            # decoding, full/ROI identity, transition statistics or clock checks.
            previous_rgb, frozen, fb, roi_header = self.framebuffer(packet, previous_rgb, frozen)
            projected = None
            if pair and self.raw_geometry is not None:
                projected = self.projection(native_values, fb, fid)
            else:
                self.observed('frame/' + str(fid) + '/shared-input-projection-available', False)
            self.frames.append({'frame_id': fid, 'elapsed_seconds': elapsed, 'delta_seconds': delta,
                                'globalTime': native_time, 'checkpoint': checkpoint,
                                'roi_png_header': roi_header,
                                'shared_input_inside_roi_x': projected['inside_roi_x'] if projected else None})
            if checkpoint:
                self.checkpoints[checkpoint]['projection'] = projected
            previous_id, previous_elapsed = fid, elapsed
        duration = previous_elapsed - first_elapsed if filenames else 0
        self.observed('session/ten-second-window-complete', duration >= 10)
        self.check('session/actual-contiguous-frame-window-duration-fact',
                   close(finite(index['duration_seconds']), duration, 1e-5) and
                   (not filenames or duration <= 10 + self.frames[-1]['delta_seconds'] + .01),
                   {'frames': len(self.frames), 'elapsed_duration_seconds': duration,
                    'largest_actual_elapsed_gap_seconds': max((b['elapsed_seconds'] - a['elapsed_seconds']
                                                             for a, b in zip(self.frames, self.frames[1:])), default=None)})
        self.observed('session/actual-native-phase-wrap-sampled', phase_wrapped)
        self.observed('session/actual-native-time-progress-sampled', time_advanced)
        native_index_time = index['actual_native_time']
        self.check('session/index-native-time-facts-match-actual-draw-packets',
                   native_index_time == {'known': first_time is not None, 'first': first_time, 'last': previous_time,
                                         'advanced': time_advanced, 'fragment_phase_wrap_sampled': phase_wrapped})
        complete_checkpoints = set(self.checkpoints) == {'first', 'middle', 'last'}
        self.observed('session/three-original-distinct-checkpoints', complete_checkpoints and
                      self.checkpoints['first']['elapsed_seconds'] < self.checkpoints['middle']['elapsed_seconds'] <
                      self.checkpoints['last']['elapsed_seconds'])
        require(index['checkpoints'] == {label: data['frame_file'] for label, data in self.checkpoints.items()},
                'index checkpoint filenames differ from actual frame packets')
        self.check('session/every-actual-frame-has-lossless-RGB-transition-statistics',
                   len(self.transitions) == max(0, len(self.frames) - 1))
        return self.report('OPEN' if any(c['status'] == 'OPEN' for c in self.checks)
                           else 'PASS_SCOPED_NATIVE_INPUTS_AND_FRAME_SEQUENCE')

    def report(self, status, error=None):
        histogram = self.transition_histogram
        total = sum(histogram.values())
        return {'schema': 'hellomine3d-water-seam-independent-audit-v1', 'status': status,
                'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'capture_path': str(self.capture), 'auditor_sha256': sha(Path(__file__).read_bytes()),
                'scope': 'one natural River Z=-224 seam; native input chain, three independent original raw checkpoints, '
                         'contiguous actual frames and exact lossless framebuffer ROIs',
                'checks_passed': sum(c['status'] == 'PASS' for c in self.checks),
                'observer_session': {'status': self.index.get('status'),
                                     'completion_reason': self.index.get('completion_reason'),
                                     'frame_count': self.index.get('frame_count'),
                                     'duration_seconds': self.index.get('duration_seconds')}
                if hasattr(self, 'index') else None,
                'checks': self.checks, 'files': self.files, 'frames': self.frames,
                'checkpoints': self.checkpoints, 'source_biome_classifications': self.source_biomes,
                'rgb_transitions': self.transitions,
                'rgb_transition_summary': {
                    'description': 'absolute consecutive RGB channel differences over every decoded ROI pixel; '
                                   'descriptive statistics, without an invented universal sparkle threshold',
                    'rgb_channel_samples': total,
                    'changed_channel_fraction': (total - histogram[0]) / total if total else None,
                    'absolute_rgb_channel_difference_max': max(histogram) if histogram else None,
                    'absolute_rgb_channel_difference_p50': quantile(histogram, .5) if histogram else None,
                    'absolute_rgb_channel_difference_p95': quantile(histogram, .95) if histogram else None,
                    'absolute_rgb_channel_difference_p99': quantile(histogram, .99) if histogram else None,
                }, 'boundaries': BOUNDARIES, **({'error': error} if error else {})}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path, help='outer capture.json with SHA256 inventory')
    parser.add_argument('--output', required=True, type=Path, help='fresh independent report JSON')
    args = parser.parse_args()
    audit = None
    output_safe = False
    try:
        output = args.output.resolve()
        require(not output.exists() and not output.is_symlink(), 'fresh output report required')
        require(output != args.capture.resolve(), 'report must not overwrite input capture')
        audit = Audit(args.capture)
        require(output.relative_to(audit.root).as_posix() not in audit.inventory
                if output.is_relative_to(audit.root) else True, 'report must not replace an input artifact')
        output_safe = True
        result = audit.run()
    except (Rejected, ObservedOpen, OSError, ValueError, KeyError, TypeError, struct.error, OverflowError, zlib.error) as error:
        status = 'OPEN' if isinstance(error, ObservedOpen) else 'FAIL'
        result = audit.report(status, str(error)) if audit is not None else {
            'schema': 'hellomine3d-water-seam-independent-audit-v1', 'status': 'FAIL',
            'error': str(error), 'boundaries': BOUNDARIES}
        if not output_safe or args.output.exists() or args.output.is_symlink() or args.output.resolve() == args.capture.resolve():
            print(json.dumps({'status': 'FAIL', 'error': str(error)}, ensure_ascii=False))
            return 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + '\n')
    print(json.dumps({'status': result['status'], 'checks_passed': result.get('checks_passed', 0),
                      'frames': len(result.get('frames', [])), 'report': str(args.output.resolve()),
                      **({'error': result['error']} if 'error' in result else {})}, ensure_ascii=False))
    return 0 if result['status'] == 'PASS_SCOPED_NATIVE_INPUTS_AND_FRAME_SEQUENCE' else 2 if result['status'] == 'OPEN' else 1


if __name__ == '__main__':
    raise SystemExit(main())
