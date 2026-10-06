#!/usr/bin/env python3
"""Record three real normal-config restarts over one isolated saved world.

This engineering runner changes only visualdetail/renderpipeline between natural
process exits. It never injects input, pose, seed or world time. Its completed
status still requires an independent audit; it cannot prove ordinary settings UI.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import struct
import subprocess
import time

from capture_visual_macos import clone_verified_package, verified_package_entries
from prepare_reference_workbench import paths_overlap, tree_files, validate_settings
from run_reference_world_edit_macos import current_source_matches
from run_reference_render_lifecycle_macos import (
    certify_loaded_executable, digest, game_census, reject_symlinks, require, write_record,
)

MAGIC = "HelloMine3D owned reference settings restart session v1\n"
ENTRY = "HELLOMINE3D_REFERENCE_SETTINGS_RESTART_DIR"
PREFIXES = ("HELLOMINE3D_", "HELLO_RENDER_", "HELLO_PERF_", "HELLO_VISUAL_")
STAGES = (("stage-1-standard-hdr", "standard", "linear-hdr"),
          ("stage-2-compatibility-legacy", "compatibility", "legacy"),
          ("stage-3-standard-hdr", "standard", "linear-hdr"))
SAMPLES = ((201, 69, -186, 44, 0), (194, 66, -183, 7, 0),
           (199, 68, -196, 35, 0), (196, 66, -184, 33, 2))
PROTECTED_RELATIVE = (
    "build/reference-visual-v1/HelloMine3D Reference Visual.app",
    "build/reference-visual-goal/GoalWorkbench.app",
    "build/reference-visual-goal/WorkbenchCurrent.app",
    "build/reference-visual-goal/HelloMine3D Reference Complete.app",
    *(f"build/reference-visual-goal/HelloMine3D Reference Complete v{v}.app" for v in range(3, 9)),
)
FUTURE_PROTECTED_RELATIVE = "build/reference-visual-goal/HelloMine3D Reference Complete v8.app"
RECORD_FIELDS = {"inventory_slot", "actor", "objective_completed", "objective_progress"}


def utc():
    return datetime.now(timezone.utc).isoformat()


def profile_config(original, detail, pipeline):
    """Replace just two exact singleton lines, preserving all other bytes."""
    result = original
    for key, value in ((b"visualdetail", detail.encode()), (b"renderpipeline", pipeline.encode())):
        pattern = rb"(?m)^" + key + rb" [^\r\n]+(?=\r?$)"
        require(len(re.findall(pattern, result)) == 1, "Missing/duplicate profile key: " + key.decode())
        result = re.sub(pattern, key + b" " + value, result)
    return result


def metadata(path):
    """Read independent disk facts; unlike runtime identity, these are not GL/World observations."""
    raw = path.read_bytes()
    require(0 < len(raw) <= 65536, "World metadata size invalid")
    fields, records = {}, {key: [] for key in RECORD_FIELDS}
    for line in raw.decode("utf-8").splitlines():
        if not line.strip():
            continue
        key, separator, value = line.partition(" ")
        require(separator and value, "Invalid metadata line")
        if key in RECORD_FIELDS:
            records[key].append(value)
        else:
            require(key not in fields, "Duplicate metadata field: " + key)
            fields[key] = value
    require(fields.get("version") == "12" and fields.get("player_present") == "1",
            "A current actual saved Player state is required")
    require(re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", fields.get("world_id", "")), "Invalid saved world id")
    def vector(key):
        values = list(map(float, fields[key].split()))
        require(len(values) == 3 and all(math.isfinite(v) for v in values), "Invalid metadata vector: " + key)
        return values
    inventory = []
    for value in records["inventory_slot"]:
        slot = list(map(int, value.split()))
        require(len(slot) == 3 and slot[0] > 0 and 0 < slot[1] <= 64 and 0 <= slot[2] <= 65535,
                "Invalid metadata inventory")
        inventory.append(dict(zip(("material", "amount", "durability"), slot)))
    require(len(inventory) == int(fields["inventory_count"]), "Inventory count mismatch")
    require(len(records["actor"]) == int(fields["actor_count"]), "Actor count mismatch")
    world_time, health = float(fields["world_time"]), float(fields["player_health"])
    require(math.isfinite(world_time) and world_time >= 0 and math.isfinite(health), "Nonfinite saved state")
    return {"fields": fields, "records": records, "sha256": hashlib.sha256(raw).hexdigest(),
            "world": {"world_id": fields["world_id"], "seed": int(fields["seed"]),
                      "terrain_generation_version": int(fields["terrain_generation_version"])},
            "world_time": world_time,
            "player": {"position": vector("player_position"), "rotation": vector("player_rotation"),
                       "held": int(fields["player_held"]), "health": health,
                       "food_cooldown": int(fields["player_food_cooldown"]),
                       "attack_cooldown": int(fields["player_attack_cooldown"]), "inventory": inventory}}


def saved_cells(save):
    cache, cells = {}, []
    for x, y, z, block, meta in SAMPLES:
        chunk = (x // 16, z // 16)
        if chunk not in cache:
            path = save / "chunks" / f"chunk_{chunk[0]}_{chunk[1]}.hmcchunk"
            data = path.read_bytes()
            require(len(data) >= 28, "Truncated saved chunk")
            magic, version, cx, cz, size, sections = struct.unpack_from("<8sIiiII", data)
            count = sections * 4096
            require(magic == b"HMCHNK1\0" and version == 2 and (cx, cz) == chunk and size == 16
                    and 0 < sections <= 64 and len(data) >= 28 + count * 2 + 4,
                    "Invalid target saved chunk")
            cache[chunk] = (path, data, count)
        path, data, count = cache[chunk]
        index = y * 256 + (z % 16) * 16 + x % 16
        require(0 <= index < count, "Target above saved sections")
        actual = (data[28 + index], data[28 + count + index])
        require(actual == (block, meta), "Saved authored target differs: " + str((x, y, z)))
        cells.append({"position": [x, y, z], "id": actual[0], "metadata": actual[1],
                      "chunk": [chunk[0], chunk[1]], "chunk_file": str(path), "chunk_sha256": digest(path)})
    return cells


def close_numbers(actual, expected, label):
    if isinstance(expected, (int, float)):
        require(isinstance(actual, (int, float)) and not isinstance(actual, bool) and math.isfinite(actual)
                and abs(actual - expected) <= 0.0001, "Saved/native numeric mismatch: " + label)
    elif isinstance(expected, list):
        require(isinstance(actual, list) and len(actual) == len(expected), "Saved/native length mismatch: " + label)
        for i, value in enumerate(expected):
            close_numbers(actual[i], value, f"{label}[{i}]")
    elif isinstance(expected, dict):
        require(isinstance(actual, dict), "Saved/native object mismatch: " + label)
        for key, value in expected.items():
            require(key in actual, "Missing native state: " + label + "." + key)
            close_numbers(actual[key], value, label + "." + key)
    else:
        require(actual == expected, "Saved/native value mismatch: " + label)


def finite_tree(value):
    if isinstance(value, float):
        require(math.isfinite(value), "Nonfinite native observation")
    elif isinstance(value, dict):
        for child in value.values():
            finite_tree(child)
    elif isinstance(value, list):
        for child in value:
            finite_tree(child)


def require_integer(value, label, minimum=0):
    require(type(value) is int and value >= minimum, "Native integer required: " + label)


def require_real(value, label):
    require(type(value) in (int, float) and math.isfinite(value), "Native finite number required: " + label)


def native_target(facts, width, height, samples):
    for key in ("target_count", "depth_count", "observer_failures"):
        require_integer(facts[key], "target." + key)
    require(facts["active"] is True and facts["target_count"] == 1 and facts["depth_count"] == 1
            and facts["owned_depth_attached"] is True and facts["observer_failures"] == 0,
            "Actual owned render target facts invalid")
    native = facts["native"]
    for key in ("gl_error_before", "gl_error_after", "resolve_fbo", "draw_fbo"):
        require_integer(native[key], "target.native." + key)
    require(native["complete"] is True and native["gl_error_before"] == native["gl_error_after"] == 0
            and native["resolve_fbo"] > 0 and native["draw_fbo"] > 0, "Actual native FBO invalid")
    for name, count in (("colour", samples), ("resolved", 0), ("depth", samples)):
        item = native[name]
        for key in ("object", "width", "height", "samples", "format"):
            require_integer(item[key], "target.native." + name + "." + key)
        require(item["object"] > 0 and [item["width"], item["height"], item["samples"]] == [width, height, count],
                "Actual render attachment invalid: " + name)
    require(native["colour"]["format"] == native["resolved"]["format"] == 34842, "Actual colour is not RGBA16F")


def check_snapshot(snapshot, stage, pid, resources, save, disk_before):
    """Reject missing core numeric observations; completion remains pending independent audit."""
    name, detail, pipeline = stage
    finite_tree(snapshot)
    for key in ("pid", "frame", "input_event_count", "simulation_updates"):
        require_integer(snapshot[key], key)
    for key in ("elapsed_ms", "loaded_world_time", "world_time", "simulation_delta"):
        require_real(snapshot[key], key)
    require(snapshot["schema"] == "hellomine3d-reference-settings-restart-observation-v1"
            and snapshot["status"] == "OBSERVED" and snapshot["pid"] == pid and snapshot["stage"] == name,
            "Wrong actual single-frame observation identity")
    require(snapshot["frame"] > 0 and snapshot["elapsed_ms"] > 0 and snapshot["input_event_count"] == 0,
            "Invalid observation frame/input")
    config = snapshot["config"]
    require(config["path"] == str(resources / "bin/config.txt") and config["visualdetail"] == detail
            and config["renderpipeline"] == pipeline, "Native config differs from normal saved config")
    loaded = snapshot["loaded_world"]
    close_numbers(loaded, disk_before["world"], "loaded_world")
    require(loaded["disk_world_id"] == disk_before["world"]["world_id"]
            and loaded["save_directory"] == str(save), "Wrong same-save runtime/disk lineage")
    close_numbers(snapshot["loaded_player"], disk_before["player"], "loaded_player")
    close_numbers(snapshot["loaded_world_time"], disk_before["world_time"], "loaded_world_time")
    close_numbers(snapshot["world"], disk_before["world"], "observed_world")
    require(snapshot["simulation_delta"] > 0 and snapshot["simulation_updates"] > 0
            and snapshot["world_time"] > snapshot["loaded_world_time"], "Normal simulation/time did not advance")
    close_numbers(snapshot["player"]["inventory"], disk_before["player"]["inventory"], "observed_inventory")
    actual_cells = snapshot["cells"]
    require(len(actual_cells) == len(SAMPLES), "Missing native resident cells")
    for cell, (x, y, z, block, meta) in zip(actual_cells, SAMPLES):
        require_integer(cell["id"], "cell.id")
        require_integer(cell["metadata"], "cell.metadata")
        require_integer(cell["observed_frame"], "cell.observed_frame", minimum=1)
        require(cell["observation"] == "blocking-World.getBlock-find-only-nonAir"
                and cell["observed_frame"] == snapshot["frame"],
                "Actual loaded-cell observation domain/frame differs")
        require(cell["position"] == [x, y, z] and cell["id"] == block and cell["id"] > 0 and cell["metadata"] == meta
                and cell["known"] is True and cell["chunk"] == [x // 16, z // 16],
                "Actual loaded non-Air authored content differs")
    window = snapshot["window"]
    for key in ("width", "height", "samples", "gl_error"):
        require_integer(window[key], "window." + key)
    require(0 < window["width"] * window["height"] <= 8294400 and window["width"] > 0 and window["height"] > 0
            and window["gl_error"] == 0 and window["bindings_restored"] is True
            and window["samples"] == (4 if pipeline == "linear-hdr" else 0), "Native window storage differs")
    for key in ("terrain_draw", "water_draw"):
        draw = snapshot[key]
        for field in ("frame", "program", "primitive_count", "gl_error", "instance_count", "expected_triangles"):
            require_integer(draw[field], key + "." + field)
        require(isinstance(draw["source_origin"], list) and len(draw["source_origin"]) == 3
                and isinstance(draw["source_section"], list) and len(draw["source_section"]) == 3,
                "Missing actual source section geometry: " + key)
        for origin, section in zip(draw["source_origin"], draw["source_section"]):
            require_real(origin, key + ".source_origin")
            require_integer(section, key + ".source_section", minimum=-2147483648)
            require(section <= 2147483647 and section == math.floor(origin / 16),
                    "Actual source section differs from rendered origin or signed coordinate bounds")
        require(draw["production_attached_shaders"] is True and draw["use_indexes"] is True
                and draw["instance_count"] == 1 and draw["primitive_count"] == draw["expected_triangles"],
                "Actual production indexed draw/primitive certificate differs: " + key)
        require_real(draw["uniforms"]["linearHdrMode"], key + ".linearHdrMode")
        require(draw["frame"] == snapshot["frame"] and draw["view"] == "main" and draw["program"] > 0
                and draw["program_linked"] is True and draw["primitive_count"] > 0
                and draw["gl_error"] == 0 and draw["state_restored"] is True,
                "Missing actual same-frame linked production draw: " + key)
        require(draw["uniforms"]["linearHdrMode"] == (1 if pipeline == "linear-hdr" else 0),
                "Actual linked HDR uniform differs")
    terrain, water = snapshot["terrain_draw"], snapshot["water_draw"]
    require_real(water["uniforms"]["planarReflectionEnabled"], "Water.planarReflectionEnabled")
    sampler_maps = {}
    for role, draw in (("terrain", terrain), ("water", water)):
        samplers = draw["samplers"]
        require(isinstance(samplers, list) and 0 < len(samplers) <= 8,
                "Actual " + role + " sampler observations missing")
        by_name = {s["name"]: s for s in samplers}
        require(len(by_name) == len(samplers), "Duplicate native sampler")
        for s in samplers:
            for key in ("location", "unit", "target", "texture", "width", "height", "depth", "internal_format", "mip_levels"):
                require_integer(s[key], role + ".sampler." + key)
            disabled_planar = (role == "water" and s["name"] == "planarReflectionTexture"
                               and water["uniforms"]["planarReflectionEnabled"] == 0)
            require(type(s["sampling_enabled"]) is bool
                    and s["sampling_enabled"] is (not disabled_planar),
                    "Sampler consumption differs from actual linked uniform: " + role + "." + s["name"])
            require(s["location"] >= 0 and 0 <= s["unit"] < 32 and s["target"] in (3553, 35866)
                    and all(s[key] >= 0 for key in ("texture", "width", "height", "depth", "internal_format", "mip_levels")),
                    "Invalid actual sampler query: " + role + "." + s["name"])
            if not disabled_planar or s["texture"] > 0:
                require(s["texture"] > 0 and s["width"] > 0 and s["height"] > 0 and s["depth"] > 0
                        and s["internal_format"] > 0 and s["mip_levels"] > 0,
                        "Consumed/bound sampler lacks actual storage: " + role + "." + s["name"])
            else:
                require(all(s[key] == 0 for key in ("width", "height", "depth", "internal_format", "mip_levels")),
                        "Unbound disabled planar sampler reports invented storage")
        require_integer(draw["array_sampler_count"], role + ".array_sampler_count")
        require(draw["array_sampler_count"] == sum(s["target"] == 35866 for s in samplers),
                "Actual array sampler count inconsistent: " + role)
        sampler_maps[role] = by_name
    by_name = sampler_maps["terrain"]
    array_count = terrain["array_sampler_count"]
    planar_sampler = sampler_maps["water"]["planarReflectionTexture"]
    require(planar_sampler["target"] == 3553, "Actual Water planar sampler is not 2D")
    if pipeline == "linear-hdr":
        require("Surface" in terrain["fragment_program"] and array_count == 3, "HDR actual surface program/arrays missing")
        for key in ("terrainArray", "terrainNormalArray", "terrainSurfaceArray"):
            s = by_name[key]
            require([s["target"], s["width"], s["height"], s["depth"], s["mip_levels"]] == [35866, 64, 64, 256, 7],
                    "Actual reference array storage differs: " + key)
        native_target(snapshot["hdr"], window["width"], window["height"], 4)
        require_real(snapshot["spatial_aa_strength"], "actual HdrResolve spatial_aa_strength")
        require(type(snapshot["spatial_aa_enabled"]) is bool
                and snapshot["spatial_aa_enabled"] is (snapshot["spatial_aa_strength"] > .5)
                and snapshot["spatial_aa_strength"] == 0
                and snapshot["spatial_aa_observation_domain"] == "actual-HdrResolve-Ogre-pass-parameter",
                "MSAA normal policy/actual parameter value should disable spatial FXAA")
        planar = snapshot["planar"]
        require_integer(planar["update_count"], "planar.update_count")
        require_integer(planar["frame"], "planar.frame")
        native_target(planar, (window["width"] + 1) // 2, (window["height"] + 1) // 2, 0)
        require(planar["update_count"] > 0 and planar["frame"] == snapshot["frame"]
                and planar["binder_bound"] is True and planar["water_sampler_bound"] is True
                and water["uniforms"]["planarReflectionEnabled"] == 1, "Standard actual current reflection missing")
        resolved = planar["native"]["resolved"]
        require(planar_sampler["sampling_enabled"] is True
                and [planar_sampler["texture"], planar_sampler["width"], planar_sampler["height"], planar_sampler["internal_format"]]
                == [resolved["object"], resolved["width"], resolved["height"], resolved["format"]],
                "Consumed Water planar sampler differs from actual resolved reflection attachment")
    else:
        for name in ("hdr", "planar"):
            for field in ("target_count", "depth_count"):
                require_integer(snapshot[name][field], name + "." + field)
            for field in ("draw_fbo", "resolve_fbo"):
                require_integer(snapshot[name]["native"][field], name + ".native." + field)
        require("Surface" not in terrain["fragment_program"] and array_count == 0
                and not any(key in by_name for key in ("terrainArray", "terrainNormalArray", "terrainSurfaceArray")),
                "Compatibility still consumes array/surface path")
        require(by_name["terrainAtlas"]["target"] == 3553, "Compatibility actual atlas binding absent")
        require(snapshot["hdr"]["active"] is False and snapshot["hdr"]["target_count"] == 0
                and snapshot["hdr"]["depth_count"] == 0 and snapshot["planar"]["active"] is False
                and snapshot["planar"]["target_count"] == snapshot["planar"]["depth_count"] == 0
                and snapshot["hdr"]["native"]["draw_fbo"] == snapshot["hdr"]["native"]["resolve_fbo"] == 0
                and snapshot["planar"]["native"]["draw_fbo"] == snapshot["planar"]["native"]["resolve_fbo"] == 0
                and water["uniforms"]["planarReflectionEnabled"] == 0, "Legacy actual HDR/reflection remains active")
        require(snapshot["spatial_aa_observation_domain"] == "actual-HdrPipeline-inactive"
                and snapshot["spatial_aa_strength"] is None and snapshot["spatial_aa_enabled"] is False,
                "Inactive legacy HDR must not claim an unread Resolve parameter")
    for key in ("waterDetailStrength", "waterBoundaryPinsV1"):
        require_real(water["uniforms"][key], "Water." + key)
        require(key in water["uniforms"] and 0 <= water["uniforms"][key] <= 1, "Missing actual Water uniform: " + key)
    return [window["width"], window["height"]]


def stage_environment(resources, session, stage):
    stage_path = session / stage[0]
    return {"HELLOMINE3D_ROOT": str(resources), "HELLOMINE3D_WINDOW_HIDDEN": "1",
            "HELLOMINE3D_SAVE_DIR": str(session / "save"), "HELLOMINE3D_CATALOGUE_DIR": str(session / "catalogue"),
            ENTRY: str(stage_path / "restart-facts"), "HELLO_RENDER_CAPTURE": "1",
            "HELLO_RENDER_CAPTURE_DIR": str(stage_path / "frames"), "HELLO_RENDER_CAPTURE_MS": "3500,7000",
            "HELLO_RENDER_CAPTURE_MAX_DELTA_MS": "5000", "HELLO_RENDER_CAPTURE_EXIT": "1"}


def png_dimensions(path):
    raw = path.read_bytes()
    require(len(raw) > 32 and raw[:8] == b"\x89PNG\r\n\x1a\n" and raw[12:16] == b"IHDR", "Capture is not original PNG")
    return list(struct.unpack_from(">II", raw, 16))


def protected_inventory(root):
    result = {}
    for relative in PROTECTED_RELATIVE:
        path = root / relative
        # v8 is a protected future delivery path: absent before creation, then
        # mandatory full-tree protection as soon as it exists. Nine old apps stay required.
        if relative == FUTURE_PROTECTED_RELATIVE and not path.exists():
            require(not path.is_symlink(), "Future published app may not be a dangling symlink")
            continue
        require(path.is_dir(), "Expected published app missing: " + str(path))
        reject_symlinks(path)
        result[str(path)] = tree_files(path)
    return result


def output_boundary(output, inputs):
    require(not output.exists(), "Session output must be new; no refresh")
    require(not any(p.is_symlink() for p in (output, *output.parents)), "Output symlink forbidden")
    require(not any(p.suffix.lower() == ".app" for p in (output, *output.parents)), "Output may not be any app descendant")
    for source in inputs:
        require(not paths_overlap(output, source), "Output physically/lexically overlaps input or protected app: " + str(source))


def plan(args):
    require(platform.system() == "Darwin", "This native observation requires macOS")
    root = Path(__file__).resolve().parents[1]
    app, template, config, output = (p.absolute() for p in (args.app, args.save_template, args.config_template, args.output))
    require(app.suffix.lower() == ".app" and app.is_dir(), "A verified current .app is required")
    require(template.is_dir() and config.is_file(), "Real save and ordinary config template required")
    for source in (app, template, config):
        reject_symlinks(source)
    boundaries = [app, template, config, *(root / p for p in PROTECTED_RELATIVE)]
    output_boundary(output, boundaries)
    app, template, config, output = app.resolve(strict=True), template.resolve(strict=True), config.resolve(strict=True), output.resolve()
    validate_settings(config)
    config_raw = config.read_bytes()
    require(profile_config(config_raw, "standard", "linear-hdr") == config_raw,
            "Initial ordinary config must already request standard/linear-hdr")
    require(b"renderdistance 3\n" in config_raw and b"directionalshadowquality medium\n" in config_raw
            and b"postprocessingquality off\n" in config_raw and b"windowsize 1280 720\n" in config_raw,
            "Bounded RD3/medium/off/1280x720 ordinary config required")
    disk = metadata(template / "world.meta")
    cells = saved_cells(template)
    distribution, entries = verified_package_entries(app)
    require(entries and len({p.as_posix() for p, _ in entries}) == len(entries), "Duplicate/empty managed inventory")
    identity = json.loads((app / "Contents/Resources/build-identity.json").read_text())
    binary = app / "Contents/Resources/bin/HelloMine3D"
    require(digest(binary) == identity["executable_sha256"], "Executable identity changed")
    binary_bytes = binary.read_bytes()
    require(ENTRY.encode() in binary_bytes
            and b"hellomine3d-reference-settings-restart-observation-v1" in binary_bytes
            and b"blocking-World.getBlock-find-only-nonAir" in binary_bytes,
            "Client lacks current restart observation sink/domain; guard tokens alone are not capability")
    require(identity.get("capabilities", {}).get("hidden_window_no_activate_v1") is True, "Hidden non-activating support required")
    require(identity["source_manifest_sha256"] == digest(app / "Contents/Resources/source-tree-sha256.txt")
            and identity["resource_manifest_sha256"] == digest(app / "Contents/Resources/media/resource-manifest.txt"),
            "Packaged source/resource receipt changed")
    source_certificate = current_source_matches(app)
    with (app / "Contents/Info.plist").open("rb") as stream:
        require(plistlib.load(stream)["CFBundleIdentifier"] == identity["bundle_id"], "Bundle identity changed")
    return {"app": app, "template": template, "config": config, "output": output,
            "config_raw": config_raw, "disk": disk, "cells": cells, "identity": identity,
            "source_certificate": source_certificate, "distribution_sha256": digest(distribution),
            "source_before": tree_files(app), "template_before": tree_files(template),
            "config_before": digest(config), "protected_before": protected_inventory(root)}


def run(args, prepared):
    app, template, config, output = (prepared[k] for k in ("app", "template", "config", "output"))
    output.mkdir(parents=True)
    record_path = output / "run.json"
    record = {"schema": "hellomine3d-reference-settings-restart-run-v1", "evidence_type": "DEVELOPER_DIAGNOSTIC",
              "normal_input": False, "input_actions": 0, "performance_isolation_claimed": False,
              "host_mode": "BUSY_HOST_ENGINEERING" if args.busy_host_engineering else "IDLE_GAME_PREFLIGHT",
              "source_app": str(app), "save_template": str(template), "config_template": str(config),
              "package_identity": prepared["identity"], "current_source_certificate": prepared["source_certificate"],
              "source_file_hashes_before": prepared["source_before"], "template_file_hashes_before": prepared["template_before"],
              "config_template_sha256_before": prepared["config_before"], "protected_file_hashes_before": prepared["protected_before"],
              "distribution_sha256": prepared["distribution_sha256"], "template_disk_facts": prepared["disk"],
              "template_decoded_cells": prepared["cells"], "started_utc": utc(), "stages": [], "launch_count": 0,
              "process_deadline_seconds": args.timeout_seconds, "game_censuses": [], "result": "PREPARING",
              "limits": ["No ordinary settings UI/menu/input/performance acceptance.",
                         "Normal config two-key file change; same private saved-world path copied only once.",
                         "Native facts require independent audit, not profile tokens or equal-time images.",
                         "Periodic process census; only owned Popen handles can be signalled."]}
    write_record(record_path, record)
    child = None
    try:
        census = game_census(output, "before")
        record["game_censuses"].append(census)
        require(args.busy_host_engineering or not census["games"], "Another game exists; refuse before launching")
        runtime, save = output / "Runtime.app", output / "save"
        clone_verified_package(app, runtime)
        runtime_before = tree_files(runtime)
        record["runtime_managed_initial_hashes"] = runtime_before
        shutil.copytree(template, save)
        require(tree_files(save) == prepared["template_before"], "Single initial save clone differs")
        (output / ".hellomine3d-reference-settings-restart-owned").write_text(MAGIC)
        resources = runtime / "Contents/Resources"
        config_path = resources / "bin/config.txt"
        config_path.write_bytes(prepared["config_raw"])
        expected_save, initial_world, window_size = tree_files(save), prepared["disk"]["world"], None
        pids = set()
        for number, stage in enumerate(STAGES, 1):
            stage_path = output / stage[0]
            stage_path.mkdir()
            stage_record = {"stage": stage[0], "requested_visualdetail": stage[1], "requested_renderpipeline": stage[2],
                            "save_directory": str(save), "result": "PREPARING", "input_actions": 0}
            record["stages"].append(stage_record)
            require(tree_files(save) == expected_save, "Save lineage changed between natural processes")
            stage_record["save_file_hashes_before"] = expected_save
            before = metadata(save / "world.meta")
            close_numbers(before["world"], initial_world, "same_disk_world")
            saved_cells(save)
            stage_record["disk_facts_before"] = before
            shutil.copy2(save / "world.meta", stage_path / "world-before.meta")
            expected_config = profile_config(prepared["config_raw"], stage[1], stage[2])
            if number > 1:
                prior = STAGES[number - 2]
                require(config_path.read_bytes() == profile_config(prepared["config_raw"], prior[1], prior[2]),
                        "Config changed outside the previous native stage")
                config_path.write_bytes(expected_config)
            require(config_path.read_bytes() == expected_config, "Unexpected stage config bytes")
            shutil.copy2(config_path, stage_path / "config-before.txt")
            stage_record["config_sha256_before"] = digest(config_path)
            diagnostic = stage_environment(resources, output, stage)
            environment = {key: value for key, value in os.environ.items() if not key.startswith(PREFIXES)}
            environment.update(diagnostic)
            stage_record["environment"] = diagnostic
            stage_record["removed_inherited_diagnostic_environment_names"] = sorted(k for k in os.environ if k.startswith(PREFIXES))
            census = game_census(stage_path, "before-launch")
            record["game_censuses"].append(census)
            require(args.busy_host_engineering or not census["games"], "Another game appeared before stage launch")
            command = [str(resources / "bin/HelloMine3D")]
            stage_record.update({"command": command, "cwd": str(resources / "bin"), "launch_utc": utc()})
            with (stage_path / "client.log").open("w") as stdout, (stage_path / "client-stderr.log").open("w") as stderr:
                child = subprocess.Popen(command, cwd=resources / "bin", env=environment, stdin=subprocess.DEVNULL,
                                         stdout=stdout, stderr=stderr)
                deadline = time.monotonic() + args.timeout_seconds
                require(child.pid not in pids, "PID reused within the three-process lineage")
                pids.add(child.pid)
                stage_record.update({"pid": child.pid, "result": "RUNNING"})
                record["launch_count"] += 1
                write_record(record_path, record)
                stage_record["loaded_executable"] = certify_loaded_executable(child, resources / "bin/HelloMine3D", stage_path)
                require(stage_record["loaded_executable"]["executable_sha256"] == prepared["identity"]["executable_sha256"],
                        "Actual loaded executable differs")
                next_census, census_number = time.monotonic(), 0
                while child.poll() is None:
                    require(time.monotonic() < deadline, "Owned restart process exceeded external deadline")
                    if time.monotonic() >= next_census:
                        census_number += 1
                        census = game_census(stage_path, f"during-{census_number:02d}")
                        record["game_censuses"].append(census)
                        if any(p["pid"] != child.pid for p in census["games"]):
                            record["other_game_observed_during_run"] = True
                        next_census = time.monotonic() + 5
                        write_record(record_path, record)
                    time.sleep(0.2)
                stage_record.update({"returncode": child.returncode, "natural_exit": True, "exit_utc": utc()})
            require(child.returncode == 0, "Native restart stage exit " + str(child.returncode))
            require(config_path.read_bytes() == expected_config, "Normal client unexpectedly changed config")
            shutil.copy2(config_path, stage_path / "config-after.txt")
            stage_record["config_sha256_after"] = digest(config_path)
            facts_path = stage_path / "restart-facts/snapshot.json"
            reject_symlinks(stage_path)
            require(facts_path.is_file() and 0 < facts_path.stat().st_size <= 1048576, "Missing/bounded single native snapshot")
            require(sorted(p.name for p in facts_path.parent.iterdir()) == ["snapshot.json"], "Unexpected extra restart observation artifacts")
            facts = json.loads(facts_path.read_text())
            actual_size = check_snapshot(facts, stage, child.pid, resources, save, before)
            require(window_size is None or actual_size == window_size, "Actual native window size changed between profiles")
            window_size = actual_size
            stage_record["snapshot"] = facts
            stage_record["snapshot_sha256"] = digest(facts_path)
            stage_record["frames"] = []
            for milliseconds in (3500, 7000):
                path = stage_path / "frames" / f"capture_{milliseconds:05d}ms.png"
                require(path.is_file() and png_dimensions(path) == actual_size, "Actual primary capture missing/size differs")
                stage_record["frames"].append({"path": str(path), "sha256": digest(path), "dimensions": actual_size})
            after = metadata(save / "world.meta")
            close_numbers(after["world"], initial_world, "saved_world")
            require(after["world_time"] >= facts["world_time"] and after["world_time"] > before["world_time"],
                    "Normal shutdown did not save advancing World time")
            close_numbers(after["player"]["inventory"], before["player"]["inventory"], "saved_inventory")
            saved_cells(save)
            shutil.copy2(save / "world.meta", stage_path / "world-after.meta")
            expected_save = tree_files(save)
            stage_record.update({"disk_facts_after": after, "save_file_hashes_after": expected_save,
                                 "result": "NATIVE_COMPLETED_PENDING_INDEPENDENT_AUDIT"})
            write_record(record_path, record)
            child = None
        record["result"] = "NATIVE_COMPLETED_PENDING_INDEPENDENT_AUDIT"
    except (Exception, KeyboardInterrupt, SystemExit) as error:
        record["result"] = "FAILED"
        record["error"] = f"{type(error).__name__}: {error}"
        if record["stages"]:
            record["stages"][-1]["result"] = "FAILED"
    finally:
        if child is not None and child.poll() is None:
            record["result"] = "FAILED"
            record["owned_child_cleanup_required"] = True
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
            if record["stages"]:
                record["stages"][-1].update({"returncode": child.returncode, "natural_exit": False})
        for label, path, before in (("source_app", app, prepared["source_before"]),
                                     ("save_template", template, prepared["template_before"])):
            try:
                after = tree_files(path)
                record[label + "_file_hashes_after"] = after
                record[label + "_all_files_unchanged"] = after == before
            except Exception as error:
                record[label + "_all_files_unchanged"] = False
                record[label + "_preservation_error"] = str(error)
        try:
            record["config_template_sha256_after"] = digest(config)
            record["config_template_unchanged"] = record["config_template_sha256_after"] == prepared["config_before"]
        except Exception as error:
            record["config_template_unchanged"] = False
            record["config_template_preservation_error"] = str(error)
        record["protected_file_hashes_after"] = {}
        for path, before in prepared["protected_before"].items():
            try:
                record["protected_file_hashes_after"][path] = tree_files(Path(path))
            except Exception as error:
                record["protected_file_hashes_after"][path] = {"preservation_error": str(error)}
        record["all_nine_published_apps_unchanged"] = record["protected_file_hashes_after"] == prepared["protected_before"]
        runtime_path = output / "Runtime.app"
        if runtime_path.is_dir() and "runtime_managed_initial_hashes" in record:
            try:
                current = tree_files(runtime_path)
                expected = dict(record["runtime_managed_initial_hashes"])
                expected.pop("Contents/Resources/bin/config.txt", None)
                record["runtime_managed_final_hashes"] = {path: current.get(path) for path in expected}
                record["runtime_managed_unchanged"] = record["runtime_managed_final_hashes"] == expected
                # Normal crash/log bookkeeping is mutable; it is recorded,
                # never inserted into or mistaken for the managed inventory.
                record["runtime_added_mutable_files"] = {path: value for path, value in current.items()
                                                        if path not in expected and path != "Contents/Resources/bin/config.txt"}
            except Exception as error:
                record["runtime_managed_unchanged"] = False
                record["runtime_preservation_error"] = str(error)
        if not all(record.get(key, False) for key in ("source_app_all_files_unchanged", "save_template_all_files_unchanged",
                                                       "config_template_unchanged", "all_nine_published_apps_unchanged")):
            record["result"] = "FAILED"
            record["preservation_error"] = "Source/input/published preservation failed"
        if record.get("runtime_managed_unchanged") is False:
            record["result"] = "FAILED"
            record["runtime_preservation_error"] = "Runtime managed files changed"
        record["finished_utc"] = utc()
        write_record(record_path, record)
    print(json.dumps({"result": record["result"], "record": str(record_path), "launch_count": record["launch_count"],
                      "input_actions": 0, "ordinary_acceptance": "NOT_RUN"}, ensure_ascii=False))
    return 0 if record["result"] == "NATIVE_COMPLETED_PENDING_INDEPENDENT_AUDIT" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--save-template", type=Path, required=True)
    parser.add_argument("--config-template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="Entirely new owned session; never reused")
    parser.add_argument("--timeout-seconds", type=int, choices=range(55, 121), default=90)
    parser.add_argument("--busy-host-engineering", action="store_true")
    parser.add_argument("--plan-only", action="store_true", help="Verify current inputs and boundaries without mkdir, copy or launch")
    args = parser.parse_args()
    try:
        prepared = plan(args)
        if args.plan_only:
            print(json.dumps({"result": "VALIDATED_PLAN_ONLY", "launch_count": 0, "input_actions": 0,
                              "app": str(prepared["app"]), "output": str(prepared["output"]),
                              "source_certificate": prepared["source_certificate"], "package_identity": prepared["identity"],
                              "protected_app_count": len(prepared["protected_before"]), "world": prepared["disk"]["world"]},
                             ensure_ascii=False))
            return 0
        return run(args, prepared)
    except (Exception, KeyboardInterrupt) as error:
        print(json.dumps({"result": "REJECTED_BEFORE_LAUNCH", "error": f"{type(error).__name__}: {error}",
                          "launch_count": 0, "input_actions": 0}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
