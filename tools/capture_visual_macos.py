#!/usr/bin/env python3
"""Repeatable visual/performance diagnostics in a disposable macOS package.

This uses forced viewpoints and optionally a HUD fixture. It is developer
diagnostic evidence, never normal-input or independent gameplay acceptance.
The supplied package and its user settings/saves are never modified.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
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


def clone_verified_package(source, destination):
    inventory = source / "Contents/Resources/distribution-sha256.txt"
    for entry in inventory.read_text().splitlines():
        expected, relative = entry.split("  ", 1)
        path = Path(relative)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError(f"Unsafe inventory path: {relative}")
        original = source / path
        if digest(original) != expected:
            raise ValueError(f"Package input changed: {relative}")
        target = destination / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    shutil.copy2(inventory, destination / inventory.relative_to(source))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scene", choices=SCENES, default="forest")
    parser.add_argument("--time", type=int, default=6000)
    parser.add_argument("--shadow", choices=("off", "medium", "high"), default="off")
    parser.add_argument("--post", choices=("off", "on"), default="off")
    parser.add_argument("--locale", choices=("en-US", "zh-CN"), default="zh-CN")
    parser.add_argument("--ui-scale", type=float, choices=(0.85, 1.0, 1.25), default=1.0)
    parser.add_argument("--hud-fixture", action="store_true")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--panel", choices=("crafting", "container"))
    parser.add_argument("--atmosphere-fallback", action="store_true")
    parser.add_argument("--terrain-fallback", action="store_true")
    parser.add_argument("--visual-detail", choices=("standard", "compatibility"))
    parser.add_argument("--performance", action="store_true")
    parser.add_argument("--streaming", action="store_true")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("macOS required")
    if not 0 <= args.time < 24000:
        parser.error("--time must be in [0, 24000)")
    if args.scene == "menu" and (args.performance or args.hud_fixture or args.streaming or args.panel):
        parser.error("menu capture cannot run gameplay fixtures/performance")
    if args.streaming and not args.performance:
        parser.error("--streaming requires --performance")
    app = args.app.resolve(strict=True)
    output = args.output.absolute()
    if output.exists():
        parser.error("Output must be new; failed attempts are retained")
    output.mkdir(parents=True)
    clone = output / "Runtime.app"
    clone_verified_package(app, clone)
    root = clone / "Contents/Resources"
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
windowsize 1280 720
fov 90
uiscale {args.ui_scale}
locale {args.locale}
audiocaptions 1
actionhints 1
sprintmode hold
sneakmode hold
feedbackintensity full
mouse_break_attack primary
mouse_use secondary
mouse_place secondary
mouse_guard secondary
seed random
"""
    if args.visual_detail:
        settings = settings.replace("settings_version 8", "settings_version 9")
        settings += f"visualdetail {args.visual_detail}\n"
    (root / "bin/config.txt").write_text(settings)
    environment = {
        "HELLOMINE3D_ROOT": str(root),
        "HELLOMINE3D_CATALOGUE_DIR": str(output / "catalogue"),
        "HELLOMINE3D_SHOW_DEBUG_INFO": "1" if args.debug else "0",
        "HELLO_RENDER_CAPTURE": "1",
        "HELLO_RENDER_CAPTURE_DIR": str(output / "frames"),
        "HELLO_RENDER_CAPTURE_MS": "5000,10000",
        "HELLO_RENDER_CAPTURE_MAX_DELTA_MS": "5000",
        "HELLO_RENDER_CAPTURE_EXIT": "0" if args.performance else "1",
    }
    if args.terrain_fallback:
        environment["HELLOMINE3D_FORCE_LEGACY_TERRAIN"] = "1"
    if args.scene != "menu":
        position, rotation = SCENES[args.scene]
        environment.update({
            "HELLOMINE3D_SAVE_DIR": str(output / "save"),
            "HELLOMINE3D_SEED": "20260807",
            "HELLOMINE3D_PLAYER_POSITION": position,
            "HELLOMINE3D_PLAYER_ROTATION": rotation,
            "HELLOMINE3D_WORLD_TIME": str(args.time),
        })
    if args.panel:
        environment["HELLOMINE3D_" + args.panel.upper() + "_FIXTURE"] = "1"
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
    command = ["/usr/bin/open", "-n", "-W", "--stdout", str(output / "client.log"),
               "--stderr", str(output / "client-stderr.log")]
    for key, value in environment.items():
        command.extend(["--env", f"{key}={value}"])
    command.append(str(clone))
    record = {"schema": 1, "evidence_type": "DEVELOPER_DIAGNOSTIC", "normal_input": False,
              "source_app": str(app), "package_identity": identity,
              "scene": args.scene, "settings": settings, "environment": environment,
              "platform": platform.platform(), "host_architecture": platform.machine(),
              "command": command, "started_unix": time.time(), "result": "RUNNING"}
    record_path = output / "capture.json"
    record_path.write_text(json.dumps(record, indent=2) + "\n")
    try:
        subprocess.run(command, check=True, timeout=100)
        frames = sorted((output / "frames").glob("*.png"))
        if len(frames) != 2:
            raise RuntimeError(f"Expected 2 captured frames, got {len(frames)}")
        artifacts = frames
        if args.performance:
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
