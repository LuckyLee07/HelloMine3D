#!/usr/bin/env python3
"""Independent, bounded audit of the opt-in production render lifecycle journal.

Consumes native observations only. It never imports production code, launches a
client, fabricates a passing capture, or closes ordinary-input acceptance.
"""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import time

PHASES = (
    'warm-a', 'resize-small', 'resize-large', 'resize-restore',
    'clear-a1', 'load-b', 'warm-b', 'clear-b', 'load-a2', 'warm-a2',
    'clear-a2', 'components-destroyed', 'root-shutdown',
)
HDR_PIXELS, PLANAR_PIXELS = 3840 * 2160, 1920 * 1080
MAX_WALL_MS, MAX_FRAMES, MAX_STAGE_MS, MAX_STAGE_FRAMES = 45000, 4096, 8000, 1024
MAX_INPUT_BYTES, MAX_EVENTS = 8 * 1024**2, 128
CACHE_COUNTS = {'sections', 'batches', 'dirty_batches', 'render_states',
                'last_live_sections', 'material_identity_revisions', 'local_lights'}
SCHEMA = 'hellomine3d-render-lifecycle-independent-oracle-v1'
OPEN = ['ordinary_input', 'ordinary_settings_save_restart',
        'ordinary_menu_world_switch_and_save', 'full_goal_exit_conditions']


class Rejected(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Rejected(message)


def integer(value, low=0, high=2**64 - 1):
    require(type(value) is int and low <= value <= high, 'integer outside bound')
    return value


def number(value, low=0, high=math.inf):
    require(type(value) in (int, float) and math.isfinite(value) and low <= value <= high,
            'nonfinite or out-of-bound numeric fact')
    return value


def text_value(value, maximum=4096, nonempty=True):
    require(type(value) is str and len(value) <= maximum and (bool(value) or not nonempty),
            'invalid text fact')
    return value


def strict_object(pairs):
    out = {}
    for name, value in pairs:
        require(name not in out, 'duplicate JSON key: ' + name)
        out[name] = value
    return out


def decode_json(data):
    return json.loads(data, object_pairs_hook=strict_object,
                      parse_constant=lambda value: (_ for _ in ()).throw(Rejected('nonfinite JSON: ' + value)))


def read_bounded(path):
    path = Path(path).resolve(strict=True)
    require(path.is_file() and 0 < path.stat().st_size <= MAX_INPUT_BYTES,
            'input size outside8MiB bound: ' + str(path))
    data = path.read_bytes()
    return path, data, hashlib.sha256(data).hexdigest()


class Audit:
    def __init__(self, journal, summary, process_exit_code):
        self.start = time.monotonic()
        self.checks = []
        self.inputs = {}
        for name, path in (('journal', journal), ('summary', summary)):
            absolute, data, sha = read_bounded(path)
            self.inputs[name] = {'path': str(absolute), 'bytes': len(data), 'sha256': sha}
            if name == 'journal':
                lines = data.decode('utf-8').splitlines()
                require(1 <= len(lines) <= MAX_EVENTS and all(line.strip() for line in lines),
                        'journal line count outside bound or blank record')
                self.events = [decode_json(line) for line in lines]
                require(all(type(event) is dict for event in self.events), 'journal records must be objects')
                self.release_anomalies = []
                for event in self.events:
                    if event.get('event') != 'release' or type(event.get('release')) is not dict:
                        continue
                    after = event['release'].get('after', {})
                    if type(after) is not dict:
                        continue
                    alive = [value for key in ('objects', 'manager_names') for value in after.get(key, [])
                             if type(value) is dict and value.get('alive') is True]
                    if alive or after.get('camera_name_alive') is True or event.get('runtime_pass') is False:
                        self.release_anomalies.append({'sequence': event.get('sequence'), 'phase': event.get('phase'),
                                                       'owner': event.get('owner'), 'observed_alive': alive,
                                                       'camera_name_alive': after.get('camera_name_alive'),
                                                       'runtime_pass': event.get('runtime_pass')})
            else:
                self.summary = decode_json(data)
                require(type(self.summary) is dict, 'summary must be an object')
        self.exit_code = integer(process_exit_code, -255, 255)

    def check(self, name, condition, detail=None):
        require(time.monotonic() - self.start <= 30, 'oracle exceeded30-second input/semantic bound')
        self.checks.append({'name': name, 'status': 'PASS' if condition else 'FAIL',
                            **({'detail': detail} if detail is not None else {})})
        require(condition, name)

    def protocol(self):
        self.check('process/natural-success-required', self.exit_code == 0)
        self.check('summary/completed-native-diagnostic',
                   self.summary.get('schema') == 'hellomine3d-render-lifecycle-summary-v1' and
                   self.summary.get('status') == 'COMPLETE' and
                   self.summary.get('normal_input') is False)
        frames, elapsed, sequences = [], [], []
        current, cursor, began, checkpoints, ended = None, 0, {}, {}, {}
        self.releases = []
        for index, event in enumerate(self.events):
            prefix = f'event{index}'
            self.check(prefix + '/diagnostic-schema',
                       event.get('schema') == 'hellomine3d-render-lifecycle-journal-v1' and
                       event.get('normal_input') is False)
            sequences.append(integer(event['sequence'], 0, MAX_EVENTS))
            frames.append(integer(event['frame'], 0, MAX_FRAMES - 1))
            elapsed.append(number(event['elapsed_ms'], 0, MAX_WALL_MS))
            phase = event['phase']
            require(phase in PHASES, 'unknown phase: ' + str(phase))
            kind = event['event']
            require(kind in ('begin', 'checkpoint', 'end', 'release'), 'failure/unknown event')
            if kind == 'begin':
                self.check(prefix + '/ordered-unique-begin',
                           current is None and cursor < len(PHASES) and
                           phase == PHASES[cursor] and phase not in began)
                current = phase
                began[phase] = event
            else:
                self.check(prefix + '/inside-current-phase', current == phase)
                if kind == 'checkpoint':
                    self.check(prefix + '/unique-checkpoint', phase not in checkpoints)
                    checkpoints[phase] = event
                elif kind == 'end':
                    self.check(prefix + '/checkpoint-before-end', phase in checkpoints and phase not in ended)
                    checkpoint = checkpoints[phase]
                    self.check(prefix + '/unchanged-checkpoint-end-facts',
                               all(event[field] == checkpoint[field] for field in
                                   ('frame', 'world_epoch', 'world_instance', 'world_directory', 'world_id', 'snapshot')))
                    ended[phase] = event
                    current, cursor = None, cursor + 1
                else:
                    self.releases.append(event)
        self.check('journal/complete13-phases',
                   current is None and cursor == len(PHASES) and
                   set(began) == set(checkpoints) == set(ended) == set(PHASES))
        self.check('journal/unique-contiguous-sequences',
                   sequences == list(range(1, len(sequences) + 1)))
        self.check('journal/monotone-frame-and-walltime',
                   all(a <= b for a, b in zip(frames, frames[1:])) and
                   all(a <= b for a, b in zip(elapsed, elapsed[1:])))
        for phase in PHASES:
            self.check(phase + '/bounded-stage',
                       ended[phase]['frame'] - began[phase]['frame'] < MAX_STAGE_FRAMES and
                       ended[phase]['elapsed_ms'] - began[phase]['elapsed_ms'] <= MAX_STAGE_MS)
        self.checkpoints = checkpoints
        self.beginnings = began

    def run(self):
        self.protocol()
        self.lifecycle()

    def dimensions(self, values):
        require(type(values) is list and len(values) == 2, 'dimension pair required')
        return tuple(integer(v, 0, 16384) for v in values)

    def managers(self, snapshot, phase):
        managers = snapshot['manager']
        self.check(phase + '/actual-managers-available', snapshot['managers_available'] is True and type(managers) is dict)
        for kind in ('texture', 'material', 'mesh', 'program', 'compositor'):
            facts = managers[kind]
            names = facts['names']
            require(type(names) is list and len(names) <= 8192, 'manager name array outside bound')
            for name in names:
                text_value(name)
                require(':' in name and all(name.split(':', 1)), 'resource group:name fact required')
            self.check(phase + '/manager-' + kind + '-actual-name-count',
                       integer(facts['count'], 0, 8192) == len(names) and names == sorted(set(names)))
            integer(facts['memory_bytes'], 0, 4 * 1024**3)
        return managers

    def storage(self, facts, width, height, samples, prefix):
        native = facts['native']
        for key in ('resolve_fbo', 'draw_fbo', 'gl_error_before', 'gl_error_after'):
            integer(native[key], 0, 2**32-1)
        self.check(prefix + '/actual-complete-GL0',
                   integer(native['resolve_fbo'], 1, 2**32-1) > 0 and
                   integer(native['draw_fbo'], 1, 2**32-1) > 0 and native['complete'] is True and
                   native['gl_error_before'] == native['gl_error_after'] == 0)
        formats = {34842: 8, 33189: 2, 33190: 4, 33191: 4, 36012: 4,
                   35056: 4, 36013: 8, 36166: 1, 36167: 1, 36168: 1, 36169: 2}
        attachments = {}
        for slot in ('colour', 'resolved', 'depth', 'stencil'):
            a = native[slot]
            require(type(a) is dict and set(a) == {'kind', 'object', 'format', 'width', 'height', 'samples'},
                    'complete native attachment shape required')
            for value in a.values():
                integer(value, 0, 2**32-1)
            kind = integer(a['kind'], 0, 2**32-1)
            if kind == 0:
                self.check(prefix + '/' + slot + '-optional-absent',
                           slot == 'stencil' and all(a[field] == 0 for field in ('object', 'format', 'width', 'height', 'samples')))
                continue
            object_id = integer(a['object'], 1, 2**32-1)
            self.check(prefix + '/' + slot + '-native-shape',
                       kind in (5890, 36161) and a['width'] == width and a['height'] == height and
                       a['samples'] == (0 if slot == 'resolved' else samples))
            if slot == 'resolved':
                self.check(prefix + '/resolved-real-half-texture', kind == 5890 and a['format'] == 34842)
            elif slot == 'colour':
                self.check(prefix + '/draw-real-half-storage', a['format'] == 34842 and kind == (36161 if samples else 5890))
            else:
                self.check(prefix + '/' + slot + '-owned-depth-stencil-storage',
                           kind == 36161 and a['format'] in (33189, 33190, 33191, 36012, 35056, 36013, 36166, 36167, 36168, 36169))
            require(a['format'] in formats, 'unknown logical storage format')
            key = (kind, object_id)
            description = (a['format'], width, height, a['samples'])
            self.check(prefix + '/' + slot + '-consistent-alias', key not in attachments or attachments[key] == description)
            attachments[key] = description
        self.check(prefix + '/actual-resolve-draw-identity',
                   (native['resolve_fbo'] != native['draw_fbo']) if samples else
                   (native['resolve_fbo'] == native['draw_fbo'] and native['resolved'] == native['colour']))
        logical_bytes = sum(w * h * formats[fmt] * max(1, count)
                            for fmt, w, h, count in attachments.values())
        return logical_bytes

    def target(self, facts, managers, dims, samples, budget, prefix):
        self.target_shape(facts)
        w, h = dims
        self.check(prefix + '/owned-count-and-dimensions',
                   facts['active'] is True and facts['target_count'] == facts['depth_count'] == 1 and
                   facts['width'] == w and facts['height'] == h and 0 < w * h <= budget and
                   facts['observer_failures'] == 0 and facts['owned_depth_attached'] is True and facts['depth_pool'] == 0)
        integer(facts['generation'], 1, MAX_EVENTS)
        name = text_value(facts['texture_name'])
        self.check(prefix + '/actual-owned-manager-name',
                   sum(value.split(':', 1)[1] == name for value in managers['texture']['names']) == 1)
        return self.storage(facts, w, h, samples, prefix)

    def empty_planar(self, facts, phase, camera_count):
        self.target_shape(facts)
        self.check(phase + '/planar-references-released',
                   facts['active'] is False and facts['target_count'] == facts['depth_count'] == 0 and
                   facts['width'] == facts['height'] == 0 and
                   facts['private_materials'] == facts['private_passes'] == 0 and facts['material_names'] == [] and
                   facts['camera_count'] == camera_count and facts['texture_name'] == '' and
                   facts['owned_depth_attached'] is False and
                   all(facts[field] is False for field in ('selected', 'binder_bound', 'water_sampler_bound',
                                                          'lod_camera_bound', 'listeners_active')) and
                   facts['observer_failures'] == 0)
        self.zero_native(facts['native'], phase + '/planar')

    def zero_native(self, native, prefix):
        for key in ('resolve_fbo', 'draw_fbo', 'gl_error_before', 'gl_error_after'):
            integer(native[key], 0, 2**32-1)
        for slot in ('colour', 'resolved', 'depth', 'stencil'):
            require(type(native[slot]) is dict and set(native[slot]) == {'kind', 'object', 'format', 'width', 'height', 'samples'},
                    'complete zero-native attachment shape required')
            for value in native[slot].values():
                integer(value, 0, 2**32-1)
        self.check(prefix + '/native-absent',
                   native['resolve_fbo'] == native['draw_fbo'] == 0 and native['complete'] is False and
                   native['gl_error_before'] == native['gl_error_after'] == 0 and
                   all(all(value == 0 for value in native[slot].values())
                       for slot in ('colour', 'resolved', 'depth', 'stencil')))

    def target_shape(self, facts):
        for field in ('generation', 'update_count', 'width', 'height', 'target_count', 'depth_count',
                      'depth_pool', 'camera_count', 'private_materials', 'private_passes', 'observer_failures'):
            integer(facts[field])
        for field in ('active', 'owned_depth_attached', 'selected', 'binder_bound', 'water_sampler_bound',
                      'lod_camera_bound', 'listeners_active'):
            require(type(facts[field]) is bool, 'actual target boolean required: ' + field)
        text_value(facts['texture_name'], nonempty=False)
        text_value(facts['camera_name'], nonempty=False)
        require(type(facts['material_names']) is list and len(facts['material_names']) <= 96,
                'actual private-material name array outside bound')
        for value in facts['material_names']:
            text_value(value)

    def retirement(self, event):
        owner, record = event['owner'], event['release']
        prefix = f"release{event['sequence']}/{owner}"
        before, after = record['before'], record['after']
        self.check(prefix + '/runtime-did-not-report-hidden-failure', event['runtime_pass'] is True)
        if owner == 'planar-camera':
            self.check(prefix + '/actual-camera-deleted',
                       text_value(before['camera_name']) == self.camera_name and after['camera_name_alive'] is False)
            return
        require(owner in ('hdr-target', 'planar-target', 'planar-materials'), 'unknown release owner')
        self.check(prefix + '/native-delete-GL0', after['gl_error_before'] == after['gl_error_after'] == 0)
        expected_ids = set()
        expected_names = set()
        if owner != 'planar-materials':
            native = before['native']
            require(before['target_count'] == before['depth_count'] == 1, 'target retirement lacks live owned object')
            self.check(prefix + '/retiring-actual-owned-depth', before['owned_depth_attached'] is True and before['depth_pool'] == 0)
            for key in ('resolve_fbo', 'draw_fbo'):
                expected_ids.add((36160, integer(native[key], 1, 2**32-1)))
            for slot in ('colour', 'resolved', 'depth', 'stencil'):
                a = native[slot]
                if a['kind']:
                    expected_ids.add((a['kind'], integer(a['object'], 1, 2**32-1)))
            expected_names.add(('texture', text_value(before['texture_name'])))
            prior = self.beginnings[event['phase']]['snapshot']['hdr' if owner == 'hdr-target' else 'planar']
            self.check(prefix + '/retired-exact-previous-allocation',
                       before['generation'] == prior['generation'] and before['texture_name'] == prior['texture_name'] and
                       before['native'] == prior['native'])
            self.storage(before, integer(before['width'], 1, 16384), integer(before['height'], 1, 16384),
                         4 if owner == 'hdr-target' else 0, prefix + '/before')
            if owner == 'hdr-target':
                self.check(prefix + '/production-compiled-operations-drained', record['compiled_operations_drained'] is True)
                self.check(prefix + '/production-main-camera-viewport-restored', record['camera_viewport_restored'] is True)
        else:
            names = before['material_names']
            require(type(names) is list and 1 <= len(names) <= 96, 'private-material retirement lacks actual names')
            self.check(prefix + '/all-private-material-names', len(names) == before['private_materials'] and len(names) == len(set(names)))
            expected_names = {('material', text_value(name)) for name in names}
        objects, names = after['objects'], after['manager_names']
        require(type(objects) is list and len(objects) <= 8 and type(names) is list and len(names) <= 96,
                'retirement object/name observations outside bound')
        actual_ids = [(o['kind'], o['id']) for o in objects]
        actual_names = [(n['kind'], n['name']) for n in names]
        self.check(prefix + '/exact-old-native-object-set',
                   len(actual_ids) == len(set(actual_ids)) and set(actual_ids) == expected_ids and
                   all(o['alive'] is False for o in objects))
        self.check(prefix + '/exact-old-manager-name-set',
                   len(actual_names) == len(set(actual_names)) and set(actual_names) == expected_names and
                   all(n['alive'] is False for n in names))

    def lifecycle(self):
        for field in ('completed_phases', 'maximum_frames', 'maximum_seconds', 'maximum_journal_records',
                      'stage_maximum_frames', 'stage_maximum_seconds', 'world_epoch'):
            integer(self.summary[field])
        self.check('summary/fixed-bounds-and-complete-count',
                   self.summary['completed_phases'] == 13 and self.summary['maximum_frames'] == MAX_FRAMES and
                   self.summary['maximum_seconds'] * 1000 == MAX_WALL_MS and
                   self.summary['maximum_journal_records'] == MAX_EVENTS and self.summary['world_epoch'] == 3 and
                   self.summary['stage_maximum_frames'] == MAX_STAGE_FRAMES and
                   self.summary['stage_maximum_seconds'] * 1000 == MAX_STAGE_MS and self.summary['reason'] == '')
        base = self.checkpoints['warm-a']
        root, scene, window = (base['snapshot'][key] for key in ('root_instance', 'scene_instance', 'window_instance'))
        for value in (root, scene, window):
            require(type(value) is str and re.fullmatch(r'0x[0-9a-fA-F]+', value) and int(value, 16) > 0,
                    'actual nonnull native owner address required')
        self.check('summary/same-Root-and-Scene-identity',
                   self.summary['root_instance'] == root and self.summary['scene_instance'] == scene)
        a, b = (Path(self.checkpoints[phase]['world_directory']) for phase in ('warm-a', 'warm-b'))
        self.check('world/two-real-independent-clone-directories',
                   a.is_absolute() and b.is_absolute() and a.name == 'save-a' and b.name == 'save-b' and
                   a.parent == b.parent and a != b and '..' not in a.parts and '..' not in b.parts)
        for path in (a, b):
            meta, data, sha = read_bounded(path / 'world.meta')
            require(len(data) <= 65536, 'world meta outside64KiB bound')
            ids = [line[9:] for line in data.decode('utf-8').splitlines() if line.startswith('world_id ')]
            require(len(ids) == 1 and ids[0], 'real clone world identity missing/duplicate')
            self.inputs['world-' + path.name] = {'path': str(meta), 'bytes': len(data), 'sha256': sha, 'world_id': ids[0]}
        initial_points = self.dimensions(base['snapshot']['requested_points'])
        initial_pixels = self.dimensions(base['snapshot']['window_pixels'])
        require(initial_points == (1280, 720), 'probe requires original1280x720 points')
        scale = initial_pixels[0] // initial_points[0]
        self.check('window/observed-point-to-pixel-scale', scale in (1, 2) and initial_pixels == tuple(v * scale for v in initial_points))
        self.camera_name = text_value(base['snapshot']['planar']['camera_name'])
        live = {'warm-a', 'resize-small', 'resize-large', 'resize-restore', 'warm-b', 'warm-a2'}
        clear = {'clear-a1', 'clear-b', 'clear-a2'}
        dims_for = {'warm-a': (1280, 720), 'resize-small': (960, 540), 'resize-large': (1600, 900)}
        hdr_generation, planar_generation = None, None
        self.derived_targets = []
        for phase in PHASES:
            event, s = self.checkpoints[phase], self.checkpoints[phase]['snapshot']
            self.check(phase + '/hidden-zero-input-nonperf',
                       s['hidden'] is True and s['input_event_count'] == 0 and s['perf_enabled'] is False)
            require(type(s['cache']) is dict and set(s['cache']) == CACHE_COUNTS | {'dynamic_shadow_off'},
                    'complete resident/cache/light/shadow observation required')
            for key in CACHE_COUNTS:
                integer(s['cache'][key])
            require(type(s['cache']['dynamic_shadow_off']) is bool, 'actual shadow-off boolean required')
            epoch = 1 if PHASES.index(phase) <= 4 else 2 if PHASES.index(phase) <= 7 else 3
            directory = b if epoch == 2 else a
            self.check(phase + '/actual-A-B-A-world-epoch',
                       event['world_epoch'] == epoch and event['world_directory'] == str(directory) and
                       event['world_id'] == self.inputs['world-' + directory.name]['world_id'])
            if phase == 'root-shutdown':
                self.check(phase + '/Root-was-live-before-destruction',
                           self.beginnings[phase]['snapshot']['root_alive'] is True and
                           self.beginnings[phase]['snapshot']['root_instance'] == root)
                self.check(phase + '/root-shutdown-without-native-query',
                           s['root_alive'] is False and s['scene_alive'] is False and s['managers_available'] is False and
                           s['manager'] is None and s['hdr_component'] is False and s['planar_component'] is False and
                           s['root_instance'] in ('0', '0x0', '(nil)') and s['window_instance'] in ('0', '0x0', '(nil)') and
                           s['scene_camera_count'] == 0 and self.dimensions(s['window_pixels']) == (0, 0) and
                           self.dimensions(s['viewport_pixels']) == (0, 0) and s['main_viewport_bound'] is False)
                self.zero_native(s['hdr']['native'], phase + '/hdr')
                self.empty_planar(s['planar'], phase, 0)
                continue
            self.check(phase + '/same-live-Root-Scene-window',
                       s['root_alive'] is True and s['scene_alive'] is True and
                       (s['root_instance'], s['scene_instance'], s['window_instance']) == (root, scene, window))
            self.check(phase + '/main-camera-bound-to-actual-window-viewport', s['main_viewport_bound'] is True)
            managers = self.managers(s, phase)
            points = dims_for.get(phase, (1280, 720))
            pixels = tuple(value * scale for value in points)
            self.check(phase + '/actual-window-and-viewport-dimensions',
                       self.dimensions(s['requested_points']) == points and
                       self.dimensions(s['window_pixels']) == self.dimensions(s['viewport_pixels']) == pixels)
            if phase == 'components-destroyed':
                self.check(phase + '/components-were-live-before-destruction',
                           self.beginnings[phase]['snapshot']['hdr_component'] is True and
                           self.beginnings[phase]['snapshot']['planar_component'] is True)
                self.check(phase + '/components-before-scene-root',
                           s['hdr_component'] is False and s['planar_component'] is False and s['scene_camera_count'] == 1)
                self.check(phase + '/HDR-ownership-empty', s['hdr']['target_count'] == s['hdr']['depth_count'] == 0 and s['hdr']['active'] is False)
                self.zero_native(s['hdr']['native'], phase + '/hdr')
                self.empty_planar(s['planar'], phase, 0)
            else:
                self.check(phase + '/production-components-present', s['hdr_component'] is True and s['planar_component'] is True)
                hbytes = self.target(s['hdr'], managers, pixels, 4, HDR_PIXELS, phase + '/hdr')
                generation = integer(s['hdr']['generation'], 1, MAX_EVENTS)
                expected = generation if hdr_generation is None else hdr_generation + (phase in ('resize-small', 'resize-large', 'resize-restore'))
                self.check(phase + '/HDR-generation-exactly-one-per-real-resize', generation == expected)
                hdr_generation = generation
                if phase in live:
                    half = tuple((value + 1) // 2 for value in pixels)
                    p = s['planar']
                    pbytes = self.target(p, managers, half, 0, PLANAR_PIXELS, phase + '/planar')
                    self.check(phase + '/completed-real-frame-wait',
                               event['frame'] - self.beginnings[phase]['frame'] >= 12)
                    self.check(phase + '/actual-private-materials-and-scoped-bindings',
                               1 <= p['private_materials'] <= 96 and p['private_materials'] == len(set(p['material_names'])) == len(p['material_names']) and
                               1 <= p['private_passes'] <= 384 and p['camera_count'] == 1 and p['camera_name'] == self.camera_name and
                               p['selected'] is True and p['binder_bound'] is True and p['water_sampler_bound'] is True and
                               p['lod_camera_bound'] is True and p['listeners_active'] is False and p['update_count'] > 0 and
                               all(sum(value.split(':', 1)[1] == name for value in managers['material']['names']) == 1
                                   for name in p['material_names']))
                    generation = integer(p['generation'], 1, MAX_EVENTS)
                    self.check(phase + '/planar-generation-after-resize-or-reentry',
                               planar_generation is None or generation == planar_generation + 1)
                    planar_generation = generation
                    self.derived_targets.append({'phase': phase, 'window_pixels': pixels, 'reflection_pixels': half,
                                                  'HDR_actual_format_bytes': hbytes, 'planar_actual_format_bytes': pbytes})
                elif phase in clear or phase in ('load-b', 'load-a2'):
                    self.empty_planar(s['planar'], phase, 1)
                    self.check(phase + '/reset-keeps-generation-and-reusable-camera',
                               s['planar']['generation'] == planar_generation and s['planar']['camera_name'] == self.camera_name)
            if phase in clear or phase == 'components-destroyed':
                if phase in clear:
                    self.check(phase + '/live-World-before-normal-clear',
                               self.beginnings[phase]['snapshot']['world_present'] is True and
                               self.beginnings[phase]['snapshot']['sandbox_present'] is True)
                self.check(phase + '/actual-world-worker-owner-and-resident-caches-empty',
                           s['world_present'] is False and s['sandbox_present'] is False and s['loader_lifetime_owner_present'] is False and
                           event['world_instance'] in ('0', '0x0', '(nil)') and
                           all(s['cache'][key] == 0 for key in CACHE_COUNTS) and s['cache']['dynamic_shadow_off'] is True)
                if phase in clear:
                    self.check(phase + '/single-retained-reflection-camera', s['scene_camera_count'] == 2)
            else:
                if phase in ('load-b', 'load-a2'):
                    self.check(phase + '/empty-owner-before-normal-world-load',
                               self.beginnings[phase]['snapshot']['world_present'] is False and
                               self.beginnings[phase]['snapshot']['sandbox_present'] is False)
                self.check(phase + '/real-world-and-loader-lifetime-owner',
                           s['world_present'] is True and s['sandbox_present'] is True and s['loader_lifetime_owner_present'] is True and
                           isinstance(event['world_instance'], str) and bool(re.fullmatch(r'0x[0-9a-fA-F]+', event['world_instance'])) and
                           int(event['world_instance'], 16) > 0)
        baseline = self.checkpoints['clear-a1']['snapshot']['manager']
        for phase in ('clear-b', 'clear-a2'):
            for kind in ('texture', 'material', 'mesh', 'program', 'compositor'):
                facts = self.checkpoints[phase]['snapshot']['manager'][kind]
                self.check(phase + '/warm-empty-baseline-' + kind,
                           facts['names'] == baseline[kind]['names'] and facts['count'] == baseline[kind]['count'] and
                           facts['memory_bytes'] == baseline[kind]['memory_bytes'])
        for event in self.releases:
            self.retirement(event)
        target_release_phases = {owner: [event['phase'] for event in self.releases if event['owner'] == owner]
                                 for owner in ('hdr-target', 'planar-target', 'planar-materials', 'planar-camera')}
        self.check('retirement/exact-complete-owned-release-stage-coverage', target_release_phases == {
            'hdr-target': ['resize-small', 'resize-large', 'resize-restore', 'components-destroyed'],
            'planar-target': ['resize-small', 'resize-large', 'resize-restore', 'clear-a1', 'clear-b', 'clear-a2'],
            'planar-materials': ['clear-a1', 'clear-b', 'clear-a2'],
            'planar-camera': ['components-destroyed']})

    def report(self, error=None):
        return {'schema': SCHEMA, 'status': 'FAIL' if error else 'PASS_SCOPED_NATIVE_LIFECYCLE',
                'normal_input': False, 'ordinary_acceptance_closed': False,
                'scope': 'Same Root production resize/world unload-reentry/resource destruction engineering observations only.',
                'process_exit_code': self.exit_code, 'inputs': self.inputs,
                'check_count': len(self.checks),
                'pass_count': sum(c['status'] == 'PASS' for c in self.checks),
                'fail_count': sum(c['status'] == 'FAIL' for c in self.checks),
                'checks': self.checks, 'open': OPEN,
                'derived_targets': getattr(self, 'derived_targets', []),
                'partial_journal_release_anomalies': self.release_anomalies,
                **({'error': str(error)} if error else {})}


def audit(journal, summary, process_exit_code):
    instance = None
    try:
        instance = Audit(journal, summary, process_exit_code)
        instance.run()
        return instance.report()
    except Exception as error:
        if instance is not None:
            return instance.report(error)
        return {'schema': SCHEMA, 'status': 'FAIL', 'normal_input': False,
                'ordinary_acceptance_closed': False, 'check_count': 0,
                'checks': [], 'error': str(error), 'open': OPEN}


def fresh_output(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x', encoding='utf-8') as handle:
        json.dump(value, handle, ensure_ascii=False, indent=2, allow_nan=False)
        handle.write('\n')


def calibrate(journal, summary, process_exit_code, destination):
    """Negative copies of one real passing journal; never invented positives."""
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    journal_data = Path(journal).read_bytes()
    summary_data = Path(summary).read_bytes()
    events = [decode_json(line) for line in journal_data.decode('utf-8').splitlines()]
    metadata = decode_json(summary_data)
    positive = audit(journal, summary, process_exit_code)
    require(positive['status'] == 'PASS_SCOPED_NATIVE_LIFECYCLE', 'real original must pass before calibration')
    results = [{'name': 'original-native-positive', 'expected': 'PASS_SCOPED_NATIVE_LIFECYCLE',
                'actual': positive['status'], 'passed': True,
                'journal_sha256': hashlib.sha256(journal_data).hexdigest(),
                'summary_sha256': hashlib.sha256(summary_data).hexdigest()}]

    def checkpoint(records, phase):
        return next(record for record in records if record['event'] == 'checkpoint' and record['phase'] == phase)

    def release(records):
        return next(record for record in records if record['event'] == 'release')

    def missing(records, _):
        records[:] = [r for r in records if not (r['phase'] == 'clear-b' and r['event'] == 'checkpoint')]

    def duplicate(records, _):
        at = next(i for i, r in enumerate(records) if r['event'] == 'checkpoint')
        records.insert(at + 1, copy.deepcopy(records[at]))

    def timeout(records, _):
        records[-1]['elapsed_ms'] = MAX_WALL_MS + 1

    def claimed_normal(records, _):
        records[0]['normal_input'] = True

    # Remaining fact mutations are schema-specific and defined after reading
    # the production diagnostic's field contract.
    mutations = [('missing-checkpoint', missing), ('duplicate-checkpoint', duplicate),
                 ('global-timeout', timeout), ('false-ordinary-scope', claimed_normal)]
    mutations.extend(calibration_mutations(checkpoint, release))
    for name, mutate in mutations:
        records, altered = copy.deepcopy(events), copy.deepcopy(metadata)
        mutate(records, altered)
        if name not in ('missing-checkpoint', 'duplicate-checkpoint'):
            # A checkpoint/end pair is one observation emitted twice. Preserve
            # that protocol invariant so fact mutations reach their own gate.
            for phase in PHASES:
                source = checkpoint(records, phase)
                end = next(record for record in records if record['event'] == 'end' and record['phase'] == phase)
                for field in ('frame', 'world_epoch', 'world_instance', 'world_directory', 'world_id', 'snapshot'):
                    end[field] = copy.deepcopy(source[field])
        case_dir = destination / name
        case_dir.mkdir()
        case_journal, case_summary = case_dir / 'journal.jsonl', case_dir / 'summary.json'
        with case_journal.open('x', encoding='utf-8') as handle:
            for record in records:
                handle.write(json.dumps(record, ensure_ascii=False, allow_nan=False) + '\n')
        fresh_output(case_summary, altered)
        result = audit(case_journal, case_summary, 1 if name == 'nonzero-process-exit' else process_exit_code)
        fresh_output(case_dir / 'oracle.json', result)
        expected_gate = CALIBRATION_GATES[name]
        rejected_at_gate = result['status'] == 'FAIL' and expected_gate in result.get('error', '')
        results.append({'name': name, 'expected': 'FAIL', 'actual': result['status'],
                        'expected_rejection': expected_gate, 'passed': rejected_at_gate, 'error': result.get('error'),
                        'inputs': result.get('inputs', {})})
    return {'schema': 'hellomine3d-render-lifecycle-oracle-calibration-v1',
            'status': 'PASS_CALIBRATED' if all(case['passed'] for case in results) else 'FAIL',
            'source': 'Fault copies of one actual native lifecycle journal; no synthetic successful capture.',
            'case_count': len(results), 'positive_count': 1,
            'negative_count': len(results) - 1,
            'pass_count': sum(case['passed'] for case in results), 'cases': results,
            'normal_input': False, 'ordinary_acceptance_closed': False}


CALIBRATION_GATES = {
    'missing-checkpoint': '/checkpoint-before-end', 'duplicate-checkpoint': '/unique-checkpoint',
    'global-timeout': 'out-of-bound numeric', 'false-ordinary-scope': '/diagnostic-schema',
    'false-physical-dimensions': '/actual-window-and-viewport-dimensions',
    'resize-without-new-generation': '/HDR-generation-exactly-one-per-real-resize',
    'manager-name-retained': '/exact-old-manager-name-set', 'native-object-still-alive': '/exact-old-native-object-set',
    'missing-native-delete-observation': '/exact-old-native-object-set',
    'resident-cache-retained': '/actual-world-worker-owner-and-resident-caches-empty',
    'reflection-camera-retained-after-component-destruction': '/planar-references-released',
    'RGBA8-silent-storage-downgrade': '/resolved-real-half-texture', 'actual-native-GL-error': '/actual-complete-GL0',
    'World-ABA-epoch-reused': '/actual-A-B-A-world-epoch', 'menu-manager-baseline-growth': '/warm-empty-baseline-texture',
    'new-Root-instead-of-same-instance': '/same-live-Root-Scene-window',
    'false-hidden-scope': '/hidden-zero-input-nonperf', 'unexpected-ordinary-input': '/hidden-zero-input-nonperf',
    'unexpected-performance-capture': '/hidden-zero-input-nonperf', 'nonzero-process-exit': '/natural-success-required',
    'camera-not-bound-to-main-viewport': '/main-camera-bound-to-actual-window-viewport',
    'HDR-camera-viewport-not-restored': '/production-main-camera-viewport-restored',
}


def calibration_mutations(checkpoint, release):
    def set_snapshot(phase, field, value):
        def mutate(records, _):
            checkpoint(records, phase)['snapshot'][field] = value
        return mutate

    def false_dimensions(records, _):
        checkpoint(records, 'resize-small')['snapshot']['window_pixels'][0] += 1

    def unchanged_generation(records, _):
        checkpoint(records, 'resize-large')['snapshot']['hdr']['generation'] = checkpoint(records, 'resize-small')['snapshot']['hdr']['generation']

    def retained_name(records, _):
        release(records)['release']['after']['manager_names'][0]['alive'] = True

    def alive_native(records, _):
        release(records)['release']['after']['objects'][0]['alive'] = True

    def missing_native(records, _):
        release(records)['release']['after']['objects'].pop()

    def resident(records, _):
        checkpoint(records, 'clear-b')['snapshot']['cache']['sections'] = 1

    def camera(records, _):
        checkpoint(records, 'components-destroyed')['snapshot']['planar']['camera_count'] = 1

    def wrong_format(records, _):
        checkpoint(records, 'warm-b')['snapshot']['planar']['native']['resolved']['format'] = 32856

    def gl_error(records, _):
        checkpoint(records, 'warm-a2')['snapshot']['hdr']['native']['gl_error_after'] = 1282

    def wrong_aba(records, _):
        checkpoint(records, 'warm-a2')['world_epoch'] = 1

    def retained_manager(records, _):
        names = checkpoint(records, 'clear-a2')['snapshot']['manager']['texture']
        names['names'].append('General:RetainedOwnedRTT')
        names['names'].sort()
        names['count'] += 1

    def unrestored_camera(records, _):
        next(record for record in records if record['event'] == 'release' and record['owner'] == 'hdr-target')['release']['camera_viewport_restored'] = False

    return [('false-physical-dimensions', false_dimensions), ('resize-without-new-generation', unchanged_generation),
            ('manager-name-retained', retained_name), ('native-object-still-alive', alive_native),
            ('missing-native-delete-observation', missing_native), ('resident-cache-retained', resident),
            ('reflection-camera-retained-after-component-destruction', camera), ('RGBA8-silent-storage-downgrade', wrong_format),
            ('actual-native-GL-error', gl_error), ('World-ABA-epoch-reused', wrong_aba),
            ('menu-manager-baseline-growth', retained_manager),
            ('new-Root-instead-of-same-instance', set_snapshot('warm-b', 'root_instance', '0x1')),
            ('false-hidden-scope', set_snapshot('warm-a', 'hidden', False)),
            ('unexpected-ordinary-input', set_snapshot('warm-a', 'input_event_count', 1)),
            ('unexpected-performance-capture', set_snapshot('warm-a', 'perf_enabled', True)),
            ('camera-not-bound-to-main-viewport', set_snapshot('warm-a', 'main_viewport_bound', False)),
            ('HDR-camera-viewport-not-restored', unrestored_camera),
            ('nonzero-process-exit', lambda records, summary: None)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('journal', type=Path)
    parser.add_argument('summary', type=Path)
    parser.add_argument('--process-exit-code', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--calibrate', type=Path)
    args = parser.parse_args()
    result = audit(args.journal, args.summary, args.process_exit_code)
    fresh_output(args.output, result)
    if args.calibrate is not None:
        require(result['status'] == 'PASS_SCOPED_NATIVE_LIFECYCLE',
                'calibration requires an actually passing original journal')
        calibration = calibrate(args.journal, args.summary, args.process_exit_code, args.calibrate)
        fresh_output(args.calibrate / 'calibration-summary.json', calibration)
        if calibration['status'] != 'PASS_CALIBRATED':
            return 1
    print(json.dumps({'status': result['status'], 'check_count': result['check_count'],
                      'output': str(args.output)}, ensure_ascii=False))
    return 0 if result['status'] == 'PASS_SCOPED_NATIVE_LIFECYCLE' else 1


if __name__ == '__main__':
    raise SystemExit(main())
