#!/usr/bin/env python3
"""Portable, no-client checks for macOS capture formats and performance validation."""
from pathlib import Path
import os
import struct
import subprocess
import sys

from capture_visual_macos import capture_frame_size, performance_framebuffer


def bmp_data(width=3, height=2, bits=24):
    """Small in-memory BI_RGB specimen with DWORD-padded rows."""
    pixels = bytes(((width * bits + 31) // 32) * 4 * abs(height))
    return (struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54) +
            struct.pack("<IiiHHIIiiII", 40, width, height, 1, bits, 0,
                        len(pixels), 0, 0, 0, 0) + pixels)


def changed(data, offset, fmt, value):
    result = bytearray(data)
    struct.pack_into(fmt, result, offset, value)
    return bytes(result)


def main():
    native = "Cocoa: Window created 1280 x 720 with backing store size 2560 x 1440 using content scaling factor 2.0\n"
    cases = [
        ("retina", native, 1280, 720, 2, True),
        ("wrong-pixel-ratio", native, 1280, 720, 1, False),
        ("wrong-window-points", native, 2560, 1440, 1, False),
        ("missing-window", "", 1280, 720, 2, False),
        ("ambiguous-window", native + native, 1280, 720, 2, False),
        ("readback-enabled", native + "[OgreRenderCapture] enabled dir=test", 1280, 720, 2, False),
        ("unexpected-frame", native + "[OgreRenderCapture] captured path=test", 1280, 720, 2, False),
    ]
    failures = 0
    for name, log, width, height, ratio, expected in cases:
        try:
            actual = performance_framebuffer(log, width, height, ratio) == [width * ratio, height * ratio]
        except RuntimeError:
            actual = False
        passed = actual == expected
        failures += not passed
        print(f"[VISUAL_CAPTURE] {name}: {'PASS' if passed else 'FAIL'}")
    bmp = bmp_data()
    png_header = b"\x89PNG\r\n\x1a\n" + struct.pack(">I4sII", 13, b"IHDR", 1280, 720)
    frame_cases = [
        ("png-dimensions", "png", png_header, (1280, 720)),
        ("bmp24-row-padding", "bmp", bmp, (3, 2)),
        ("bmp32-top-down", "bmp", bmp_data(height=-2, bits=32), (3, 2)),
        ("bmp-zero-image-size", "bmp", changed(bmp, 34, "<I", 0), (3, 2)),
        ("png-as-bmp", "bmp", png_header, None),
        ("bmp-as-png", "png", bmp, None),
        ("bmp-truncated-header", "bmp", bmp[:53], None),
        ("bmp-truncated-pixels", "bmp", bmp[:-1], None),
        ("bmp-trailing-data", "bmp", changed(bmp + b"x", 2, "<I", len(bmp) + 1), None),
        ("bmp-unsupported-dib", "bmp", changed(bmp, 14, "<I", 124), None),
        ("bmp-compressed", "bmp", changed(bmp, 30, "<I", 1), None),
        ("bmp-indexed", "bmp", changed(bmp, 28, "<H", 8), None),
        ("bmp-negative-width", "bmp", changed(bmp, 18, "<i", -3), None),
        ("bmp-zero-height", "bmp", changed(bmp, 22, "<i", 0), None),
        ("bmp-invalid-planes", "bmp", changed(bmp, 26, "<H", 2), None),
        ("bmp-offset-in-header", "bmp", changed(bmp, 10, "<I", 53), None),
        ("bmp-wrong-file-size", "bmp", changed(bmp, 2, "<I", len(bmp) - 1), None),
        ("bmp-wrong-image-size", "bmp", changed(bmp, 34, "<I", 1), None),
    ]
    for name, capture_format, data, expected in frame_cases:
        try:
            actual = capture_frame_size(data, capture_format)
        except RuntimeError:
            actual = None
        passed = actual == expected
        failures += not passed
        print(f"[VISUAL_CAPTURE] {name}: {'PASS' if passed else 'FAIL'}")
    # --app is this regular Python file, so even a missing preflight guard
    # cannot resolve an app identity or create an output directory/launch a client.
    this_file = str(Path(__file__).resolve())
    command = [sys.executable, "-B", str(Path(__file__).with_name("capture_visual_macos.py")),
               "--app", this_file, "--output", this_file, "--capture-format", "bmp",
               "--launch-method", "direct"]
    illegal_options = [
        ("open", ["--launch-method", "open"]), ("foreground", ["--foreground"]),
        ("performance", ["--performance"]), ("material", ["--material-identity"]),
        ("camera", ["--camera-diagnostics"]), ("fern", ["--fern-wind"]),
        ("pause", ["--pause-notifications"]), ("shore", ["--shore-edit"]),
        ("water", ["--water-seam"]), ("actor", ["--actor-visual", "walk"]),
        ("player", ["--player-motion", "forward"]), ("hud", ["--hud-fixture"]),
        ("panel", ["--panel", "crafting"]),
    ]
    for name, options in illegal_options:
        result = subprocess.run(command + options, capture_output=True, text=True, timeout=10)
        passed = result.returncode == 2 and "--capture-format bmp requires" in result.stderr
        failures += not passed
        print(f"[VISUAL_CAPTURE] bmp-reject-{name}: {'PASS' if passed else 'FAIL'}")
    for variable, error_text in (
            ("HELLO_PERF_CAPTURE", "Inherited diagnostic fixtures"),
            ("HELLOMINE3D_PLAYER_MOTION_CAPTURE", "Inherited player motion fixture"),
            ("HELLOMINE3D_ACTOR_VISUAL_CAPTURE", "Inherited diagnostic fixtures"),
            ("HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR", "Inherited camera diagnostics")):
        result = subprocess.run(command, env={**os.environ, variable: "1"},
                                capture_output=True, text=True, timeout=10)
        passed = result.returncode == 2 and error_text in result.stderr
        failures += not passed
        print(f"[VISUAL_CAPTURE] bmp-reject-inherited-{variable}: {'PASS' if passed else 'FAIL'}")
    result = subprocess.run(command + ["--capture-ms", "1,2,3,4,5,6,7,8,9"],
                            capture_output=True, text=True, timeout=10)
    passed = result.returncode == 2 and "up to eight increasing integers" in result.stderr
    failures += not passed
    print(f"[VISUAL_CAPTURE] nine-targets-rejected: {'PASS' if passed else 'FAIL'}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
