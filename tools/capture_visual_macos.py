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
    parser.add_argument("--shadow", choices=("off", "medium", "high"), default="off")
    parser.add_argument("--post", choices=("off", "on"), default="off")
    parser.add_argument("--locale", choices=("en-US", "zh-CN"), default="zh-CN")
    parser.add_argument("--ui-scale", type=float, choices=(0.85, 1.0, 1.25), default=1.0)
    parser.add_argument("--feedback", choices=("off", "reduced", "full"), default="full")
    parser.add_argument("--minimap-range", type=int, choices=(64, 128, 256))
    parser.add_argument("--actor-visual", choices=("idle", "windup", "recover", "walk", "cycle", "projectiles", "projectile-flight"))
    parser.add_argument("--actor-distance", type=int, choices=(6, 12, 24),
                        help="Diagnostic gallery distance; requires --actor-visual")
    parser.add_argument("--hud-fixture", action="store_true")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--panel", choices=("crafting", "container", "settings"))
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
    if args.performance and args.capture_ms != "5000,10000":
        parser.error("--capture-ms applies only to render capture")
    if args.actor_distance is not None and not args.actor_visual:
        parser.error("--actor-distance requires --actor-visual")
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
renderdistance 8
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
    if args.terrain_fallback:
        environment["HELLOMINE3D_FORCE_LEGACY_TERRAIN"] = "1"
    if args.scene != "menu":
        position, rotation = SCENES[args.scene]
        position = args.position or position
        rotation = args.rotation or rotation
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
    if args.panel:
        key = "HELLOMINE3D_V10E_SETTINGS_FIXTURE" if args.panel == "settings" else (
            "HELLOMINE3D_" + args.panel.upper() + "_FIXTURE")
        environment[key] = "1"
    if args.atmosphere_fallback:
        environment["HELLOMINE3D_V10C_FALLBACK"] = "1"
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
                subprocess.run(command, check=True, timeout=100,
                               env={**os.environ, **environment},
                               stdout=stdout, stderr=stderr)
            # macOS reports ru_maxrss in bytes. In direct mode the executable
            # is the child we waited for; LaunchServices mode cannot claim that.
            record["peak_child_rss_bytes"] = int(
                resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
        frames = sorted((output / "frames").glob("*.png"))
        expected_frames = 0 if args.performance else len(capture_times)
        if len(frames) != expected_frames:
            raise RuntimeError(f"Expected {expected_frames} captured frames, got {len(frames)}")
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
