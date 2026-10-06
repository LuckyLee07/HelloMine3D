#!/usr/bin/env python3
"""Validate/plan, or create a new ordinary WorkbenchCurrent from verified inputs.

This never launches a client. A fresh diagnostic capture establishes the authored
save's provenance; the resulting app uses its ordinary menu and mutable bin/saves.
The original app, old GoalWorkbench, published Current/Complete and capture stay read-only.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct

WORLD_NAME = '临水建筑完整样板'
DEFAULT_BUNDLE_ID = 'local.hellomine3d.reference-current-v1'
PUBLISHED_V2_BUNDLE_ID = 'local.hellomine3d.reference-current-v2'
PUBLISHED_V2_PATH = 'build/reference-visual-goal/HelloMine3D Reference Complete.app'
PUBLISHED_V3_BUNDLE_ID = 'local.hellomine3d.reference-current-v3'
PUBLISHED_V3_PATH = 'build/reference-visual-goal/HelloMine3D Reference Complete v3.app'
PUBLISHED_V4_BUNDLE_ID = 'local.hellomine3d.reference-current-v4'
PUBLISHED_V4_PATH = 'build/reference-visual-goal/HelloMine3D Reference Complete v4.app'
PUBLISHED_V5_BUNDLE_ID = 'local.hellomine3d.reference-current-v5'
PUBLISHED_V5_PATH = 'build/reference-visual-goal/HelloMine3D Reference Complete v5.app'
PUBLISHED_V6_BUNDLE_ID = 'local.hellomine3d.reference-current-v6'
PUBLISHED_V6_PATH = 'build/reference-visual-goal/HelloMine3D Reference Complete v6.app'
PUBLISHED_V7_BUNDLE_ID = 'local.hellomine3d.reference-current-v7'
PUBLISHED_V7_PATH = 'build/reference-visual-goal/HelloMine3D Reference Complete v7.app'
LAUNCHER = '''#!/bin/bash
set -euo pipefail
# Diagnostics from the preparing shell must not reach ordinary gameplay.
for name in $(env | cut -d= -f1); do
    case "$name" in HELLOMINE3D_*|HELLO_RENDER_*|HELLO_PERF_*|HELLO_VISUAL_*) unset "$name";; esac
done
package_root="$(cd "$(dirname "$0")/../Resources" && pwd)"
export HELLOMINE3D_ROOT="$package_root"
cd "$package_root/bin"
exec ./HelloMine3D "$@"
'''
RECORDS = {'objective_completed', 'objective_progress', 'inventory_slot', 'actor'}
SINGLETONS = set('version world_id world_name seed created_utc last_played_utc last_build spawn world_time generator terrain_generation_version exploration_reward_version difficulty_profile_version difficulty_id post_victory_event_version post_victory_completed_events alpha_journey_flags world_outcome_phase world_outcome_reward_epoch world_outcome_claimed_epoch objective_definition_version objective_completed_count objective_progress_count player_present player_position player_rotation player_held player_health player_food_cooldown player_attack_cooldown inventory_format inventory_count actor_count'.split())


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate_bundle_id(value=DEFAULT_BUNDLE_ID, protected=()):
    require(isinstance(value, str) and re.fullmatch(r'local\.hellomine3d\.[a-z0-9-]+', value), 'Invalid local HelloMine3D bundle ID')
    require(value not in protected, 'New bundle ID belongs to a protected app: '+value)
    return value


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def safe_relative(text):
    path = Path(text)
    require(text and not path.is_absolute() and '..' not in path.parts and path.as_posix() == text, 'Unsafe relative path: '+text)
    return path


def path_inside(path, boundary):
    """Include existing physical ancestors: resolve() retains APFS case aliases."""
    if path == boundary or path.is_relative_to(boundary):
        return True
    if boundary.exists():
        return any(candidate.exists() and candidate.samefile(boundary)
                   for candidate in (path, *path.parents))
    return False


def paths_overlap(first, second):
    return path_inside(first, second) or path_inside(second, first)


def tree_files(root):
    require(root.is_dir() and not root.is_symlink(), 'Expected real directory: '+str(root))
    files = {}
    for path in sorted(root.rglob('*')):
        require(not path.is_symlink(), 'Symlinks are not accepted: '+str(path))
        if path.is_file():
            files[path.relative_to(root).as_posix()] = sha(path)
    return files


def read_inventory(path, root):
    entries = {}
    for line in path.read_text().splitlines():
        digest, separator, text = line.partition('  ')
        require(separator and re.fullmatch('[0-9a-f]{64}', digest), 'Invalid SHA inventory line')
        relative = safe_relative(text)
        require(text not in entries and (root/relative).is_file(), 'Duplicate/missing inventory file: '+text)
        require(sha(root/relative) == digest, 'Inventory hash mismatch: '+text)
        entries[text] = digest
    require(entries, 'Empty SHA inventory')
    return entries


def read_world(path):
    raw = path.read_bytes()
    require(0 < len(raw) <= 65536, 'World metadata exceeds catalogue byte budget')
    fields, records = {}, {key: [] for key in RECORDS}
    for line in raw.decode('utf-8').splitlines():
        if not line.strip():
            continue
        key, separator, value = line.strip().partition(' ')
        require(separator and value and key in SINGLETONS | RECORDS, 'Unknown/empty world metadata field: '+key)
        if key in RECORDS:
            records[key].append(value)
        else:
            require(key not in fields, 'Duplicate world metadata field: '+key)
            fields[key] = value
    require(fields.keys() == SINGLETONS, 'Missing current v12 world fields: '+str(sorted(SINGLETONS-fields.keys())))
    def integer(key, low, high):
        require(re.fullmatch('-?[0-9]+', fields[key]), 'Invalid integer: '+key)
        value = int(fields[key]); require(low <= value <= high, 'World integer outside bounds: '+key)
        return value
    integer('version', 12, 12); integer('seed', -2147483648, 2147483647); integer('terrain_generation_version', 30, 30)
    integer('inventory_format', 2, 2); integer('difficulty_profile_version', 1, 1)
    integer('difficulty_id', 0, 2); integer('post_victory_event_version', 1, 1)
    integer('post_victory_completed_events', 0, 0); integer('exploration_reward_version', 1, 1)
    integer('objective_definition_version', 4, 4); integer('player_present', 1, 1)
    integer('alpha_journey_flags', 0, 0)
    for key in ['world_outcome_phase', 'world_outcome_reward_epoch', 'world_outcome_claimed_epoch']:
        integer(key, 0, 0)
    created = integer('created_utc', 946684800, 253402300799)
    integer('last_played_utc', created, 253402300799)
    require(re.fullmatch('[a-z0-9][a-z0-9_-]{0,63}', fields['world_id']), 'Noncanonical world ID')
    require(re.fullmatch('[a-zA-Z0-9_.+-]{1,80}', fields['last_build']), 'Noncanonical last_build')
    name = json.loads(fields['world_name'])
    require(isinstance(name, str) and 0 < len(name) <= 80 and name.strip(' ') == name and name not in ['.', '..'] and not any(ord(c) < 32 or ord(c) == 127 or c in '/\\"' for c in name), 'Invalid UTF-8 display name')
    require(fields['generator'] == 'ClassicOverWorld', 'Reference scene must retain its normal generator')
    for key, length in [('spawn', 3), ('player_position', 3), ('player_rotation', 3), ('world_time', 1), ('player_health', 1)]:
        values = fields[key].split(); require(len(values) == length, 'Invalid numeric field: '+key)
        require(all(math.isfinite(float(v)) and abs(float(v)) <= 3.402823466e38 for v in values), 'Nonfinite world field: '+key)
    require(0 <= float(fields['player_health']) <= 20, 'Invalid player health')
    integer('player_food_cooldown', 0, 1200); integer('player_attack_cooldown', 0, 1200)
    for count_key, record_key, limit in [('inventory_count', 'inventory_slot', 5), ('actor_count', 'actor', 0), ('objective_completed_count', 'objective_completed', 256), ('objective_progress_count', 'objective_progress', 256)]:
        require(integer(count_key, 0, limit) == len(records[record_key]), 'Metadata record count mismatch: '+count_key)
    integer('player_held', 0, max(0, len(records['inventory_slot'])-1))
    for slot in records['inventory_slot']:
        parts = slot.split(); require(len(parts) == 3 and all(re.fullmatch('[0-9]+', v) for v in parts), 'Invalid current inventory slot')
        material, amount, durability = map(int, parts)
        require(1 <= material <= 60 and 1 <= amount <= 64 and 0 <= durability <= 65535, 'Inventory slot outside current bounds')
    completed = records['objective_completed']; require(len(set(completed)) == len(completed), 'Duplicate completed IDs')
    require(all(re.fullmatch('[a-z0-9_.-]{1,128}', v) for v in completed), 'Noncanonical completed ID')
    progress_ids = set()
    for record in records['objective_progress']:
        parts = record.split(); require(len(parts) == 2 and re.fullmatch('[a-z0-9_.-]{1,128}', parts[0]) and re.fullmatch('[0-9]+', parts[1]) and int(parts[1]) <= 1000000, 'Invalid objective progress')
        require(parts[0] not in progress_ids and parts[0] not in completed, 'Duplicate/conflicting objective progress'); progress_ids.add(parts[0])
    return fields, raw.decode('utf-8')


def validate_chunk(path):
    match = re.fullmatch(r'chunk_(-?[0-9]+)_(-?[0-9]+)\.hmcchunk', path.name)
    require(match, 'Noncanonical chunk filename')
    data = path.read_bytes(); require(28 <= len(data) <= 32*1024*1024, 'Invalid reference chunk size')
    magic, version, x, z, size, sections = struct.unpack_from('<8sIiiII', data)
    require(magic == b'HMCHNK1\0' and version == 2 and (x, z) == tuple(map(int, match.groups())) and size == 16 and 0 <= sections <= 64, 'Invalid current chunk header')
    count = sections*4096; offset = 28+2*count
    require(offset+4 <= len(data), 'Truncated chunk blocks/metadata')
    ids, metadata = data[28:28+count], data[28+count:offset]
    require(all(v <= 44 for v in ids), 'Unknown reference block ID')
    require(all(v <= 3 for block, v in zip(ids, metadata) if 33 <= block <= 44), 'Invalid architectural metadata')
    entities = struct.unpack_from('<I', data, offset)[0]; offset += 4
    require(entities <= 4096, 'Block entity budget exceeded'); positions = set()
    for _ in range(entities):
        require(offset+12 <= len(data), 'Truncated entity position'); px, py, pz = struct.unpack_from('<iii', data, offset); offset += 12
        require(0 <= px < 16 and 0 <= pz < 16 and 0 <= py < sections*16 and (px, py, pz) not in positions, 'Invalid entity position'); positions.add((px, py, pz))
        texts = []
        for maximum in [128, 65536]:
            require(offset+4 <= len(data), 'Truncated entity length'); length = struct.unpack_from('<I', data, offset)[0]; offset += 4
            require(length <= maximum and offset+length <= len(data), 'Truncated/oversize entity'); texts.append(data[offset:offset+length]); offset += length
        kind = texts[0].decode('utf-8'); require(re.fullmatch('[a-z0-9_.-]+:[a-z0-9_./-]+', kind), 'Invalid entity stable ID')
        index = py*256+pz*16+px
        require({'hellomine:chest': 16, 'hellomine:furnace': 19, 'hellomine:crusher': 26}.get(kind) == ids[index], 'Entity attachment does not match block')
    require(offset == len(data), 'Trailing chunk bytes')
    return x, z



def prepare_metadata(fields, metadata, save):
    root = Path(__file__).resolve().parents[1]
    scene = json.loads((root/'media/sources/reference-visual-scene-v2.json').read_text())
    street = next(view for view in scene['views'] if view['name'] == 'street')
    point = list(map(float, fields['player_position'].split()))
    expected = [scene['origin'][i]+street['position'][i] for i in range(3)]
    require(all(abs(point[i]-expected[i]) <= .05 for i in range(3)), 'Fresh player state is not the safe saved street position')
    chunk_data = {}
    def block_at(x, y, z):
        cx, cz = x//16, z//16
        if (cx, cz) not in chunk_data:
            path = save/'chunks'/('chunk_'+str(cx)+'_'+str(cz)+'.hmcchunk')
            validate_chunk(path); data = path.read_bytes(); chunk_data[cx, cz] = data
        data = chunk_data[cx, cz]; sections = struct.unpack_from('<I', data, 24)[0]
        require(0 <= y < sections*16, 'Street body/support is outside stored height')
        return data[28+y*256+(z-cz*16)*16+(x-cx*16)]
    bounds = [(point[0]-.3, point[0]+.3), (point[1]-1, point[1]+1), (point[2]-.3, point[2]+.3)]
    for x in range(math.floor(bounds[0][0]+1e-6), math.ceil(bounds[0][1]-1e-6)):
        for y in range(math.floor(bounds[1][0]+1e-6), math.ceil(bounds[1][1]-1e-6)):
            for z in range(math.floor(bounds[2][0]+1e-6), math.ceil(bounds[2][1]-1e-6)):
                require(block_at(x, y, z) == 0, 'Street saved player body is obstructed')
    ground_y = math.floor(point[1]-1+1e-5)-1
    require(abs(point[1]-1-(ground_y+1)) <= .05 and block_at(math.floor(point[0]), ground_y, math.floor(point[2])) == 23, 'Street saved player has no actual full road support')
    prepared = re.sub(r'^world_name .*$', 'world_name '+json.dumps(WORLD_NAME, ensure_ascii=False), metadata, count=1, flags=re.M)
    prepared = re.sub(r'^spawn .*$', 'spawn '+fields['player_position'], prepared, count=1, flags=re.M)
    restored = prepared.replace('world_name '+json.dumps(WORLD_NAME, ensure_ascii=False), 'world_name '+fields['world_name'], 1)
    restored = re.sub(r'^spawn .*$', 'spawn '+fields['spawn'], restored, count=1, flags=re.M)
    require(restored == metadata, 'Only live display name and prepared spawn may change')
    return prepared


def validate_settings(path):
    text = path.read_text(); fields = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        key, separator, value = line.partition(' ')
        require(separator and value and key not in fields, 'Invalid duplicate settings key'); fields[key] = value
    required = set('settings_version renderdistance directionalshadowquality postprocessingquality fullscreen windowsize fov uiscale locale audiocaptions actionhints sprintmode sneakmode feedbackintensity mouse_break_attack mouse_use mouse_place mouse_guard seed visualdetail minimaprange cameraperspective renderpipeline'.split())
    require(fields.keys() == required, 'Provide the complete ordinary v12 menu config, without extra diagnostic keys')
    require(fields['settings_version'] == '12' and fields['fullscreen'] == '0' and fields['seed'] == 'random', 'Expected versioned ordinary windowed menu settings')
    require(1 <= int(fields['renderdistance']) <= 32 and 45 <= int(fields['fov']) <= 120 and .75 <= float(fields['uiscale']) <= 1.75, 'Menu settings outside bounds')
    require(fields['directionalshadowquality'] in ['off', 'medium', 'high'] and fields['postprocessingquality'] in ['off', 'on'] and fields['renderpipeline'] in ['legacy', 'linear-hdr'], 'Invalid graphics settings')
    require(fields['visualdetail'] in ['standard', 'compatibility'] and fields['minimaprange'] in ['64', '128', '256'] and fields['cameraperspective'] in ['first', 'third'] and fields['locale'] in ['zh-CN', 'en-US'], 'Invalid ordinary menu settings')
    require(all(fields[k] in ['0', '1'] for k in ['audiocaptions', 'actionhints']) and all(fields[k] in ['hold', 'toggle'] for k in ['sprintmode', 'sneakmode']) and fields['feedbackintensity'] in ['off', 'reduced', 'full'], 'Invalid gameplay settings')
    require(all(fields[k] in ['primary', 'secondary', 'middle'] for k in required if k.startswith('mouse_')), 'Invalid mouse binding')
    size = fields['windowsize'].split(); require(len(size) == 2 and 640 <= int(size[0]) <= 7680 and 480 <= int(size[1]) <= 4320, 'Invalid ordinary window dimensions')
    return text


def make_plan(args):
    root = Path(__file__).resolve().parents[1]
    app, capture_path, menu = args.app.resolve(strict=True), args.capture.resolve(strict=True), args.menu_config.resolve(strict=True)
    output = args.output.resolve()
    require(output.suffix == '.app' and 0 < len(output.stem) <= 80 and not any(ord(c) < 32 or ord(c) == 127 for c in output.stem) and not output.exists(), 'Output must be a new named app; refresh is never allowed')
    protection = json.loads(args.protection_receipt.read_text())
    protected = Path(protection['protected_app']).resolve(strict=True)
    old_work = Path(protection['work_app']).resolve(strict=True)
    snapshots = {}
    protected_ids = {PUBLISHED_V2_BUNDLE_ID, PUBLISHED_V3_BUNDLE_ID, PUBLISHED_V4_BUNDLE_ID, PUBLISHED_V5_BUNDLE_ID, PUBLISHED_V6_BUNDLE_ID, PUBLISHED_V7_BUNDLE_ID}
    protected_apps = [protected, old_work]
    current = root/'build/reference-visual-goal/WorkbenchCurrent.app'
    if current.exists():
        protected_apps.append(current.resolve(strict=True))
    for version, relative in [('v2', PUBLISHED_V2_PATH), ('v3', PUBLISHED_V3_PATH), ('v4', PUBLISHED_V4_PATH), ('v5', PUBLISHED_V5_PATH), ('v6', PUBLISHED_V6_PATH), ('v7', PUBLISHED_V7_PATH)]:
        published = (root/relative).resolve()
        require(not paths_overlap(output, published), 'Output overlaps the published '+version+' app')
        if published.exists():
            protected_apps.append(published.resolve(strict=True))
    for candidate in protected_apps:
        require(not paths_overlap(output, candidate), 'Output overlaps a protected app')
        if args.record:
            record = args.record.resolve()
            require(not paths_overlap(record, candidate), 'Evidence record overlaps a protected app')
        snapshots[str(candidate)] = tree_files(candidate)
        with (candidate/'Contents/Info.plist').open('rb') as stream:
            protected_ids.add(plistlib.load(stream)['CFBundleIdentifier'])
    for relative, expected in protection['protected_files'].items():
        require(snapshots[str(protected)].get(relative) == expected, 'Protected original has changed: '+relative)
    for relative, expected in protection['copied_save_files'].items():
        require(snapshots[str(old_work)].get(relative) == expected, 'Old GoalWorkbench save changed: '+relative)
    bundle_id = validate_bundle_id(getattr(args, 'bundle_id', DEFAULT_BUNDLE_ID), protected_ids)
    require(not any(paths_overlap(app, candidate) for candidate in protected_apps), 'A current independent Release source package is required')
    # Protect declared source/capture trees before validating version identities.
    # This also catches a save child reached through a case alias of its parent.
    for source in [app, capture_path.parent, menu, args.protection_receipt.resolve(strict=True)]:
        require(not paths_overlap(output, source), 'Output overlaps an input')
        if args.record:
            require(not paths_overlap(args.record.resolve(), source), 'Evidence record overlaps a read-only input or bundle')
    tree_files(app)
    resources = app/'Contents/Resources'
    managed = read_inventory(resources/'distribution-sha256.txt', app)
    require(not any(v.startswith('Contents/Resources/bin/saves/') or v == 'Contents/Resources/bin/config.txt' for v in managed), 'Mutable settings/saves must not enter managed distribution inventory')
    identity = json.loads((resources/'build-identity.json').read_text())
    require(identity['platform'] == 'macOS' and identity['configuration'] == 'Release' and identity['source_tree_required'] is False and identity['signed_or_notarized'] is False, 'Expected standalone verified Release package')
    require(identity['executable_sha256'] == args.expected_executable_sha256 == sha(resources/'bin/HelloMine3D'), 'Release executable identity mismatch')
    require((resources/'bin/HelloMine3D').read_bytes()[:4] in [b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe'] and os.access(resources/'bin/HelloMine3D', os.X_OK), 'Expected executable Mach-O client')
    require(identity['resource_manifest_sha256'] == sha(resources/'media/resource-manifest.txt') and identity['source_manifest_sha256'] == sha(resources/'source-tree-sha256.txt'), 'Runtime/source receipt identity mismatch')
    manifest_lines = (resources/'media/resource-manifest.txt').read_text().splitlines()
    require(manifest_lines[0] == '# HelloMine3D resource manifest v1', 'Invalid resource manifest header')
    manifest_entries = [line for line in manifest_lines[1:] if line and not line.startswith('#')]
    require(manifest_entries == sorted(set(manifest_entries)), 'Unsorted/duplicate resource manifest')
    for line in manifest_entries:
        if not line or line.startswith('#'):continue
        category, separator, relative = line.partition('|'); safe_relative(relative)
        require(separator and re.fullmatch('[a-z][a-z-]*', category) and 'Contents/Resources/'+relative in managed, 'Resource not managed by release package')
    with (app/'Contents/Info.plist').open('rb') as stream:info = plistlib.load(stream)
    require(info['CFBundleIdentifier'] == identity['bundle_id'], 'Source bundle identity mismatch')
    sources = read_inventory(resources/'source-tree-sha256.txt', root)
    for path in ['src/HelloMine3D/Diagnostics/ReferenceVisualScene.cpp', 'src/HelloMine3D/Diagnostics/ReferenceVisualSceneData.inc']:
        require(sources.get(path) == sha(root/path), 'Current frozen scene source identity mismatch')
    capture = json.loads(capture_path.read_text())
    require(capture['result'] == 'CAPTURED' and capture['normal_input'] is False and capture['save_template'] is None and capture.get('reference_view') == 'street', 'Use a successful fresh street capture, never a reused template')
    for key in ['executable_sha256', 'resource_manifest_sha256', 'source_manifest_sha256']:
        require(capture['package_identity'][key] == identity[key], 'Fresh capture/package mismatch: '+key)
    save = Path(capture['environment']['HELLOMINE3D_SAVE_DIR']).resolve(strict=True)
    require(save == capture_path.parent/'save', 'Fresh save must be the capture sibling directory')
    for source in [app, save, menu, capture_path.parent, args.protection_receipt.resolve()]:
        require(not paths_overlap(output, source), 'Output overlaps an input')
    saved_files = tree_files(save); fields, metadata = read_world(save/'world.meta')
    require(capture['world_metadata'] == metadata, 'Save metadata changed after capture')
    chunks = {validate_chunk(path) for path in sorted((save/'chunks').glob('*.hmcchunk'))}
    require({(x, z) for x in range(11, 15) for z in range(-14, -10)} <= chunks, 'Fresh save lacks resident authored site chunks')
    log = (capture_path.parent/'client.log').read_text()
    scene_lines = [line for line in log.splitlines() if line.startswith('[REFERENCE_VISUAL_SCENE] ')]
    require(len(scene_lines) == 1, 'Fresh capture needs exactly one real scene construction record')
    facts = dict(re.findall(r'(\w+)=([^ ]+)', scene_lines[0]))
    for key, expected in [('version', '2'), ('saved', '1'), ('normal_world', '1'), ('view', 'street'), ('scene_chunks', '16'), ('weather_shell_checks', '443'), ('weather_roof_checks', '49'), ('planted_trees', '2'), ('planted_tree_cells', '159'), ('tree_owner_override', '0')]:
        require(facts.get(key) == expected, 'Fresh scene fact missing/mismatched: '+key)
    require(int(facts['edits']) == 62967 <= int(facts['budget']) == 72000, 'Fresh final scene budget mismatch')
    config = validate_settings(menu)
    renamed = prepare_metadata(fields, metadata, save)
    if args.record:
        record = args.record.resolve()
        for directory in protected_apps+[app, save, capture_path.parent, output]:
            require(not paths_overlap(record, directory), 'Evidence record overlaps a read-only input or bundle')
    plan = {'schema': 1, 'status': 'VALIDATED_PLAN', 'source_app': str(app), 'source_capture': str(capture_path), 'source_save': str(save), 'output': str(output), 'bundle_id': bundle_id, 'bundle_name': output.stem, 'protected_bundle_ids': sorted(protected_ids), 'executable_sha256': identity['executable_sha256'], 'source_manifest_sha256': identity['source_manifest_sha256'], 'resource_manifest_sha256': identity['resource_manifest_sha256'], 'world_id': fields['world_id'], 'world_name': WORLD_NAME, 'save_target': 'Contents/Resources/bin/saves/'+fields['world_id'], 'save_input_files': saved_files, 'managed_input_files': managed, 'menu_config_sha256': sha(menu), 'protected_snapshots': snapshots, 'scene_facts': facts, 'launch_count': 0, 'normal_input': 'NOT_RUN', 'runtime_fixture': False, 'runtime_diagnostic_environment': False, 'mutable_paths': ['Contents/Resources/bin/config.txt', 'Contents/Resources/bin/saves/**', 'Contents/Resources/bin/Mine.cfg', 'Contents/Resources/bin/MineResources.cfg', 'Contents/Resources/bin/MineOgre.log', 'Contents/Resources/bin/imgui-ogre.ini', 'Contents/Resources/bin/resource-packs.txt'], 'live_state_except_display_name_and_spawn_preserved': True, 'world_id_preserved': True, 'preparation': {'ordinary_input': False, 'initial_authored_fixture': True, 'live_display_name_changed': True, 'original_spawn': fields['spawn'], 'prepared_spawn_body_center': fields['player_position'], 'spawn_from_actual_saved_player_position': True, 'actual_saved_body_and_road_support_checked': True}, 'backup_bytes_preserved': True, 'backup_restore_keeps_original_backup_name_and_spawn': True}
    return plan, app, save, menu, renamed


def create(plan, app, save, menu, metadata):
    bundle_id = validate_bundle_id(plan.get('bundle_id', DEFAULT_BUNDLE_ID), plan.get('protected_bundle_ids', ()))
    output = Path(plan['output']); output.mkdir(parents=True, exist_ok=False)
    for relative in plan['managed_input_files']:
        source, target = app/relative, output/relative
        target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(source, target)
    resources = output/'Contents/Resources'; launcher = output/'Contents/MacOS/HelloMine3D'
    launcher.write_text(LAUNCHER); launcher.chmod(0o755)
    with (output/'Contents/Info.plist').open('rb') as stream:info = plistlib.load(stream)
    info.update(CFBundleName=output.stem, CFBundleIdentifier=bundle_id)
    with (output/'Contents/Info.plist').open('wb') as stream:plistlib.dump(info, stream)
    identity = json.loads((resources/'build-identity.json').read_text()); identity.update(bundle_id=info['CFBundleIdentifier'], acceptance='NOT_RUN', workbench_origin={'source_app': str(app), 'source_capture': plan['source_capture'], 'normal_menu': True, 'runtime_fixture': False})
    (resources/'build-identity.json').write_text(json.dumps(identity, indent=2)+'\n')
    shutil.copy2(menu, resources/'bin/config.txt')
    world = output/plan['save_target']; shutil.copytree(save, world); (world/'world.meta').write_text(metadata)
    managed = {relative: sha(output/relative) for relative in plan['managed_input_files']}
    changed_managed = {'Contents/MacOS/HelloMine3D', 'Contents/Info.plist', 'Contents/Resources/build-identity.json'}
    require(all(digest == plan['managed_input_files'][relative] for relative, digest in managed.items() if relative not in changed_managed), 'Managed copy bytes changed')
    copied_save = tree_files(world)
    require(all(copied_save.get(relative) == digest for relative, digest in plan['save_input_files'].items() if relative != 'world.meta'), 'Save payload or backup changed during copy')
    (resources/'distribution-sha256.txt').write_text(''.join(value+'  '+key+'\n' for key, value in sorted(managed.items())))
    plan.update(status='PREPARED', bundle_id=bundle_id, bundle_name=output.stem, managed_output_files=managed, initial_mutable_save_files=tree_files(world), initial_mutable_config_sha256=sha(resources/'bin/config.txt'), distribution_manifest_sha256=sha(resources/'distribution-sha256.txt'))
    for directory, expected in plan['protected_snapshots'].items():require(tree_files(Path(directory)) == expected, 'Protected input changed during preparation: '+directory)
    require(tree_files(save) == plan['save_input_files'], 'Source save changed during preparation')
    read_world(world/'world.meta'); read_inventory(resources/'distribution-sha256.txt', output)
    (resources/'workbench-handoff.json').write_text(json.dumps(plan, indent=2, ensure_ascii=False)+'\n')
    return plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--menu-config', type=Path, required=True)
    parser.add_argument('--expected-executable-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--bundle-id', default=DEFAULT_BUNDLE_ID, help='Independent local.hellomine3d.[a-z0-9-]+ app identity')
    parser.add_argument('--protection-receipt', type=Path, default=Path(__file__).resolve().parents[1]/'build/reference-visual-goal/ordinary-a-r1/package-receipt.json')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--create', action='store_true')
    mode.add_argument('--plan-only', action='store_true')
    mode.add_argument('--validate', action='store_true', help='Read-only validation/plan; no app writes')
    parser.add_argument('--record', type=Path, help='Explicit new plan/evidence JSON path')
    args = parser.parse_args()
    if args.record:
        require(not args.record.exists(), 'Evidence record must be new')
    try:
        plan, app, save, menu, metadata = make_plan(args)
        if args.create:plan = create(plan, app, save, menu, metadata)
        if args.record:
            require(not any(paths_overlap(args.record.resolve(), Path(p)) for p in plan['protected_snapshots']), 'Record may not touch protected inputs')
            args.record.parent.mkdir(parents=True, exist_ok=True); args.record.write_text(json.dumps(plan, indent=2, ensure_ascii=False)+'\n')
        print('[REFERENCE_WORKBENCH] '+json.dumps({key: plan[key] for key in ['status', 'output', 'bundle_id', 'bundle_name', 'world_id', 'world_name', 'executable_sha256', 'launch_count', 'runtime_fixture']}, ensure_ascii=False, sort_keys=True))
    except (ValueError, KeyError, OSError, json.JSONDecodeError, struct.error) as error:
        parser.error(str(error))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
