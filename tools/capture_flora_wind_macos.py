#!/usr/bin/env python3
"""Capture actual grass/flower motion or its cost from an isolated macOS app.

Forced viewpoints are developer diagnostics, not normal-input acceptance.
Performance runs disable readback; sequence runs save every original frame.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def clone_verified_package(source, destination):
    inventory = source / "Contents/Resources/distribution-sha256.txt"
    for line in inventory.read_text().splitlines():
        expected, relative = line.split("  ", 1)
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
    parser.add_argument("--shadow", choices=("off", "high"), default="off")
    parser.add_argument("--format", choices=("png", "bmp"), default="png",
                        help="BMP reduces synchronous encoding overhead for clients that support it")
    parser.add_argument("--performance", action="store_true")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("macOS required")
    output = args.output.absolute()
    if output.exists():
        parser.error("Output must be new; failures are retained")
    output.mkdir(parents=True)
    app = args.app.resolve(strict=True)
    clone = output / "Runtime.app"
    clone_verified_package(app, clone)
    root = clone / "Contents/Resources"
    identity = json.loads((root / "build-identity.json").read_text())
    assert digest(root / "bin/HelloMine3D") == identity["executable_sha256"]
    settings = f"""settings_version 9
visualdetail standard
renderdistance 8
directionalshadowquality {args.shadow}
postprocessingquality off
fullscreen 0
windowsize 1280 720
fov 70
uiscale 0.85
locale zh-CN
audiocaptions 0
actionhints 0
sprintmode hold
sneakmode hold
feedbackintensity full
mouse_break_attack primary
mouse_use secondary
mouse_place secondary
mouse_guard secondary
seed random
"""
    (root / "bin/config.txt").write_text(settings)
    environment = {
        "HELLOMINE3D_ROOT": str(root),
        "HELLOMINE3D_SAVE_DIR": str(output / "save"),
        "HELLOMINE3D_CATALOGUE_DIR": str(output / "catalogue"),
        "HELLOMINE3D_SEED": "20260807",
        "HELLOMINE3D_PLAYER_POSITION": "1056 106 928",
        "HELLOMINE3D_PLAYER_ROTATION": "8 0 0",
        "HELLOMINE3D_WORLD_TIME": "6000",
        "HELLOMINE3D_SHOW_DEBUG_INFO": "0",
    }
    # 80 frames over eight seconds, including multiple old one-second resets.
    captures = list(range(4000, 12000, 100))
    if args.performance:
        environment.update({
            "HELLO_RENDER_CAPTURE": "0", "HELLO_PERF_CAPTURE": "1",
            "HELLO_PERF_CAPTURE_DIR": str(output / "performance"),
            "HELLO_PERF_CAPTURE_WARMUP_MS": "5000",
            "HELLO_PERF_CAPTURE_DURATION_MS": "30000",
            "HELLO_PERF_CAPTURE_EXIT": "1",
        })
    else:
        environment.update({
            "HELLO_PERF_CAPTURE": "0", "HELLO_RENDER_CAPTURE": "1",
            "HELLO_RENDER_CAPTURE_DIR": str(output / "frames"),
            "HELLO_RENDER_CAPTURE_FORMAT": args.format,
            "HELLO_RENDER_CAPTURE_MS": ",".join(map(str, captures)),
            "HELLO_RENDER_CAPTURE_MAX_DELTA_MS": "5000",
            "HELLO_RENDER_CAPTURE_EXIT": "1",
        })
    command = ["/usr/bin/open", "-n", "-W", "--stdout", str(output / "client.log"),
               "--stderr", str(output / "client-stderr.log")]
    for key, value in environment.items():
        command.extend(["--env", f"{key}={value}"])
    command.append(str(clone))
    record = {"evidence_type": "DEVELOPER_DIAGNOSTIC", "normal_input": False,
              "source_app": str(app), "package_identity": identity,
              "settings": settings, "environment": environment,
              "platform": platform.platform(), "command": command,
              "started_unix": time.time(), "result": "RUNNING"}
    record_path = output / "capture.json"
    record_path.write_text(json.dumps(record, indent=2) + "\n")
    try:
        subprocess.run(command, check=True, timeout=100)
        if args.performance:
            artifacts = [output / "performance/frames.csv", output / "performance/summary.txt"]
        else:
            artifacts = sorted((output / "frames").glob(f"*.{args.format}"))
            if len(artifacts) != len(captures):
                raise RuntimeError(f"Expected {len(captures)} frames; got {len(artifacts)}")
        for path in artifacts:
            if not path.is_file() or not path.stat().st_size:
                raise RuntimeError(f"Missing artifact: {path}")
        if (output / "client-stderr.log").stat().st_size:
            raise RuntimeError("Client reported stderr; inspect before accepting capture")
        record["artifacts"] = {str(p.relative_to(output)): digest(p) for p in artifacts}
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
