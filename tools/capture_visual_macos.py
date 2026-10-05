#!/usr/bin/env python3
"""Repeatable visual/performance diagnostics in a macOS package.

This defaults to a hidden, non-activating client; --foreground explicitly opts
into a visible window. This uses forced viewpoints and optionally a HUD fixture. It is developer
diagnostic evidence, never normal-input or independent gameplay acceptance.
By default the supplied package is copied before use. --reuse-app runs the
same supplied package for every capture and updates its diagnostic config;
freeze and hash that package only after the final capture.
Performance mode disables render readback; run visual captures separately.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import resource
import shutil
import subprocess
import time


SCENES = {
    "forest": ("1024 140 1024", "0 0 0"),
    "ridge": ("-480 121 -480", "0 0 0"),
    "coast": ("-256 70 -256", "0 0 0"),
    # WV2-00 candidates selected from the production v5 height/biome survey;
    # their capture records and raw frames establish the actual scene.
    "dense": ("928 110 928", "0 0 0"),
    "shore": ("0 82 512", "0 0 0"),
    "grassland": ("1056 106 928", "0 0 0"),
    "menu": None,
}

# Frozen V06b viewpoints; shore-edit targets must come from a current-world
# locator in the local 32x32 region, never from the historical terrain output.
SHORE_EDIT_SITES = {
    "river": ("212 70 -192", "10 90 0"),
    "lake": ("196 72 -144", "10 90 0"),
    "sea": ("-89 67 602", "10 -75.6186 0"),
    "wetland": ("-101 67 312", "10 -126.027 0"),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def performance_framebuffer(client_log, width, height, pixel_ratio):
    """Check actual Cocoa backing dimensions without a timed GPU readback."""
    sizes = re.findall(r"Cocoa: Window created (\d+) x (\d+) with backing store size (\d+) x (\d+)", client_log)
    expected = (width, height, width * pixel_ratio, height * pixel_ratio)
    if len(sizes) != 1 or tuple(map(int, sizes[0])) != expected:
        raise RuntimeError(f"Expected one Cocoa window with dimensions {expected}; got {sizes}")
    if "[OgreRenderCapture] enabled" in client_log or "[OgreRenderCapture] captured" in client_log:
        raise RuntimeError("Render readback was active during the performance run")
    return list(expected[2:])


def verified_package_entries(source):
    inventory = source / "Contents/Resources/distribution-sha256.txt"
    entries = []
    for entry in inventory.read_text().splitlines():
        expected, relative = entry.split("  ", 1)
        path = Path(relative)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError(f"Unsafe inventory path: {relative}")
        original = source / path
        if digest(original) != expected:
            raise ValueError(f"Package input changed: {relative}")
        entries.append((path, original))
    return inventory, entries


def clone_verified_package(source, destination):
    inventory, entries = verified_package_entries(source)
    for path, original in entries:
        target = destination / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    shutil.copy2(inventory, destination / inventory.relative_to(source))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--save-template", type=Path,
                        help="Copy an existing diagnostic world into this run's new save directory")
    parser.add_argument("--scene", choices=SCENES, default="forest")
    parser.add_argument("--position", help="Override diagnostic world spawn as 'x y z'")
    parser.add_argument("--rotation", help="Override diagnostic camera rotation as 'x y z'")
    parser.add_argument("--time", type=int, default=6000)
    parser.add_argument("--seed", type=int, default=20260807)
    parser.add_argument("--fov", type=int, default=90)
    parser.add_argument("--render-distance", type=int, choices=range(1, 33), default=8)
    parser.add_argument("--perspective", choices=("first", "third"),
                        help="Use the normal saved camera setting in a diagnostic capture")
    parser.add_argument("--player-motion", choices=("forward", "backward", "left", "right"),
                        help="Hidden render-only avatar motion facts; does not move the player or exercise input")
    parser.add_argument("--shadow", choices=("off", "medium", "high"), default="off")
    parser.add_argument("--post", choices=("off", "on"), default="off")
    parser.add_argument("--locale", choices=("en-US", "zh-CN"), default="zh-CN")
    parser.add_argument("--ui-scale", type=float, choices=(0.85, 1.0, 1.25), default=1.0)
    parser.add_argument("--feedback", choices=("off", "reduced", "full"), default="full")
    parser.add_argument("--minimap-range", type=int, choices=(64, 128, 256))
    parser.add_argument("--actor-visual", choices=("idle", "windup", "recover", "walk", "cycle", "projectiles", "projectile-flight", "wildlife-cycle"))
    parser.add_argument("--actor-distance", type=int, choices=(6, 12, 24),
                        help="Diagnostic gallery distance; requires --actor-visual")
    parser.add_argument("--hud-fixture", action="store_true")
    parser.add_argument("--material-identity", action="store_true",
                        help="Capture nine actual material-consumer phases in a new isolated world (diagnostic input only)")
    parser.add_argument("--camera-diagnostics", action="store_true",
                        help="Capture six normal render-camera/scene phases in a new isolated world (diagnostic input only)")
    parser.add_argument("--pause-notifications", action="store_true",
                        help="Observe twelve actual pause-notification backend frames in a fresh compact hidden client (diagnostic input only)")
    parser.add_argument("--fern-wind", action="store_true",
                        help="Observe four actual draws of two natural Fern sources in a new isolated forest (diagnostic input only)")
    parser.add_argument("--shore-edit", action="store_true",
                        help="Observe six production shore-edit/world-map phases in a fresh hidden isolated world (diagnostic input only)")
    parser.add_argument("--shore-edit-target", metavar="x y z",
                        help="Canonical4 Water64 target from the current-world bounded locator; requires --shore-edit")
    parser.add_argument("--shore-edit-site", choices=SHORE_EDIT_SITES,
                        help="Frozen shore viewpoint (default: river); requires --shore-edit")
    parser.add_argument("--inspect-slot", type=int, choices=range(5), help="Item detail diagnostic; requires pointer panel and HUD fixture")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--panel", choices=("crafting", "container", "furnace", "crusher", "settings", "map", "journal", "pointer"))
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--pixel-ratio", type=int, choices=(1, 2), default=1,
                        help="Expected framebuffer pixels per window point; use 2 on Retina")
    parser.add_argument("--atmosphere-fallback", action="store_true")
    parser.add_argument("--terrain-fallback", action="store_true")
    parser.add_argument("--visual-detail", choices=("standard", "compatibility"))
    parser.add_argument("--performance", action="store_true")
    parser.add_argument("--capture-ms", default="5000,10000",
                        help="Up to eight increasing render capture times in milliseconds (1..60000)")
    parser.add_argument("--streaming", action="store_true")
    parser.add_argument("--launch-method", choices=("open", "direct"), default="open")
    parser.add_argument("--foreground", action="store_true",
                        help="Explicitly show/activate the client; default is hidden background capture")
    parser.add_argument("--reuse-app", action="store_true",
                        help="Run the supplied stable app in place; its diagnostic config is updated")
    args = parser.parse_args()
    try:
        capture_times = [int(value) for value in args.capture_ms.split(',')]
        if (not 1 <= len(capture_times) <= 8 or
                capture_times != sorted(set(capture_times)) or
                not all(1 <= value <= 60000 for value in capture_times)):
            raise ValueError
    except ValueError:
        parser.error("--capture-ms requires up to eight increasing integers in 1..60000")
    if args.inspect_slot is not None and (args.panel != "pointer" or not args.hud_fixture):
        parser.error("--inspect-slot requires --panel pointer --hud-fixture")
    if args.performance and args.capture_ms != "5000,10000":
        parser.error("--capture-ms applies only to render capture")
    if args.actor_distance is not None and not args.actor_visual:
        parser.error("--actor-distance requires --actor-visual")
    if (args.shore_edit_target is not None or args.shore_edit_site is not None) and not args.shore_edit:
        parser.error("--shore-edit-target and --shore-edit-site require --shore-edit")
    shore_edit_site = args.shore_edit_site or "river"
    shore_edit_target = None
    if args.shore_edit:
        try:
            parts = args.shore_edit_target.split() if args.shore_edit_target else []
            if len(parts) != 3:
                raise ValueError
            x, y, z = map(int, parts)
            centre_x, _, centre_z = map(int, SHORE_EDIT_SITES[shore_edit_site][0].split())
            if (y != 64 or x % 4 != 0 or z % 4 != 0 or
                    not centre_x - 16 <= x < centre_x + 16 or
                    not centre_z - 16 <= z < centre_z + 16):
                raise ValueError
            shore_edit_target = f"{x} {y} {z}"
        except ValueError:
            parser.error("--shore-edit requires --shore-edit-target 'x 64 z' with integer canonical4 x/z inside the frozen site's 32x32 locator region")
    if args.player_motion and (args.perspective != "third" or args.foreground or args.performance or args.scene == "menu"):
        parser.error("--player-motion requires hidden third-person world render capture")
    if "HELLOMINE3D_PLAYER_MOTION_CAPTURE" in os.environ:
        parser.error("Inherited player motion fixture is not accepted; use --player-motion explicitly")
    if "HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR" in os.environ:
        parser.error("Inherited material identity fixture is not accepted; use --material-identity explicitly")
    if "HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR" in os.environ:
        parser.error("Inherited camera diagnostics are not accepted; use --camera-diagnostics explicitly")
    if "HELLOMINE3D_FERN_WIND_CAPTURE_DIR" in os.environ:
        parser.error("Inherited Fern diagnostics are not accepted; use --fern-wind explicitly")
    if "HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR" in os.environ:
        parser.error("Inherited pause notifications are not accepted; use --pause-notifications explicitly")
    if any(name in os.environ for name in ("HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR",
            "HELLOMINE3D_SHORE_EDIT_TARGET", "HELLOMINE3D_SHORE_EDIT_SITE")):
        parser.error("Inherited shore edit diagnostics are not accepted; use --shore-edit explicitly")
    if args.pause_notifications and (args.foreground or args.performance or args.player_motion or
            args.actor_visual or args.hud_fixture or args.panel or args.inspect_slot is not None or
            args.material_identity or args.camera_diagnostics or args.fern_wind or args.shore_edit or args.scene == "menu" or
            args.reuse_app or args.save_template or args.streaming or args.position or args.rotation or
            args.render_distance != 1 or args.visual_detail != "standard" or
            args.launch_method != "direct" or args.capture_ms != "5000,10000" or
            args.terrain_fallback or args.atmosphere_fallback or os.environ.get("HELLOMINE3D_RESOURCE_PACKS") or
            args.perspective != "first" or args.shadow != "off" or args.post != "off" or
            args.width != 640 or args.height != 480 or args.ui_scale != 1.25):
        parser.error("--pause-notifications requires a fresh hidden default-pack standard RD1 world, direct launch, 640x480 points/scale1.25, first perspective, and no other diagnostics")
    if args.fern_wind and (args.foreground or args.performance or args.player_motion or
            args.actor_visual or args.hud_fixture or args.panel or args.inspect_slot is not None or
            args.material_identity or args.camera_diagnostics or args.shore_edit or args.scene != "forest" or
            args.reuse_app or args.save_template or args.streaming or args.position or args.rotation or
            args.render_distance != 1 or args.visual_detail not in ("standard", "compatibility") or
            args.launch_method != "direct" or args.capture_ms != "5000,10000" or
            args.terrain_fallback or args.atmosphere_fallback or os.environ.get("HELLOMINE3D_RESOURCE_PACKS") or
            args.seed != 20260807 or args.time != 6000 or args.perspective != "first" or
            args.shadow != "off" or args.post != "off" or args.fov != 90 or
            args.width != 1280 or args.height != 720):
        parser.error("--fern-wind requires a new hidden default-pack forest, seed20260807/time6000, RD1, explicit standard/compatibility, direct launch, first perspective, FOV90 and 1280x720 points")
    if args.camera_diagnostics and (args.foreground or args.performance or args.player_motion or
            args.actor_visual or args.hud_fixture or args.panel or args.material_identity or
            args.fern_wind or args.shore_edit or
            args.scene == "menu" or args.reuse_app or args.save_template or args.streaming or
            args.position or args.rotation or
            args.render_distance != 1 or args.visual_detail not in ("standard", "compatibility") or
            args.launch_method != "direct" or args.capture_ms != "5000,10000" or
            args.terrain_fallback or args.atmosphere_fallback or os.environ.get("HELLOMINE3D_RESOURCE_PACKS") or
            args.fov != 120 or args.width != 1920 or args.height != 640):
        parser.error("--camera-diagnostics requires a new hidden default-pack RD1 world, explicit standard/compatibility, direct launch, FOV120 and 1920x640 points")
    if args.material_identity and (args.foreground or args.performance or args.player_motion or
            args.actor_visual or args.hud_fixture or args.panel or args.inspect_slot is not None or
            args.fern_wind or args.shore_edit or
            args.scene == "menu" or args.reuse_app or args.save_template or
            args.render_distance != 1 or args.visual_detail not in ("standard", "compatibility") or
            args.launch_method != "direct" or args.capture_ms != "5000,10000" or
            args.terrain_fallback or args.atmosphere_fallback or os.environ.get("HELLOMINE3D_RESOURCE_PACKS")):
        parser.error("--material-identity requires a new hidden default-pack world, RD1, explicit standard/compatibility, direct launch, and no other fixtures")
    if args.shore_edit and (args.foreground or args.performance or args.player_motion or
            args.actor_visual or args.actor_distance is not None or args.hud_fixture or args.panel or
            args.inspect_slot is not None or args.debug or args.material_identity or
            args.camera_diagnostics or args.fern_wind or args.pause_notifications or args.scene == "menu" or
            args.reuse_app or args.save_template or args.streaming or args.position or args.rotation or
            args.render_distance != 1 or args.visual_detail not in ("standard", "compatibility") or
            args.launch_method != "direct" or args.capture_ms != "5000,10000" or
            args.terrain_fallback or args.atmosphere_fallback or
            args.seed != 42 or args.time != 7000 or args.perspective != "first" or
            args.shadow != "off" or args.post != "off" or args.fov != 90 or args.minimap_range != 256 or
            args.width != 1280 or args.height != 720):
        parser.error("--shore-edit requires a fresh hidden default-pack world, seed42/time7000, RD1, explicit standard/compatibility, direct launch, first perspective, FOV90/minimap256, 1280x720 points and no other diagnostics")
    if args.shore_edit and any(name in os.environ for name in (
            "HELLOMINE3D_RESOURCE_PACKS", "HELLOMINE3D_ACTOR_VISUAL_DISTANCE",
            "HELLOMINE3D_E2_BATCH_EVENTS", "HELLOMINE3D_E2_RENDER_PHASES",
            "HELLOMINE3D_EXIT_AFTER_FRAMES", "HELLOMINE3D_SKIP_MAIN_MENU",
            "HELLOMINE3D_P11_LIGHT_FIXTURE", "HELLOMINE3D_DISABLE_VERTEX_AO",
            "HELLOMINE3D_PAUSE_NOTIFICATION_CAPTURE_DIR", "HELLO_PERF_CAPTURE")):
        parser.error("Inherited diagnostic fixtures cannot be combined with shore edit capture")
    if args.material_identity or args.camera_diagnostics or args.fern_wind or args.pause_notifications or args.shore_edit:
        other_fixtures = ("HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE", "HELLOMINE3D_COMBAT_FIXTURE",
            "HELLOMINE3D_CONTAINER_FIXTURE", "HELLOMINE3D_CRAFTING_FIXTURE", "HELLOMINE3D_CROP_FIXTURE",
            "HELLOMINE3D_MACHINE_FIXTURE", "HELLOMINE3D_ORE_FIXTURE", "HELLOMINE3D_SPAWN_VALIDATION_ACTORS",
            "HELLOMINE3D_TRANSPARENT_FIXTURE", "HELLOMINE3D_VERTEX_LIGHTING_FIXTURE",
            "HELLOMINE3D_VERTICAL_SLICE_FIXTURE", "HELLOMINE3D_HUD_FIXTURE", "HELLOMINE3D_HUD_PAGE_FIXTURE",
            "HELLOMINE3D_ACTOR_VISUAL_CAPTURE", "HELLOMINE3D_VISUAL_CAMERA_SWEEP", "HELLOMINE3D_VISUAL_CAMERA_PATH",
            "HELLOMINE3D_RC_PERF_PROFILE", "HELLOMINE3D_E2_BATCH_MANIFEST", "HELLOMINE3D_V10C_FALLBACK",
            "HELLOMINE3D_V10E_SETTINGS_FIXTURE", "HELLOMINE3D_V10D_SHADOW_FIXTURE", "HELLOMINE3D_V10E_POST_FIXTURE",
            "HELLOMINE3D_HUD_INSPECT_SLOT", "HELLOMINE3D_FORCE_LEGACY_TERRAIN", "HELLOMINE3D_TERRAIN_FALLBACK",
            "HELLOMINE3D_V10D_SHADOW_FALLBACK", "HELLOMINE3D_V10E_POST_FALLBACK", "HELLOMINE3D_CONTROLLED_CRASH")
        if any(name in os.environ for name in other_fixtures):
            parser.error("Inherited diagnostic fixtures cannot be combined with a consumer observer")
    if platform.system() != "Darwin":
        parser.error("macOS required")
    if not 0 <= args.time < 24000:
        parser.error("--time must be in [0, 24000)")
    if not -(2**31) <= args.seed < 2**31:
        parser.error("--seed must fit a signed 32-bit integer")
    if not 45 <= args.fov <= 120:
        parser.error("--fov must be in [45, 120]")
    if not 640 <= args.width <= 3840 or not 480 <= args.height <= 2160:
        parser.error("Window size must be within 640..3840 by 480..2160")
    if args.scene == "menu" and (args.performance or args.hud_fixture or args.streaming or args.panel or args.actor_visual):
        parser.error("menu capture cannot run gameplay fixtures/performance")
    if args.scene == "menu" and (args.position or args.rotation):
        parser.error("menu capture has no world position or rotation")
    if args.scene == "menu" and args.save_template:
        parser.error("menu capture cannot load a world template")
    if args.streaming and not args.performance:
        parser.error("--streaming requires --performance")
    app = args.app.resolve(strict=True)
    source_identity = json.loads(
        (app / "Contents/Resources/build-identity.json").read_text())
    if not args.foreground and not source_identity.get("capabilities", {}).get(
            "hidden_window_no_activate_v1", False):
        parser.error("Package lacks verified hidden-window support; rebuild/repackage it. No app launched.")
    output = args.output.absolute()
    if output.exists():
        parser.error("Output must be new; failed attempts are retained")
    output.mkdir(parents=True)
    template = args.save_template.resolve(strict=True) if args.save_template else None
    template_meta_sha256 = None
    if template:
        template_meta_sha256 = digest(template / "world.meta")
        shutil.copytree(template, output / "save")
    if args.reuse_app:
        verified_package_entries(app)
        runtime_app = app
    else:
        runtime_app = output / "Runtime.app"
        clone_verified_package(app, runtime_app)
    root = runtime_app / "Contents/Resources"
    identity = json.loads((root / "build-identity.json").read_text())
    if digest(root / "bin/HelloMine3D") != identity["executable_sha256"]:
        raise ValueError("Executable identity mismatch")
    # A complete v8 set of the mandatory fields; other fields take the
    # production defaults. These are settings, not modifications to a save.
    settings = f"""settings_version 8
renderdistance {args.render_distance}
directionalshadowquality {args.shadow}
postprocessingquality {args.post}
fullscreen 0
windowsize {args.width} {args.height}
fov {args.fov}
uiscale {args.ui_scale}
locale {args.locale}
audiocaptions 1
actionhints 1
sprintmode hold
sneakmode hold
feedbackintensity {args.feedback}
mouse_break_attack primary
mouse_use secondary
mouse_place secondary
mouse_guard secondary
seed random
"""
    if args.visual_detail:
        settings = settings.replace("settings_version 8", "settings_version 9")
        settings += f"visualdetail {args.visual_detail}\n"
    if args.minimap_range:
        settings = settings.replace("settings_version 8", "settings_version 10").replace(
            "settings_version 9", "settings_version 10")
        if not args.visual_detail:
            settings += "visualdetail standard\n"
        settings += f"minimaprange {args.minimap_range}\n"
    if args.perspective:
        settings = settings.replace(settings.splitlines()[0], "settings_version 11", 1)
        if not args.visual_detail and not args.minimap_range:
            settings += "visualdetail standard\n"
        if not args.minimap_range:
            settings += "minimaprange 128\n"
        settings += f"cameraperspective {args.perspective}\n"
    (root / "bin/config.txt").write_text(settings)
    environment = {
        "HELLOMINE3D_ROOT": str(root),
        "HELLOMINE3D_WINDOW_HIDDEN": "0" if args.foreground else "1",
        "HELLOMINE3D_CATALOGUE_DIR": str(output / "catalogue"),
        "HELLOMINE3D_SHOW_DEBUG_INFO": "1" if args.debug else "0",
        "HELLO_RENDER_CAPTURE": "0" if args.performance else "1",
        "HELLO_RENDER_CAPTURE_DIR": str(output / "frames"),
        "HELLO_RENDER_CAPTURE_MS": ','.join(map(str, capture_times)),
        "HELLO_RENDER_CAPTURE_MAX_DELTA_MS": "5000",
        "HELLO_RENDER_CAPTURE_EXIT": "0" if args.performance else "1",
    }
    if args.material_identity:
        environment["HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR"] = str(output / "material-identity")
        environment["HELLO_RENDER_CAPTURE_MS"] = "60000"
        environment["HELLO_RENDER_CAPTURE_EXIT"] = "0"
    if args.camera_diagnostics:
        environment["HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR"] = str(output / "camera-diagnostics")
        environment["HELLO_RENDER_CAPTURE_MS"] = "60000"
        environment["HELLO_RENDER_CAPTURE_EXIT"] = "0"
    if args.fern_wind:
        environment["HELLOMINE3D_FERN_WIND_CAPTURE_DIR"] = str(output / "fern-wind")
        environment["HELLO_RENDER_CAPTURE_MS"] = "60000"
        environment["HELLO_RENDER_CAPTURE_EXIT"] = "0"
    if args.pause_notifications:
        environment["HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR"] = str(output / "pause-notifications")
        environment["HELLO_RENDER_CAPTURE_MS"] = "60000"
        environment["HELLO_RENDER_CAPTURE_EXIT"] = "0"
    if args.shore_edit:
        environment["HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR"] = str(output / "shore-edit")
        environment["HELLOMINE3D_SHORE_EDIT_TARGET"] = shore_edit_target
        environment["HELLOMINE3D_SHORE_EDIT_SITE"] = shore_edit_site
        environment["HELLO_RENDER_CAPTURE_MS"] = "60000"
        environment["HELLO_RENDER_CAPTURE_EXIT"] = "0"
    if args.terrain_fallback:
        environment["HELLOMINE3D_FORCE_LEGACY_TERRAIN"] = "1"
    if args.player_motion:
        environment["HELLOMINE3D_PLAYER_MOTION_CAPTURE"] = args.player_motion
    if args.scene != "menu":
        position, rotation = SCENES[args.scene]
        position = args.position or position
        rotation = args.rotation or rotation
        if args.camera_diagnostics:
            position, rotation = "0.5 200 0.5", "0 0 0"
        if args.fern_wind:
            position, rotation = "966.5 81 -21.5", "30 0 0"
        if args.shore_edit:
            position, rotation = SHORE_EDIT_SITES[shore_edit_site]
        for value in (position, rotation):
            try:
                if len(value.split()) != 3:
                    raise ValueError
                [float(part) for part in value.split()]
            except ValueError:
                parser.error("position and rotation must each contain three numbers")
        environment.update({
            "HELLOMINE3D_SAVE_DIR": str(output / "save"),
            "HELLOMINE3D_SEED": str(args.seed),
            "HELLOMINE3D_PLAYER_POSITION": position,
            "HELLOMINE3D_PLAYER_ROTATION": rotation,
            "HELLOMINE3D_WORLD_TIME": str(args.time),
        })
    if args.actor_visual:
        environment["HELLOMINE3D_ACTOR_VISUAL_CAPTURE"] = args.actor_visual
    if args.actor_distance is not None:
        environment["HELLOMINE3D_ACTOR_VISUAL_DISTANCE"] = str(args.actor_distance)
    if args.panel in ("map", "journal", "pointer"):
        environment["HELLOMINE3D_HUD_PAGE_FIXTURE"] = args.panel
    elif args.panel in ("furnace", "crusher"):
        environment["HELLOMINE3D_MACHINE_FIXTURE"] = args.panel
    elif args.panel:
        key = "HELLOMINE3D_V10E_SETTINGS_FIXTURE" if args.panel == "settings" else (
            "HELLOMINE3D_" + args.panel.upper() + "_FIXTURE")
        environment[key] = "1"
    if args.atmosphere_fallback:
        environment["HELLOMINE3D_V10C_FALLBACK"] = "1"
    if args.inspect_slot is not None:
        environment["HELLOMINE3D_HUD_INSPECT_SLOT"] = str(args.inspect_slot)
    if args.hud_fixture:
        environment["HELLOMINE3D_HUD_FIXTURE"] = "1"
    if args.performance:
        environment.update({
            "HELLO_PERF_CAPTURE": "1",
            "HELLO_PERF_CAPTURE_DIR": str(output / "performance"),
            "HELLO_PERF_CAPTURE_WARMUP_MS": "5000",
            "HELLO_PERF_CAPTURE_DURATION_MS": "30000",
            "HELLO_PERF_CAPTURE_EXIT": "1",
        })
    if args.streaming:
        environment["HELLOMINE3D_RC_PERF_PROFILE"] = "fast-streaming"
    if args.launch_method == "open":
        command = ["/usr/bin/open", "-n", "-W", "--stdout", str(output / "client.log"),
                   "--stderr", str(output / "client-stderr.log")]
        if not args.foreground:
            command.extend(["-g", "-j"])
        for key, value in environment.items():
            command.extend(["--env", f"{key}={value}"])
        command.append(str(runtime_app))
    else:
        command = [str(runtime_app / "Contents/MacOS/HelloMine3D")]
    record = {"schema": 1, "evidence_type": "DEVELOPER_DIAGNOSTIC", "normal_input": False,
              "source_app": str(app), "runtime_app": str(runtime_app),
              "package_mode": "REUSE_STABLE_APP" if args.reuse_app else "VERIFIED_COPY",
              "save_template": str(template) if template else None,
              "save_template_meta_sha256": template_meta_sha256,
              "package_identity": identity,
              "scene": args.scene, "settings": settings, "environment": environment,
              "diagnostic_fixture": "camera-fixed-resident-origin" if args.camera_diagnostics else
                  ("fern-natural-source-native-draw" if args.fern_wind else
                   ("pause-notification-production-rail" if args.pause_notifications else
                    ("shore-edit-production-world-map" if args.shore_edit else None))),
              "inherited_diagnostic_environment": {
                  key: os.environ[key] for key in (
                      "HELLOMINE3D_VISUAL_CAMERA_SWEEP", "HELLOMINE3D_VISUAL_CAMERA_PATH",
                      "HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE") if key in os.environ},
              "window_size_points": [args.width, args.height],
              "window_mode": "foreground" if args.foreground else "hidden",
              "render_readback": not args.performance,
              "expected_pixel_ratio": args.pixel_ratio,
              "platform": platform.platform(), "host_architecture": platform.machine(),
              "command": command, "launch_method": args.launch_method,
              "started_unix": time.time(), "result": "RUNNING"}
    record_path = output / "capture.json"
    record_path.write_text(json.dumps(record, indent=2) + "\n")
    try:
        if args.launch_method == "open":
            subprocess.run(command, check=True, timeout=100)
        else:
            with (output / "client.log").open("w") as stdout, \
                    (output / "client-stderr.log").open("w") as stderr:
                if args.fern_wind or args.pause_notifications or args.shore_edit:
                    child = subprocess.Popen(command,
                        env={**os.environ, **environment}, stdout=stdout, stderr=stderr)
                    record["child_pid"] = child.pid
                    if args.shore_edit:
                        record["child_timed_out"] = False
                        record["child_wait_timeout_seconds"] = 60
                    record_path.write_text(json.dumps(record, indent=2) + "\n")
                    try:
                        child.wait(timeout=60 if args.pause_notifications or args.shore_edit else 100)
                    except subprocess.TimeoutExpired:
                        child.kill()
                        child.wait()
                        record["child_timed_out"] = True
                        raise
                    finally:
                        record["child_returncode"] = child.returncode
                        record["child_signal"] = -child.returncode if child.returncode is not None and child.returncode < 0 else None
                    if child.returncode:
                        raise subprocess.CalledProcessError(child.returncode, command)
                else:
                    subprocess.run(command, check=True, timeout=100,
                                   env={**os.environ, **environment},
                                   stdout=stdout, stderr=stderr)
            # macOS reports ru_maxrss in bytes. In direct mode the executable
            # is the child we waited for; LaunchServices mode cannot claim that.
            record["peak_child_rss_bytes"] = int(
                resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
        frames = sorted((output / "material-identity").glob("client-*.png")) if args.material_identity else \
            sorted((output / "camera-diagnostics").glob("*.png")) if args.camera_diagnostics else \
            sorted((output / "fern-wind").glob("*.png")) if args.fern_wind else \
            sorted((output / "pause-notifications").glob("frame-*.png")) if args.pause_notifications else \
            sorted((output / "shore-edit").glob("phase-*.png")) if args.shore_edit else \
            sorted((output / "frames").glob("*.png"))
        expected_frames = 9 if args.material_identity else (6 if args.camera_diagnostics else
            (4 if args.fern_wind else (12 if args.pause_notifications else
            (6 if args.shore_edit else (0 if args.performance else len(capture_times))))))
        if len(frames) != expected_frames:
            raise RuntimeError(f"Expected {expected_frames} captured frames, got {len(frames)}")
        if args.shore_edit and [frame.name for frame in frames] != [f"phase-{phase:03d}.png" for phase in range(6)]:
            raise RuntimeError("Shore edit capture requires exactly phase-000.png through phase-005.png")
        # Window points and framebuffer pixels differ on Retina displays. Require
        # an explicit ratio so an unexpected resolution still fails the capture.
        import struct
        record["frame_sizes_pixels"] = {}
        expected_size = (args.width * args.pixel_ratio, args.height * args.pixel_ratio)
        for frame in frames:
            data = frame.read_bytes()
            if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
                raise RuntimeError(f"Invalid PNG header: {frame.name}")
            width, height = struct.unpack(">II", data[16:24])
            record["frame_sizes_pixels"][frame.name] = [width, height]
            if (width, height) != expected_size:
                raise RuntimeError(f"Actual frame is {width}x{height}, expected "
                                   f"{expected_size[0]}x{expected_size[1]} at pixel ratio {args.pixel_ratio}")
        artifacts = frames
        if args.material_identity:
            index = output / "material-identity/index.json"
            session = json.loads(index.read_text())
            if len(session.get("frames", [])) != 9:
                raise RuntimeError("Material identity session did not finish its nine actual backend frames")
            artifacts += [index]
            record["material_identity_session"] = str(index)
            record["material_identity_scope"] = "actual consumer capture; independent oracle required; normal input remains false"
        if args.fern_wind:
            index = output / "fern-wind/index.json"
            session = json.loads(index.read_text())
            if len(session.get("frames", [])) != 4:
                raise RuntimeError("Fern session did not finish its four actual native draw frames")
            artifacts += [index]
            record["fern_wind_session"] = str(index)
            record["fern_wind_scope"] = "natural source native draw capture; independent oracle required; normal input remains false"
        if args.pause_notifications:
            index = output / "pause-notifications/index.json"
            session = json.loads(index.read_text())
            if session.get("status") != "CAPTURED" or len(session.get("frames", [])) != 12 or session.get("status_submit_count") != 3:
                raise RuntimeError("Pause notification session did not complete twelve actual backend frames and three discrete status events")
            artifacts += [index]
            record["pause_notifications_session"] = str(index)
            record["pause_notifications_scope"] = "actual production rail; independent glyph/clip/PNG oracle required; normal input false"
        if args.shore_edit:
            index = output / "shore-edit/index.json"
            session = json.loads(index.read_text())
            if (session.get("schema") != "hellomine3d-shore-edit-capture-v1" or
                    session.get("status") != "CAPTURED" or len(session.get("frames", [])) != 6):
                raise RuntimeError("Shore edit session did not complete its six production world-map capture phases")
            world_maps = [output / "shore-edit" / f"phase-{phase:03d}-world-map.json" for phase in range(6)]
            for path in world_maps:
                json.loads(path.read_text())
            artifacts += sorted(path for path in (output / "shore-edit").iterdir()
                                if path.is_file() and path not in artifacts)
            record["shore_edit_session"] = str(index)
            record["shore_edit_site"] = shore_edit_site
            record["shore_edit_target"] = [int(part) for part in shore_edit_target.split()]
            record["shore_edit_scope"] = "production Place/Break commands and diagnostic restore; actual world-map/UI facts; independent oracle required; normal input false"
        if args.performance:
            record["framebuffer_size_pixels"] = performance_framebuffer(
                (output / "client.log").read_text(), args.width, args.height, args.pixel_ratio)
            record["framebuffer_evidence"] = "Cocoa native window creation log"
            artifacts += [output / "performance/summary.txt", output / "performance/frames.csv"]
        for path in artifacts:
            if not path.is_file() or not path.stat().st_size:
                raise RuntimeError(f"Missing artifact {path}")
        record["artifacts"] = {str(p.relative_to(output)): digest(p) for p in artifacts}
        if args.scene != "menu":
            record["world_metadata"] = (output / "save/world.meta").read_text()
            if args.shore_edit:
                metadata_lines = record["world_metadata"].splitlines()
                if metadata_lines.count("seed 42") != 1 or metadata_lines.count("terrain_generation_version 30") != 1:
                    raise RuntimeError("Shore edit capture requires actual saved seed42/terrain30 metadata")
        record["result"] = "CAPTURED"
    except Exception as error:
        record["result"] = "FAIL"
        record["error"] = str(error)
        raise
    finally:
        record["finished_unix"] = time.time()
        record_path.write_text(json.dumps(record, indent=2) + "\n")
    print(record_path)


if __name__ == "__main__":
    main()
