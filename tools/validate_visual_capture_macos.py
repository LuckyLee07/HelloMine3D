#!/usr/bin/env python3
"""Portable regression checks for readback-free macOS performance validation."""
from capture_visual_macos import performance_framebuffer


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
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
