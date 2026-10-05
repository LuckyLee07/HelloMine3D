#!/usr/bin/env python3
"""Synthetic CPU adversaries only; packets here are not production evidence."""
import copy
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import sys
import zlib

from pause_notification_capture_oracle import BUTTONS, PHASES, validate_session

COLOUR = 244 | 238 << 8 | 220 << 16 | 255 << 24
WIDTH, HEIGHT = 640, 480


def png(rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    rows = b"".join(b"\0" + bytes(rgba[y * WIDTH * 4:(y + 1) * WIDTH * 4]) for y in range(HEIGHT))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def packet(i, seconds, tick):
    text = ("A_START_OLD_STATUS" if i < 2 else "B_START_NEW_STATUS") + "\n" + "\n".join("content" + str(n) for n in range(1, 16))
    scroll = 60 if i in (1, 4, 6, 7, 8, 9) else 0
    origin = [70.7, 406.8 - scroll]
    glyphs = [{"codepoint": ord(c), "actual_codepoint": ord(c), "advance": 8,
               "bounds": [0, 0, 7, 10], "uv": [ord(c) / 1000, .2, ord(c) / 1000 + .006, .21],
               "visible": True, "coloured": False} for c in text.split("\n")[0]]
    vertices, indices = bytearray(), []
    rgba = bytearray(bytes((25, 45, 52, 255)) * (WIDTH * HEIGHT))
    for j, glyph in enumerate(glyphs if i < 10 else []):
        x, y = math.trunc(origin[0]) + j * 8, math.trunc(origin[1])
        u, v, uu, vv = glyph["uv"]
        for xx, yy, tx, ty in ((x, y, u, v), (x + 7, y, uu, v), (x + 7, y + 10, uu, vv), (x, y + 10, u, vv)):
            vertices.extend(struct.pack("<4fI", xx, yy, tx, ty, COLOUR))
        at = j * 4
        indices.extend((at, at + 1, at + 2, at, at + 2, at + 3))
        for yy in range(max(y, 400), min(y + 10, 464)):
            for xx in range(x, x + 7):
                rgba[4 * (yy * WIDTH + xx):4 * (yy * WIDTH + xx) + 4] = bytes((244, 238, 220, 255))
    status_event = 0 if i < 2 else .2 if i < 5 else .5
    remaining = max(0, 4 - (seconds - status_event))
    caption_event = .7 if i == 7 else .8
    caption_remaining = max(0, 2.5 - (seconds - caption_event)) if i >= 7 else 0
    counts = 1 if i < 2 else 2 if i < 5 else 3
    caption_counts = 0 if i < 7 else 1 if i == 7 else 2
    name = f"frame-{i:03d}"
    frame = {"schema": "hellomine3d-pause-notification-frame-v1", "phase": PHASES[i], "frame": i,
             "tick": tick, "locale": "en-US", "normal_input": False, "actual_native_focused": False,
             "phase_enter_tick": tick, "last_status_submit_tick": 3 if i < 2 else 5 if i < 5 else 8,
             "simulation_allowed": False, "pending_action": 0, "elapsed_seconds": seconds,
             "presentation_seconds": seconds, "actual_delta_seconds": .1,
             "status": {"text": text, "remaining_seconds": remaining, "submit_count": counts},
             "caption": {"cue": "diagnostic.pause.scroll" if caption_remaining else "", "text": "caption" if caption_remaining else "",
                         "remaining_seconds": caption_remaining, "submit_count": caption_counts},
             "io": {"mouse_position": [320, 432], "mouse_wheel": -1 if i in (1, 4, 6) else 0, "app_focus_lost": False},
             "rail": {"name": "##PauseNotifications", "id": 99, "position": [60, 400], "size": [520, 64],
                      "inner_clip": [60, 400, 580, 464], "scroll_y": scroll, "scroll_max_y": 160,
                      "content_size_previous": [520, 224], "hovered": True, "focused": False,
                      "draw_list_owner": "##PauseNotifications"} if i < 10 else None,
             "status_text": {"text": text, "first_explicit_line": text.split("\n")[0], "origin": origin,
                             "font_size": 10, "baked_size": 10, "wrap": 500, "colour": COLOUR,
                             "measured_full_size": [200, 160], "glyphs": glyphs, "font_texture_id": "12",
                             "vertex_begin": 0, "vertex_end": len(vertices) // 20, "index_begin": 0, "index_end": len(indices),
                             "draw_list_owner": "##PauseNotifications", "add_text_clip": [60, 400, 580, 464]} if i < 10 else None,
             "buttons": {button: {"rect": [200, 40 + j * 50, 440, 70 + j * 50], "window_id": 20 + j,
                                    "window_focused": False, "hovered": False} for j, button in enumerate(sorted(BUTTONS))},
             "draw_data": {"display_position": [0, 0], "display_size": [WIDTH, HEIGHT], "framebuffer_scale": [1, 1],
                           "vertex_stride": 20, "position_offset": 0, "uv_offset": 8, "colour_offset": 16,
                           "colour_packing": "IM_COL32_RGBA_shifts_0_8_16_24", "index_bytes": 2,
                           "lists": [{"owner": "##PauseNotifications", "vertex_file": name + ".vbo.bin",
                                      "index_file": name + ".ibo.bin", "vertex_count": len(vertices) // 20,
                                      "index_count": len(indices), "commands": [{"element_count": len(indices), "index_offset": 0,
                                      "vertex_offset": 0, "clip": [60, 400, 580, 464], "texture_id": "12",
                                      "has_callback": False, "reset_callback": False}]}]},
             "png": name + ".png", "png_source": "SYNTHETIC CPU MOCK ONLY", "image_row_origin": "top_left"}
    return frame, vertices, struct.pack("<" + str(len(indices)) + "H", *indices), rgba


def build(directory):
    directory.mkdir()
    times = [.1, .2, .3, .4, .5, .6, .7, .8, .9, 3.4, 4.6, 4.7]
    ticks = list(range(3, 12)) + [37, 49, 50]
    frames = []
    for i, (seconds, tick) in enumerate(zip(times, ticks)):
        frame, vb, ib, rgba = packet(i, seconds, tick)
        name = f"frame-{i:03d}"
        (directory / (name + ".json")).write_text(json.dumps(frame))
        (directory / (name + ".vbo.bin")).write_bytes(vb)
        (directory / (name + ".ibo.bin")).write_bytes(ib)
        (directory / (name + ".png")).write_bytes(png(rgba))
        frames.append(name + ".json")
    text_a, text_b = packet(0, .1, 3)[0]["status"]["text"], packet(2, .3, 5)[0]["status"]["text"]
    events = []
    for phase, typ, seconds, tick, value in ((0, "status_submit", 0, 3, text_a), (1, "wheel", .1, 4, ""),
        (2, "status_submit", .2, 5, text_b), (4, "wheel", .4, 7, ""), (5, "status_submit", .5, 8, text_b),
        (6, "wheel", .6, 9, ""), (7, "caption_submit", .7, 10, "caption"), (8, "caption_submit", .8, 11, "caption")):
        events.append({"tick": tick, "phase": PHASES[phase], "type": typ, "text": value,
                       "wheel_relative": -120 if typ == "wheel" else 0,
                       "elapsed_seconds": seconds, "presentation_seconds": seconds})
    (directory / "index.json").write_text(json.dumps({"schema": "hellomine3d-pause-notification-capture-v1",
        "locale": "en-US", "normal_input": False, "status": "CAPTURED", "captured_frames": 12, "maximum_frames": 12,
        "status_submit_count": 3, "caption_submit_count": 2, "frames": frames, "events": events}))


def alter_frame(directory, i, mutate):
    path = directory / f"frame-{i:03d}.json"
    frame = json.loads(path.read_text())
    mutate(frame)
    path.write_text(json.dumps(frame))


def dark_glyph(directory, i, all_glyphs=False):
    from pause_capture_pixels import decode_png
    path = directory / f"frame-{i:03d}.png"
    image = decode_png(path.read_bytes())
    rgba = bytearray(image.rgba)
    for y in range(406, 416):
        for x in range(70, 222 if all_glyphs else 77):
            rgba[4 * (y * WIDTH + x):4 * (y * WIDTH + x) + 4] = bytes((25, 45, 52, 255))
    # Bright, unrelated tail evidence must never replace the first glyph.
    for y in range(440, 450):
        for x in range(300, 320):
            rgba[4 * (y * WIDTH + x):4 * (y * WIDTH + x) + 4] = bytes((244, 238, 220, 255))
    path.write_bytes(png(rgba))


def main():
    output = Path(sys.argv[1]).resolve()
    if output.exists():
        raise ValueError("CPU test output must be new")
    output.mkdir(parents=True)
    positive = output / "positive"
    build(positive)
    variants = [
        ("positive", "PASS_NARROW_READABILITY", None),
        ("different-submit-retains-scroll", "BEHAVIOR_FAIL", lambda p: alter_frame(p, 2, lambda f: f["rail"].update(scroll_y=60))),
        ("same-text-submit-retains-scroll", "BEHAVIOR_FAIL", lambda p: alter_frame(p, 5, lambda f: f["rail"].update(scroll_y=60))),
        ("one-glyph-dark-others-correct", "BEHAVIOR_FAIL", lambda p: dark_glyph(p, 2)),
        ("firstline-dark-unrelated-tail-bright", "BEHAVIOR_FAIL", lambda p: dark_glyph(p, 2, True)),
        ("first-hidden-later-settled-correct", "BEHAVIOR_FAIL", lambda p: alter_frame(p, 2, lambda f: f["draw_data"]["lists"][0]["commands"][0].update(clip=[60, 415, 580, 464]))),
        ("wrong-original-glyph-UV", "BEHAVIOR_FAIL", lambda p: change_vertex(p, 2)),
        ("label-B-original-A", "BEHAVIOR_FAIL", lambda p: change_vertex(p, 2, 65 / 1000)),
        ("skipped-first-frame", "FIXTURE_FAIL", lambda p: alter_frame(p, 2, lambda f: f.update(tick=6))),
        ("skipped-settled-frame", "FIXTURE_FAIL", lambda p: alter_frame(p, 3, lambda f: f.update(tick=9))),
        ("wheel-not-delivered", "FIXTURE_FAIL", lambda p: alter_frame(p, 1, lambda f: f["io"].update(mouse_wheel=0))),
        ("wrong-DPI-mapping", "FIXTURE_FAIL", lambda p: alter_frame(p, 2, lambda f: f["draw_data"].update(framebuffer_scale=[2, 2]))),
        ("missing-final-PNG", "FIXTURE_FAIL", lambda p: (p / "frame-002.png").unlink()),
        ("new-caption-resets-scroll", "BEHAVIOR_FAIL", lambda p: alter_frame(p, 7, lambda f: f["rail"].update(scroll_y=0))),
        ("button-shifts", "BEHAVIOR_FAIL", lambda p: alter_frame(p, 2, lambda f: f["buttons"]["pause.save_quit"].update(rect=[200, 191, 440, 221]))),
        ("TTL-expanded", "FIXTURE_FAIL", lambda p: alter_frame(p, 2, lambda f: f["status"].update(remaining_seconds=5))),
        ("source-font-fallback", "FIXTURE_FAIL", lambda p: alter_frame(p, 2, lambda f: f["status_text"]["glyphs"][0].update(actual_codepoint=63))),
        ("font-command-binding-wrong", "FIXTURE_FAIL", lambda p: alter_frame(p, 2, lambda f: f["status_text"].update(font_texture_id="99"))),
        ("unknown-painter-callback", "OPEN", lambda p: alter_frame(p, 2, lambda f: f["draw_data"]["lists"][0]["commands"].append(
            {"element_count": 0, "index_offset": 0, "vertex_offset": 0, "clip": [60, 400, 580, 464], "texture_id": "12", "has_callback": True, "reset_callback": False}))),
        ("missing-presentation-clock", "OPEN", lambda p: alter_frame(p, 2, lambda f: f.pop("presentation_seconds"))),
        ("expired-rail-still-present", "BEHAVIOR_FAIL", lambda p: alter_frame(p, 10, lambda f: f.update(rail={"name": "##PauseNotifications"}))),
        ("fractional-glyph-neighbor-bright", "BEHAVIOR_FAIL", lambda p: fractional_neighbor(p)),
        ("rail-cover-button-rect-stable", "BEHAVIOR_FAIL", lambda p: alter_frame(p, 2, lambda f: f["rail"].update(position=[60, 50], size=[520, 414], inner_clip=[60, 50, 580, 464]))),
        ("actual-sampler-callbacks-known", "PASS_NARROW_READABILITY", lambda p: add_callbacks(p, "nearest", "linear")),
        ("callback-flags-forged", "FIXTURE_FAIL", lambda p: add_callbacks(p, "none", "linear")),
    ]
    results = []
    for name, expected, mutation in variants:
        directory = positive if name == "positive" else output / name
        if mutation:
            shutil.copytree(positive, directory)
            mutation(directory)
        try:
            report = validate_session(directory)
            actual = report["status"]
        except (ValueError, KeyError, TypeError, OSError, OverflowError, struct.error, zlib.error) as error:
            actual, report = "FIXTURE_FAIL", {"status": "FIXTURE_FAIL", "error": str(error)}
        (output / (name + "-report.json")).write_text(json.dumps(report, indent=2) + "\n")
        results.append({"name": name, "expected": expected, "actual": actual, "passed": expected == actual})
    receipt = {"schema": "hellomine3d-pause-notification-oracle-CPU-adversaries-v1",
               "scope": "Synthetic CPU data only, no actual ImGui/native execution or production PASS claim",
               "checks": len(results), "failures": sum(not r["passed"] for r in results), "results": results,
               "sources": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in Path(__file__).parent.glob("*.py")}}
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"checks": receipt["checks"], "failures": receipt["failures"]}))
    return 1 if receipt["failures"] else 0


def change_vertex(directory, i, u=.3):
    path = directory / f"frame-{i:03d}.vbo.bin"
    data = bytearray(path.read_bytes())
    struct.pack_into("<f", data, 8, u)
    path.write_bytes(data)


def fractional_neighbor(directory):
    from pause_capture_pixels import decode_png
    alter_frame(directory, 2, lambda f: f["status_text"]["glyphs"][0].update(bounds=[.2, 0, 1.2, 10]))
    path = directory / "frame-002.vbo.bin"
    data = bytearray(path.read_bytes())
    for j, x in enumerate((70.2, 71.2, 71.2, 70.2)):
        struct.pack_into("<f", data, j * 20, x)
    path.write_bytes(data)
    path = directory / "frame-002.png"
    image = decode_png(path.read_bytes())
    rgba = bytearray(image.rgba)
    for y in range(406, 416):
        rgba[4 * (y * WIDTH + 70):4 * (y * WIDTH + 70) + 4] = bytes((25, 45, 52, 255))
    path.write_bytes(png(rgba))


def add_callbacks(directory, first, second):
    def change(frame):
        commands = frame["draw_data"]["lists"][0]["commands"]
        for role in (second, first):
            commands.insert(0, {"element_count": 0, "index_offset": 0, "vertex_offset": 0,
                "clip": [60, 400, 580, 464], "texture_id": "12", "has_callback": True,
                "reset_callback": False, "callback_role": role})
    alter_frame(directory, 2, change)


if __name__ == "__main__":
    sys.exit(main())
