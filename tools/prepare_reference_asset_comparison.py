#!/usr/bin/env python3
"""Prepare immutable old/new visual fixtures and four hidden capture commands.

This tool never launches a game. The exact old channels come from commit 552;
current world geometry and runtime code remain. New kit faces use explicit old
material surrogates, so this is not a historical scene reconstruction.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
OLD_COMMIT = "552345675265f77f8a30dbf1ae205a52d1abb64f"
OLD_CHANNELS = ("media/textures/DefaultPack.png", "media/textures/WarmWilderness64.hmt",
                "media/materials/Base.terrain-atlas")
SURFACE = ("media/materials/Reference.surface-material", "media/textures/ReferenceColour64.hmt",
           "media/textures/ReferenceNormal64.hmt", "media/textures/ReferenceSurface64.hmt")
# Coordinates are read/checked against exact old block definitions and atlas.
SURROGATES = (
    ("pale_stone", (0, 9), "stone", "Stone", (3, 0)),
    ("rough_stone", (1, 9), "cobblestone", "Cobblestone", (7, 1)),
    ("terracotta", (2, 9), "clay", "Clay", (8, 8)),
    ("timber", (3, 9), "oak_planks", "OakPlank", (5, 1)),
    ("metal", (4, 9), "iron_ore", "IronOre", (14, 0)),
    ("lamp_glow", (5, 9), "torch", "Torch", (6, 1)),
)
TEXTURE_KEYS = {"TexAll", "TexTop", "TexSide", "TexBottom"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    return sha(path.read_bytes())


def need(ok, message):
    if not ok:
        raise ValueError(message)


def logical(value):
    p = Path(value)
    need(value and not p.is_absolute() and ".." not in p.parts and "\\" not in value,
         "Unsafe relative path: " + value)
    return p


def manifest(text):
    lines = text.splitlines()
    need(lines and lines[0] == "# HelloMine3D resource manifest v1", "Invalid resource manifest header")
    entries = [s for s in lines if s and not s.startswith("#")]
    need(entries == sorted(set(entries)), "Manifest must be unique and sorted")
    result = {}
    for s in entries:
        category, sep, path = s.partition("|")
        need(sep and re.fullmatch(r"[a-z][a-z-]*", category) and "|" not in path,
             "Invalid manifest entry: " + s)
        logical(path)
        need(path not in result, "Duplicate manifest path: " + path)
        result[path] = category
    return result


def block_fields(text):
    values = [s.strip() for s in text.splitlines() if s.strip()]
    need(len(values) % 2 == 0, "Malformed block field/value pairs")
    result = dict(zip(values[0::2], values[1::2]))
    need(len(result) * 2 == len(values), "Duplicate block fields")
    return result


def patch_block(text, mapping):
    lines = text.splitlines(keepends=True)
    changes = []
    key = None
    for i, line in enumerate(lines):
        value = line.strip()
        if not value:
            continue
        if key is None:
            key = value
            continue
        if key in TEXTURE_KEYS:
            fields = value.split()
            need(len(fields) == 2 and all(s.isdigit() for s in fields), "Invalid texture coordinate")
            tile = tuple(map(int, fields))
            if tile[1] == 9:
                need(tile in mapping, "Unknown row9 face semantic: " + value)
                replacement = mapping[tile]
                ending = "\r\n" if line.endswith("\r\n") else "\n" if line.endswith("\n") else ""
                lines[i] = f"{replacement['tile'][0]} {replacement['tile'][1]}" + ending
                changes.append({"field": key, "from_tile": list(tile), **replacement})
        key = None
    need(key is None, "Block field missing value")
    result = "".join(lines)
    before, after = block_fields(text), block_fields(result)
    need({k: v for k, v in before.items() if k not in TEXTURE_KEYS} ==
         {k: v for k, v in after.items() if k not in TEXTURE_KEYS}, "Nonvisual block field changed")
    return result, changes


def shape_bindings(text, fields):
    values = [s.strip() for s in text.splitlines() if s.strip()]
    need(len(values) % 2 == 0, "Malformed shape")
    records = []
    for key, value in zip(values[0::2], values[1::2]):
        if key != "Box":
            continue
        parts = value.split()
        need(len(parts) == 14, "Unexpected v2 box schema")
        slots = list(map(int, parts[6:12]))
        need(all(s in (0, 1, 2) for s in slots), "Unexpected v2 face slot")
        faces = []
        for side, slot in zip(("front", "back", "left", "right", "top", "bottom"), slots):
            field = ("TexTop", "TexSide", "TexBottom")[slot]
            tile = tuple(map(int, fields.get(field, fields.get("TexAll", "")).split()))
            need(len(tile) == 2 and tile[1] != 9, "Unmapped old shape face")
            faces.append({"face": side, "selector": slot, "block_field": field,
                          "tile": list(tile), "layer": tile[1] * 16 + tile[0]})
        records.append({"box": len(records), "coordinates": parts[:6], "uv_parameters": parts[12:], "faces": faces})
    return records


def verified_app(app):
    inventory = app / "Contents/Resources/distribution-sha256.txt"
    lines = inventory.read_text().splitlines()
    need(lines and lines == sorted(lines, key=lambda s: s.split("  ", 1)[1]), "Unsorted distribution inventory")
    entries = {}
    for line in lines:
        expected, sep, path = line.partition("  ")
        need(sep and re.fullmatch(r"[0-9a-f]{64}", expected), "Malformed distribution identity")
        logical(path)
        need(path not in entries, "Duplicate distribution identity")
        target = app / path
        need(not target.is_symlink() and target.is_file() and digest(target) == expected,
             "Package input changed: " + path)
        entries[path] = expected
    resources = app / "Contents/Resources"
    identity = json.loads((resources / "build-identity.json").read_text())
    need(not identity.get("signed_or_notarized"), "Use an unsigned workbench source package")
    need(digest(resources / "bin/HelloMine3D") == identity["executable_sha256"], "Executable identity mismatch")
    need(digest(resources / "media/resource-manifest.txt") == identity["resource_manifest_sha256"], "Manifest identity mismatch")
    need(digest(resources / "source-tree-sha256.txt") == identity["source_manifest_sha256"], "Source identity mismatch")
    for path in manifest((resources / "media/resource-manifest.txt").read_text()):
        need("Contents/Resources/" + path in entries, "Resource absent from distribution: " + path)
    return entries, identity


def old_inputs():
    def blob(path):
        return subprocess.check_output(["git", "show", OLD_COMMIT + ":" + path], cwd=ROOT)
    channels = {p: blob(p) for p in OLD_CHANNELS}
    records = {p: {"commit": OLD_COMMIT, "sha256": sha(data), "bytes": len(data),
                  "git_blob": subprocess.check_output(["git", "rev-parse", OLD_COMMIT + ":" + p], cwd=ROOT, text=True).strip()}
               for p, data in channels.items()}
    atlas = {}
    for line in channels[OLD_CHANNELS[2]].decode().splitlines():
        if line and not line.startswith("#"):
            f = line.split("|"); atlas[f[0]] = (int(f[1]), int(f[2]))
    mapping = {}
    for semantic, new_tile, old_semantic, old_block, expected in SURROGATES:
        path = "media/blocks/" + old_block + ".block"
        data = blob(path); fields = block_fields(data.decode())
        tile = tuple(map(int, fields["TexAll"].split()))
        need(tile == expected == atlas[old_semantic], "Old block/atlas semantic mismatch: " + semantic)
        mapping[new_tile] = {"semantic": semantic, "old_semantic": old_semantic,
                             "tile": list(tile), "layer": tile[1] * 16 + tile[0],
                             "old_block_path": path, "old_block_sha256": sha(data), "old_block_id": fields["Id"]}
    return channels, records, mapping


def source_snapshot(directory):
    need(directory.is_dir() and (directory / "world.meta").is_file(), "Save template must contain world.meta")
    result = {}
    for path in sorted(directory.rglob("*")):
        need(not path.is_symlink(), "Save symlink is not accepted")
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = digest(path)
    return result


def finite_pose(text):
    parts = text.split()
    need(len(parts) == 3 and all(math.isfinite(float(s)) for s in parts), "Pose requires three finite values")
    return text


def prepare(args):
    source = args.app.resolve(strict=True); save = args.save_template.resolve(strict=True)
    output = args.output.resolve()
    allowed = ROOT / "build/reference-visual-implementation"
    need(output != allowed and output.is_relative_to(allowed), "Output must be a new directory below this worktree's build/reference-visual-implementation")
    need(not output.exists() and not output.is_relative_to(source) and not source.is_relative_to(output), "Output must be new and separate from source app")
    need(not output.is_relative_to(save) and not save.is_relative_to(output), "Output/save must be separate")
    entries, identity = verified_app(source)
    source_inventory = digest(source / "Contents/Resources/distribution-sha256.txt")
    save_hashes = source_snapshot(save)
    channels, old_records, mapping = old_inputs()
    original_manifest = manifest((source / "Contents/Resources/media/resource-manifest.txt").read_text())
    need(all(p in original_manifest for p in SURFACE), "Current source must declare the complete four-item reference profile")
    output.mkdir(parents=True)
    shutil.copytree(save, output / "world-template")
    need(source_snapshot(output / "world-template") == save_hashes, "World snapshot copy changed")
    fixtures = {}
    for mode in ("old", "new"):
        app = output / (mode + "-assets.app")
        for relative in entries:
            dest = app / relative; dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / relative, dest)
        resources = app / "Contents/Resources"
        changed, removed, block_maps, shapes = {}, [], [], []
        if mode == "old":
            for path, data in channels.items():
                (resources / path).write_bytes(data); changed[path] = {"before": entries["Contents/Resources/" + path], "after": sha(data)}
            for path in SURFACE:
                (resources / path).unlink(); removed.append(path)
            remaining = [category + "|" + path for path, category in original_manifest.items() if path not in SURFACE]
            (resources / "media/resource-manifest.txt").write_text("# HelloMine3D resource manifest v1\n" + "\n".join(sorted(remaining)) + "\n")
            changed["media/resource-manifest.txt"] = {"before": identity["resource_manifest_sha256"], "after": digest(resources / "media/resource-manifest.txt")}
            for path, category in original_manifest.items():
                if category != "block":
                    continue
                file = resources / path; before = file.read_text(); after, records = patch_block(before, mapping)
                if not records:
                    continue
                file.write_text(after)
                changed[path] = {"before": entries["Contents/Resources/" + path], "after": digest(file)}
                fields = block_fields(after)
                block_maps.append({"block": path, "id": fields["Id"], "textures": records,
                                   "nonvisual_fields_preserved": True})
                if "Shape" in fields:
                    shape_path = "media/shapes/" + fields["Shape"] + ".shape"
                    shape_file = resources / shape_path
                    need(digest(shape_file) == entries["Contents/Resources/" + shape_path], "Shape bytes changed")
                    shapes.append({"shape": shape_path, "block": path, "sha256": digest(shape_file),
                                   "geometry_uv_selectors_identical": True, "yaw_metadata_unchanged": True,
                                   "box_material_bindings": shape_bindings(shape_file.read_text(), fields)})
            need(block_maps and shapes, "Source lacks the current reference architectural kit")
            need(not any(p in manifest((resources / "media/resource-manifest.txt").read_text()) or (resources / p).exists() for p in SURFACE), "Profile/channel removal must be complete")
        for path, category in manifest((resources / "media/resource-manifest.txt").read_text()).items():
            if category == "block" and mode == "old":
                need(not any(tuple(map(int, v.split()))[1] == 9 for k, v in block_fields((resources / path).read_text()).items() if k in TEXTURE_KEYS), "Old clone has unmapped row9")
        unchanged_shapes = {p: digest(resources / p) for p, c in original_manifest.items() if c == "shape"}
        need(all(value == entries["Contents/Resources/" + p] for p, value in unchanged_shapes.items()), "Geometry changed")
        need(digest(resources / "media/recipes/Base.recipe") == entries["Contents/Resources/media/recipes/Base.recipe"], "Recipe changed")
        receipt = {"fixture_version": 1, "mode": mode, "source_app": str(source),
                   "source_executable_sha256": identity["executable_sha256"], "source_distribution_sha256": source_inventory,
                   "normal_input": 0, "game_launched": False, "runtime_compatibility": "NOT_RUN",
                   "old_commit": OLD_COMMIT, "old_channel_blobs": old_records if mode == "old" else {},
                   "surrogate_policy": "Current geometry with explicit old material surrogates; not an old historical scene",
                   "semantic_mapping": list(mapping.values()) if mode == "old" else [], "block_mappings": block_maps,
                   "shape_bindings": shapes, "shape_sha256_unchanged": unchanged_shapes,
                   "profile_complete_absence": mode == "old", "removed_paths": removed,
                   "old_channel_has_normal_RME": False if mode == "old" else None,
                   "changed_resources": changed, "world_template_sha256": save_hashes,
                   "preserved": ["executable", "world geometry/IDs/metadata", "all shape bytes/UV/selectors", "block nonvisual fields/collision", "recipe"],
                   "required_runtime_policy": "Complete profile plus three channels absent is legal; any partial declaration/bad assets remain errors"}
        receipt_path = resources / "asset-comparison-receipt.json"
        receipt_path.write_text(json.dumps(receipt, indent=2) + "\n")
        clone_identity = dict(identity)
        bundle = "local.hellomine3d.asset-comparison-" + mode
        clone_identity.update(bundle_id=bundle, resource_manifest_sha256=digest(resources / "media/resource-manifest.txt"), acceptance="NOT_RUN")
        clone_identity["asset_comparison"] = {"mode": mode, "normal_input": 0, "receipt": receipt_path.name,
            "receipt_sha256": digest(receipt_path), "source_build_identity_sha256": digest(source / "Contents/Resources/build-identity.json")}
        (resources / "build-identity.json").write_text(json.dumps(clone_identity, indent=2) + "\n")
        plist_path = app / "Contents/Info.plist"
        info = plistlib.loads(plist_path.read_bytes()); info.update(CFBundleIdentifier=bundle, CFBundleName=app.stem)
        plist_path.write_bytes(plistlib.dumps(info))
        managed = set(entries) - {"Contents/Resources/" + p for p in removed}
        managed.add("Contents/Resources/asset-comparison-receipt.json")
        inventory = resources / "distribution-sha256.txt"
        inventory.write_text("\n".join(digest(app / p) + "  " + p for p in sorted(managed)) + "\n")
        verified_app(app)
        fixtures[mode] = {"app": str(app), "receipt_sha256": digest(receipt_path), "distribution_sha256": digest(inventory)}
    need(verified_app(source)[0] == entries and digest(source / "Contents/Resources/distribution-sha256.txt") == source_inventory,
         "Original app changed during fixture preparation")
    need(source_snapshot(save) == save_hashes, "Original save changed during fixture preparation")
    cases = []
    for mode in ("old", "new"):
        for pipeline in ("legacy", "linear-hdr"):
            name = mode + "-" + pipeline
            command = [sys.executable, str(ROOT / "tools/capture_visual_macos.py"), "--app", fixtures[mode]["app"],
                       "--output", str(output / "captures" / name), "--save-template", str(output / "world-template"),
                       "--scene", "shore", "--position", args.position, "--rotation", args.rotation,
                       "--time", str(args.time), "--seed", str(args.seed), "--render-distance", str(args.render_distance),
                       "--shadow", args.shadow, "--post", "off", "--render-pipeline", pipeline,
                       "--visual-detail", "standard", "--perspective", "first", "--minimap-range", "128",
                       "--width", str(args.width), "--height", str(args.height), "--pixel-ratio", str(args.pixel_ratio),
                       "--capture-ms", args.capture_ms, "--launch-method", "direct"]
            if args.msaa4 and pipeline == "linear-hdr":
                command.append("--msaa4")
            cases.append({"case": name, "assets": mode, "pipeline": pipeline, "normal_input": 0,
                          "expected_profile_available": mode == "new", "command": command})
    summary = {"status": "PREPARED_NOT_LAUNCHED", "original_app_unchanged": True, "original_save_unchanged": True,
               "fixtures": fixtures, "cases": cases, "interpretation": [
                   "All four use current runtime and identical saved world/shape geometry.",
                   "Old assets use exact commit552 visual channels and explicit old material surrogates for new kit parts.",
                   "The whole old channel lacks normal and roughness/metalness/emission maps.",
                   "This 2x2 measures assets and complete renderer routes; it does not isolate each renderer feature.",
                   "Requested HDR must log active=linear-hdr fallback=0; fallback=1 is a fallback result, never HDR success.",
                   "Complete old profile absence is valid only after the production compatibility fix; missing/partial declared assets must fail.",
                   "Current retained surface shader variants still compile before HDR capability fallback; no shader bypass.",
                   "Hidden render captures never prove ordinary input or independent gameplay acceptance."]}
    (output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
    (output / "run_capture_matrix.py").write_text('''#!/usr/bin/env python3
import argparse,json,os,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description="Explicitly launch a prepared hidden asset comparison; normal_input=0")
p.add_argument("--case",required=True,choices=["all","old-legacy","old-linear-hdr","new-legacy","new-linear-hdr"])
a=p.parse_args()
env={k:v for k,v in os.environ.items() if not k.startswith(("HELLOMINE3D_","HELLO_RENDER_","HELLO_PERF_"))}
results=[]
for case in json.loads((root/"comparison.json").read_text())["cases"]:
    if a.case!="all" and a.case!=case["case"]: continue
    record={"case":case["case"],"normal_input":0,"command":case["command"],"started_unix":time.time()}
    record["exit"]=subprocess.run(case["command"],env=env).returncode
    record["finished_unix"]=time.time();results.append(record)
    (root/("launch-"+case["case"]+".json")).write_text(json.dumps(record,indent=2)+"\\n")
    if record["exit"]: raise SystemExit(record["exit"])
''')
    print("[ASSET_COMPARISON] PREPARED_NOT_LAUNCHED", output)
    return 0


class Calibration(unittest.TestCase):
    def test_manifest(self):
        self.assertEqual(manifest("# HelloMine3D resource manifest v1\nblock|media/a.block\n"), {"media/a.block": "block"})
        for text in ("bad", "# HelloMine3D resource manifest v1\nblock|../a\n", "# HelloMine3D resource manifest v1\nblock|media/a\nblock|media/a\n", "# HelloMine3D resource manifest v1\ntexture|media/a\nblock|media/b\n"):
            with self.assertRaises(ValueError): manifest(text)
    def test_mapping(self):
        mapping = {(0, 9): {"tile": [3, 0], "layer": 3}}
        before = "Name\nStone\n\nId\n35\n\nTexAll\n0 9\n\nCollidable\n1\n"
        after, records = patch_block(before, mapping)
        self.assertEqual(block_fields(after)["TexAll"], "3 0")
        self.assertEqual(block_fields(after)["Id"], "35")
        self.assertEqual(len(records), 1)
        with self.assertRaises(ValueError): patch_block(before.replace("0 9", "7 9"), mapping)
    def test_shape(self):
        s = "Version\n2\n\nBox\n0 0 0 1 1 1 0 1 2 0 1 2 0 2\n"
        b = shape_bindings(s, {"TexTop": "3 0", "TexSide": "7 1", "TexBottom": "8 8"})[0]
        self.assertEqual([q["layer"] for q in b["faces"]], [3, 23, 136, 3, 23, 136])
        with self.assertRaises(ValueError): shape_bindings(s, {"TexAll": "0 9"})
        with self.assertRaises(ValueError): shape_bindings(s.replace("0 1 2 0 1 2", "0 1 9 0 1 2"), {"TexAll": "3 0"})


def main():
    if sys.argv[1:] == ["--self-test"]:
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Calibration))
        return not result.wasSuccessful()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--save-template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--position", type=finite_pose, required=True)
    parser.add_argument("--rotation", type=finite_pose, required=True)
    parser.add_argument("--time", type=int, default=6000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--render-distance", type=int, choices=range(1, 33), default=3)
    parser.add_argument("--shadow", choices=("off", "medium", "high"), default="medium")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--pixel-ratio", type=int, choices=(1, 2), default=2)
    parser.add_argument("--capture-ms", default="4000,7000")
    parser.add_argument("--msaa4", action="store_true")
    return prepare(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
