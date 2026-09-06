#!/usr/bin/env python3
"""Launch an isolated macOS app and capture its actual visible window.

Uses LaunchServices and screencapture; never injects gameplay or captures the
whole desktop as a fallback. Requires macOS desktop Screen Recording access.
"""

import argparse
import hashlib
import json
import platform
import plistlib
from pathlib import Path
import struct
import subprocess
import sys
import time
from datetime import datetime, timezone


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def utc():
    return datetime.now(timezone.utc).isoformat()


def run(command, timeout=30):
    return subprocess.run(command, check=True, capture_output=True, text=True,
                          timeout=timeout).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="New PNG file; existing output/evidence is never overwritten")
    parser.add_argument("--launch", action="store_true",
                        help="Open the app normally if it is not already running")
    parser.add_argument("--window-id", type=int,
                        help="Select one of this exact app's windows when several exist")
    parser.add_argument("--configuration", choices=("Debug", "Release"), required=True,
                        help="Actual build configuration, supplied by the examiner")
    parser.add_argument("--note", default="",
                        help="Examiner's scene/settings description, not inferred verification")
    parser.add_argument("--wait-seconds", type=float, default=15)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This tool requires macOS")
    app = args.app.resolve(strict=True)
    if app.suffix != ".app":
        parser.error("--app must name an extracted .app")
    output = args.output.absolute()
    record_path = output.with_suffix(".json")
    if output.suffix.lower() != ".png" or output.exists() or record_path.exists():
        parser.error("Choose a new .png output and unused matching .json path")
    if not 0 <= args.wait_seconds <= 60:
        parser.error("--wait-seconds must be between 0 and 60")
    with (app / "Contents/Info.plist").open("rb") as stream:
        bundle = plistlib.load(stream)
    bundle_id = bundle["CFBundleIdentifier"]
    binary = app / "Contents/Resources/bin/HelloMine3D"
    if not binary.is_file():
        parser.error("Expected a HelloMine3D isolated package executable")
    root = Path(__file__).resolve().parents[1]
    source = Path(__file__).with_name("macos_window_query.m")
    cache = root / "build/macos-window-evidence"
    cache.mkdir(parents=True, exist_ok=True)
    helper = cache / ("query-" + digest(source)[:16])
    output.parent.mkdir(parents=True, exist_ok=True)
    record = {
        "started_at_utc": utc(), "platform": platform.platform(),
        "host_architecture": platform.machine(), "app": str(app),
        "bundle_id": bundle_id, "configuration": args.configuration,
        "examiner_note": args.note, "executable_sha256": digest(binary),
        "capture_method": "macOS screencapture of an exact app-owned visible window",
        "query_source_sha256": digest(source), "commands": [],
        "result": "FAIL", "gameplay_acceptance": "NOT_CLAIMED",
    }
    identity = app / "Contents/Resources/build-identity.json"
    if identity.is_file():
        record["package_build_identity"] = json.loads(identity.read_text())
    try:
        if not helper.exists():
            command = ["/usr/bin/xcrun", "clang", "-fobjc-arc", "-Wall", "-Wextra",
                       "-framework", "AppKit", "-framework", "CoreGraphics",
                       str(source), "-o", str(helper)]
            record["commands"].append(command)
            run(command, timeout=60)
        record["query_executable_sha256"] = digest(helper)
        query_command = [str(helper), bundle_id, str(app)]
        record["commands"].append(query_command)

        def query():
            return json.loads(run(query_command))

        state = query()
        if not state["pids"] and args.launch:
            command = ["/usr/bin/open", str(app)]
            record["commands"].append(command)
            run(command)
        deadline = time.monotonic() + args.wait_seconds
        while not state["windows"] and time.monotonic() < deadline:
            time.sleep(0.25)
            state = query()
        record["window_query"] = state
        windows = state["windows"]
        if args.window_id is not None:
            windows = [w for w in windows if w["id"] == args.window_id]
        if len(windows) != 1:
            raise RuntimeError("Expected exactly one visible window of this app; "
                               "use --window-id if needed. No desktop fallback.")
        window = windows[0]
        record["window"] = window
        command = ["/usr/sbin/screencapture", "-x", "-o", "-t", "png",
                   "-l", str(window["id"]), str(output)]
        record["commands"].append(command)
        record["capture_started_at_utc"] = utc()
        run(command)
        record["capture_finished_at_utc"] = utc()
        data = output.read_bytes()
        if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
            raise RuntimeError("screencapture did not produce a valid PNG header")
        width, height = struct.unpack(">II", data[16:24])
        if not width or not height:
            raise RuntimeError("Empty screenshot dimensions")
        record.update(result="CAPTURED", image=str(output),
                      image_pixels=[width, height], image_bytes=len(data),
                      image_sha256=hashlib.sha256(data).hexdigest(),
                      visual_review="PENDING")
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        record["error"] = str(error)
        if isinstance(error, subprocess.CalledProcessError):
            record["stderr"] = error.stderr
        print(str(error), file=sys.stderr)
    finally:
        record["finished_at_utc"] = utc()
        with record_path.open("x") as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
    print(record_path)
    return 0 if record["result"] == "CAPTURED" else 1


if __name__ == "__main__":
    sys.exit(main())
