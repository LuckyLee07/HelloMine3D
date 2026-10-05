#!/usr/bin/env python3
"""Check captured production pause notifications; never launch or render a client."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import zlib

from pause_capture_pixels import DrawData, Inputs, decode_png, finite, require, triangle_contains

PHASES = ("old_A_top", "old_A_scroll", "B_new_first", "B_new_settled",
          "B_manual_scroll", "B_repeat_same_text", "B_repeat_manual_scroll",
          "caption_submit", "caption_refresh", "caption_expiry", "status_expiry", "buttons_idle")
BUTTONS = {"pause.resume", "pause.settings", "pause.save_main", "pause.save_quit"}
VISIBLE_PHASES = {"old_A_top", "B_new_first", "B_new_settled", "B_repeat_same_text"}
SCROLLED_PHASES = {"old_A_scroll", "B_manual_scroll", "B_repeat_manual_scroll"}
SCOPE_OPEN = [
    "ordinary native input and usability (normal_input=false)",
    "ordinary button hover/press/disabled behavior beyond observed geometry/focus/action facts",
    "independent font atlas raster/alpha sampling, exact glyph pixel shape and OCR",
    "unobserved settings/backpack/machine/map feedback and full V10 coverage",
]


class Checks:
    def __init__(self):
        self.items = []
        self.open = []

    def add(self, category, name, passed, detail=None):
        self.items.append({"category": category, "name": name, "passed": bool(passed), "detail": detail})
        return bool(passed)

    def unresolved(self, name, detail):
        self.open.append({"name": name, "detail": detail})


def canonical_draw(raw):
    match = re.fullmatch(r"IM_COL32_RGBA_shifts_(\d+)_(\d+)_(\d+)_(\d+)", raw["colour_packing"])
    require(match is not None, "Unknown recorded colour packing")
    require(raw["index_bytes"] in (2, 4), "Unknown original ImDrawIdx width")
    lists = []
    def callback_role(command):
        role = command.get("callback_role")
        if role is None:
            return "none" if not command["has_callback"] else "reset" if command["reset_callback"] else "unknown"
        require(role in ("none", "reset", "nearest", "linear", "unknown") and
                command["has_callback"] == (role != "none") and command["reset_callback"] == (role == "reset"),
                "Actual callback identity flags differ")
        return "restore" if role == "linear" else role
    for item in raw["lists"]:
        lists.append({
            "owner": item["owner"],
            "vertices": {"file": item["vertex_file"], "count": item["vertex_count"],
                         "bytes": item["vertex_count"] * raw["vertex_stride"]},
            "indices": {"file": item["index_file"], "count": item["index_count"],
                        "bytes": item["index_count"] * raw["index_bytes"]},
            "commands": [{"index_offset": c["index_offset"], "vertex_offset": c["vertex_offset"],
                          "count": c["element_count"], "clip": c["clip"], "texture_id": c["texture_id"],
                          "callback": callback_role(c)} for c in item["commands"]]})
    return {"display_pos": raw["display_position"], "display_size": raw["display_size"],
            "framebuffer_scale": raw["framebuffer_scale"], "vertex_stride": raw["vertex_stride"],
            "pos_offset": raw["position_offset"], "uv_offset": raw["uv_offset"],
            "colour_offset": raw["colour_offset"], "colour_shifts": list(map(int, match.groups())),
            "index_type": "uint16" if raw["index_bytes"] == 2 else "uint32", "lists": lists}


def close(actual, expected, tolerance):
    return len(actual) == len(expected) and all(abs(a - b) <= tolerance for a, b in zip(actual, expected))


def overlaps(a, b):
    return min(a[2], b[2]) - max(a[0], b[0]) > .01 and min(a[3], b[3]) - max(a[1], b[1]) > .01


def glyph_evidence(text_record, draw, checks, prefix):
    """Derive short explicit first-line glyph quads from actual font source facts.

    This is layout/source identity plus final foreground-presence evidence. It
    does not reconstruct font texture alpha or claim an exact pixel oracle.
    """
    first = text_record["text"].split("\n", 1)[0]
    require(first == text_record["first_explicit_line"] and 1 <= len(first) <= 128,
            "First-line identity/length differs")
    glyphs = text_record["glyphs"]
    require([g["codepoint"] for g in glyphs] == [ord(c) for c in first], "Glyph source sequence differs")
    font, baked, wrap = (text_record[k] for k in ("font_size", "baked_size", "wrap"))
    require(finite([font, baked, wrap] + text_record["origin"]) and min(font, baked, wrap) > 0,
            "Invalid actual font/position facts")
    scale = font / baked
    require(sum(g["advance"] * scale for g in glyphs) <= wrap, "Explicit first line unexpectedly wraps")
    owners = [i for i, item in enumerate(draw.record["lists"])
              if item["owner"] == text_record["draw_list_owner"]]
    require(len(owners) == 1, "Status original draw-list ownership is ambiguous")
    marker = {k: text_record[k] for k in ("vertex_begin", "vertex_end", "index_begin", "index_end")}
    marker["list"] = owners[0]
    quads = draw.marked_quads(marker)
    x, y = map(math.trunc, text_record["origin"])
    expected, evidence = [], []
    for glyph in glyphs:
        require(finite([glyph["advance"]] + glyph["bounds"] + glyph["uv"]) and
                len(glyph["bounds"]) == len(glyph["uv"]) == 4, "Invalid glyph source facts")
        checks.add("fixture", f"{prefix}/glyph-{glyph['codepoint']}-no-font-fallback",
                   glyph.get("actual_codepoint") == glyph["codepoint"])
        if glyph["visible"]:
            x0, y0, x1, y1 = glyph["bounds"]
            u0, v0, u1, v1 = glyph["uv"]
            require(x0 < x1 and y0 < y1 and 0 <= u0 < u1 <= 1 and 0 <= v0 < v1 <= 1,
                    "Invalid visible glyph bounds/UV")
            points = [(x + x0 * scale, y + y0 * scale), (x + x1 * scale, y + y0 * scale),
                      (x + x1 * scale, y + y1 * scale), (x + x0 * scale, y + y1 * scale)]
            expected.append((glyph, points, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]))
        x += glyph["advance"] * scale
    require(len(expected) >= 4, "Fixture needs at least four visible first-line glyphs")
    used = set()
    for gi, (glyph, points, uv) in enumerate(expected):
        matches = [i for i, q in enumerate(quads)
                   if all(close(v["position"], p, .08) and close(v["uv"], t, 2e-6)
                          for v, p, t in zip(q["vertices"], points, uv))]
        matched = len(matches) == 1 and matches[0] not in used
        checks.add("behavior", f"{prefix}/glyph-{gi}-original-source-quad", matched,
                   {"codepoint": glyph["codepoint"], "expected_screen_quad": points, "matches": matches})
        item = {"codepoint": glyph["codepoint"], "geometry_matched": matched,
                "candidate_pixels": 0, "foreground_pixels": 0, "covered_pixels": 0,
                "sampled_pixels_all": 0, "foreground_pixels_all": 0}
        evidence.append(item)
        if not matched:
            continue
        qi = matches[0]
        used.add(qi)
        quad = quads[qi]
        if glyph["coloured"]:
            checks.unresolved(prefix + "/coloured-font-glyph", glyph["codepoint"])
            continue
        triangles = quad["triangles"]
        colours = {v["tint"] for v in quad["vertices"]}
        texture = text_record.get("font_texture_id")
        colour = text_record.get("colour")
        colour_shifts = draw.record["colour_shifts"]
        expected_tint = tuple((colour >> shift) & 255 for shift in colour_shifts) if isinstance(colour, int) else None
        source_bound = (len(colours) == 1 and expected_tint in colours and texture is not None and
                        int(texture) > 0 and all(t["texture_id"] == str(texture) for _, t in triangles))
        checks.add("fixture", f"{prefix}/glyph-{gi}-actual-colour-font-binding", source_bound)
        if not source_bound:
            continue
        # Require the complete source glyph rectangle inside both real clip
        # rectangles. Partial top clipping is unreadable even if a tail stroke
        # or another correct glyph contributes bright pixels.
        pixel_points = [draw.point(p) for p in points]
        box = (min(p[0] for p in pixel_points), min(p[1] for p in pixel_points),
               max(p[0] for p in pixel_points), max(p[1] for p in pixel_points))
        clips = [t["clip"] for _, t in triangles]
        fully_inside = all(c[0] - .1 <= box[0] and c[1] - .1 <= box[1] and
                           box[2] <= c[2] + .1 and box[3] <= c[3] + .1 for c in clips)
        fully_inside &= 0 <= box[0] <= box[2] <= draw.image.width and 0 <= box[1] <= box[3] <= draw.image.height
        checks.add("behavior", f"{prefix}/glyph-{gi}-complete-effective-clip", fully_inside, {"box": box, "clips": clips})
        for py in range(max(0, math.floor(box[1])), min(draw.image.height, math.ceil(box[3]))):
            for px in range(max(0, math.floor(box[0])), min(draw.image.width, math.ceil(box[2]))):
                centre = (px + .5, py + .5)
                candidates = [(s, t) for s, t in triangles if
                              t["clip"][0] <= centre[0] < t["clip"][2] and
                              t["clip"][1] <= centre[1] < t["clip"][3] and
                              triangle_contains(t["points"], *centre)]
                if not candidates:
                    continue
                rgba = draw.image.pixel(px, py)
                foreground = min(rgba[:3]) >= 150 and max(abs(rgba[i] - expected_tint[i]) for i in range(3)) <= 65
                item["sampled_pixels_all"] += 1
                item["foreground_pixels_all"] += int(foreground)
                serial = max(s for s, _ in triangles)
                if draw.covered_after(serial, *centre):
                    item["covered_pixels"] += 1
                    continue
                item["candidate_pixels"] += 1
                # Only foreground close to the actual near-white WarmText tint
                # qualifies. A dark card/another tail line cannot close this
                # exact glyph region. No glyph-centre assumption (O/V holes).
                if foreground:
                    item["foreground_pixels"] += 1
        if item["sampled_pixels_all"] >= 4:
            required_all = max(2, min(8, math.ceil(item["sampled_pixels_all"] * .02)))
            checks.add("behavior", f"{prefix}/glyph-{gi}-final-PNG-has-foreground",
                       item["foreground_pixels_all"] >= required_all, dict(item, required_foreground_pixels=required_all))
        unknown = any(t["unknown_callback_before"] for _, t in triangles) or any(
            serial >= min(s for s, _ in triangles) for serial in draw.unknown_callback_serials)
        if unknown or item["candidate_pixels"] < 4:
            checks.unresolved(f"{prefix}/glyph-{gi}-painter-visibility", item)
        else:
            required = max(2, min(8, math.ceil(item["candidate_pixels"] * .02)))
            checks.add("behavior", f"{prefix}/glyph-{gi}-final-PNG-foreground", item["foreground_pixels"] >= required,
                       dict(item, required_foreground_pixels=required))
    return evidence


def validate_session(directory):
    inputs, checks = Inputs(directory), Checks()
    index = inputs.json("index.json")
    require(index["schema"] == "hellomine3d-pause-notification-capture-v1", "Capture schema differs")
    require(index["status"] == "CAPTURED" and index["normal_input"] is False and
            index["captured_frames"] == index["maximum_frames"] == len(PHASES) and
            len(index["frames"]) == len(PHASES), "Capture is incomplete or ordinary-input identity differs")
    require(index["locale"] in ("zh-CN", "en-US"), "Unexpected locale")
    events = index["events"]
    require(len(events) == 8, "Fixture event count differs")
    statuses = [e for e in events if e["type"] == "status_submit"]
    captions = [e for e in events if e["type"] == "caption_submit"]
    wheels = [e for e in events if e["type"] == "wheel"]
    require(len(statuses) == index["status_submit_count"] == 3 and
            len(captions) == index["caption_submit_count"] == 2 and len(wheels) == 3,
            "Status/caption/wheel event lineage differs")
    require([e["phase"] for e in statuses] == [PHASES[i] for i in (0, 2, 5)] and
            [e["phase"] for e in captions] == [PHASES[i] for i in (7, 8)] and
            [e["phase"] for e in wheels] == [PHASES[i] for i in (1, 4, 6)] and
            all(e["wheel_relative"] == -120 for e in wheels), "Wrong production event phases")
    a, b = statuses[0]["text"], statuses[1]["text"]
    require(a.startswith("A_START_OLD_STATUS\n") and b.startswith("B_START_NEW_STATUS\n") and
            a != b and statuses[2]["text"] == b and len(a.splitlines()) == len(b.splitlines()) == 16,
            "Long-A/new-B/same-B source events differ")
    require(all(events[i]["tick"] < events[i + 1]["tick"] for i in range(len(events) - 1)),
            "Events are out of order")
    frames, observations = [], []
    for i, filename in enumerate(index["frames"]):
        frame = inputs.json(filename)
        require(frame["schema"] == "hellomine3d-pause-notification-frame-v1" and
                frame["phase"] == PHASES[i] and frame["frame"] == i and
                frame["locale"] == index["locale"] and frame["normal_input"] is False,
                "Actual phase/frame/locale identity differs")
        require(frame["image_row_origin"] == "top_left" and frame["png"] == f"frame-{i:03d}.png",
                "Original PNG mapping is not explicit")
        png_bytes = inputs.read(frame["png"])
        require(len(png_bytes) <= 8 * 1024 * 1024, "Frame PNG budget exceeded")
        image = decode_png(png_bytes)
        draw = DrawData(canonical_draw(frame["draw_data"]), image, inputs)
        checks.add("fixture", PHASES[i] + "/paused-simulation", frame["simulation_allowed"] is False)
        checks.add("behavior", PHASES[i] + "/no-unrequested-action", frame["pending_action"] == 0)
        require(isinstance(frame["actual_native_focused"], bool) and
                isinstance(frame["io"]["app_focus_lost"], bool), "Missing actual focus observations")
        require(finite([frame["actual_delta_seconds"], frame["elapsed_seconds"]]) and
                frame["actual_delta_seconds"] >= 0 and 0 <= frame["elapsed_seconds"] < 45,
                "Actual clock/deadline facts differ")
        require(set(frame["buttons"]) == BUTTONS, "Original pause footer/resume button observation missing")
        for name, item in frame["buttons"].items():
            rect = item["rect"]
            require(len(rect) == 4 and finite(rect), "Invalid original button rectangle")
            require(isinstance(item["window_focused"], bool) and isinstance(item["hovered"], bool),
                    "Missing actual button focus observations")
            size = frame["draw_data"]["display_size"]
            checks.add("behavior", PHASES[i] + "/button-visible/" + name,
                       0 <= rect[0] < rect[2] <= size[0] and 0 <= rect[1] < rect[3] <= size[1])
        expected_submits = 1 if i < 2 else 2 if i < 5 else 3
        expected_captions = 0 if i < 7 else 1 if i == 7 else 2
        require(frame["status"]["submit_count"] == expected_submits and
                frame["caption"]["submit_count"] == expected_captions,
                "Frame does not correspond to actual source submission")
        require(finite([frame["status"]["remaining_seconds"], frame["caption"]["remaining_seconds"]]) and
                0 <= frame["status"]["remaining_seconds"] <= 4 and
                0 <= frame["caption"]["remaining_seconds"] <= 2.5, "Actual normal timer outside bounds")
        if i < 10:
            rail, text = frame["rail"], frame["status_text"]
            require(rail is not None and text is not None and rail["name"] == "##PauseNotifications" and
                    rail["draw_list_owner"] == text["draw_list_owner"] == rail["name"],
                    "Not the original production pause notification rail")
            require(frame["status"]["text"] == text["text"] == (a if i < 2 else b) and
                    frame["status"]["remaining_seconds"] > 0 and rail["scroll_max_y"] > 0,
                    "Long source status/actual overflow is not established")
            require(finite([rail["scroll_y"], rail["scroll_max_y"]]) and
                    0 <= rail["scroll_y"] <= rail["scroll_max_y"] + .1, "Invalid actual scroll")
            require(finite(rail["position"] + rail["size"] + rail["inner_clip"]) and
                    len(rail["position"]) == len(rail["size"]) == 2 and len(rail["inner_clip"]) == 4,
                    "Invalid actual rail window/clip")
            outer = rail["position"] + [rail["position"][j] + rail["size"][j] for j in range(2)]
            for button, item in frame["buttons"].items():
                checks.add("behavior", PHASES[i] + "/notification-does-not-cover-button/" + button,
                           not overlaps(outer, item["rect"]) and not overlaps(rail["inner_clip"], item["rect"]))
            if i in (2, 5):
                event = statuses[1 if i == 2 else 2]
                checks.add("fixture", PHASES[i] + "/actual-first-submission-frame",
                           frame["tick"] == event["tick"] == frame["phase_enter_tick"] == frame["last_status_submit_tick"])
            if PHASES[i] in VISIBLE_PHASES:
                checks.add("behavior", PHASES[i] + "/new-status-scroll-top", abs(rail["scroll_y"]) <= .1)
                observations.append({"phase": PHASES[i], "glyphs": glyph_evidence(text, draw, checks, PHASES[i])})
            if PHASES[i] in SCROLLED_PHASES:
                require(frame["io"]["mouse_wheel"] < 0 and rail["hovered"] and rail["scroll_y"] > 1,
                        "Actual original wheel routing/scroll is not established")
                checks.add("fixture", PHASES[i] + "/header-actually-scrolled-out",
                           math.trunc(text["origin"][1]) + text["font_size"] < rail["inner_clip"][1])
        else:
            checks.add("behavior", PHASES[i] + "/expired-status-caption-and-rail",
                       frame["status"]["remaining_seconds"] == 0 and
                       frame["caption"]["remaining_seconds"] == 0 and frame["rail"] is None and frame["status_text"] is None)
        frames.append(frame)
    checks.add("fixture", "new-B-next-actual-frame", frames[3]["tick"] == frames[2]["tick"] + 1)
    ids = {f["rail"]["id"] for f in frames[:10]}
    checks.add("fixture", "same-live-notification-window-id", len(ids) == 1 and next(iter(ids)) > 0)
    for i in range(1, len(frames)):
        checks.add("fixture", PHASES[i] + "/actual-monotonic-frame-clock",
                   frames[i]["tick"] > frames[i - 1]["tick"] and
                   frames[i]["elapsed_seconds"] >= frames[i - 1]["elapsed_seconds"])
        for button in BUTTONS:
            checks.add("behavior", PHASES[i] + "/stable-original-button/" + button,
                       close(frames[i]["buttons"][button]["rect"], frames[0]["buttons"][button]["rect"], .1) and
                       frames[i]["buttons"][button]["window_id"] == frames[0]["buttons"][button]["window_id"])
    for i in (7, 8, 9):
        checks.add("behavior", PHASES[i] + "/caption-does-not-reset-status-scroll",
                   abs(frames[i]["rail"]["scroll_y"] - frames[6]["rail"]["scroll_y"]) <= .1)
        checks.add("behavior", PHASES[i] + "/caption-keeps-actual-status-source",
                   frames[i]["status_text"]["text"] == b)
    checks.add("fixture", "actual-caption-submit-refresh-expiry",
               frames[7]["caption"]["cue"] == frames[8]["caption"]["cue"] == "diagnostic.pause.scroll" and
               frames[7]["caption"]["remaining_seconds"] > 0 and frames[8]["caption"]["remaining_seconds"] > 0 and
               frames[9]["caption"]["remaining_seconds"] == 0)
    if not all("presentation_seconds" in f for f in frames) or not all("presentation_seconds" in e for e in events):
        checks.unresolved("normal-TTL-causal-clock", "Actual remaining/expiry observed; presentation event clock not captured")
    else:
        for i in range(1, len(frames)):
            previous, frame = frames[i - 1], frames[i]
            good = finite([previous["presentation_seconds"], frame["presentation_seconds"]]) and \
                   frame["presentation_seconds"] >= previous["presentation_seconds"]
            if frame["tick"] == previous["tick"] + 1:
                good &= abs(frame["presentation_seconds"] - previous["presentation_seconds"] - frame["actual_delta_seconds"]) <= .002
            checks.add("fixture", frame["phase"] + "/actual-presentation-clock", good)
        for frame in frames:
            status_event = statuses[0 if frame["frame"] < 2 else 1 if frame["frame"] < 5 else 2]
            elapsed = frame["presentation_seconds"] - status_event["presentation_seconds"]
            checks.add("fixture", frame["phase"] + "/normal-status-four-second-clock",
                       elapsed >= 0 and abs(frame["status"]["remaining_seconds"] - max(0, 4 - elapsed)) <= .035)
        for i in (7, 8, 9, 10, 11):
            frame, event = frames[i], captions[0 if i == 7 else 1]
            elapsed = frame["presentation_seconds"] - event["presentation_seconds"]
            checks.add("fixture", frame["phase"] + "/normal-caption-two-and-half-second-clock",
                       elapsed >= 0 and abs(frame["caption"]["remaining_seconds"] - max(0, 2.5 - elapsed)) <= .035)
    checks.add("fixture", "all-captured-inputs-unchanged", inputs.unchanged())
    failed = [c for c in checks.items if not c["passed"]]
    fixture_failed = [c for c in failed if c["category"] == "fixture"]
    behavior_failed = [c for c in failed if c["category"] == "behavior"]
    status = "FIXTURE_FAIL" if fixture_failed else "BEHAVIOR_FAIL" if behavior_failed else "OPEN" if checks.open else "PASS_NARROW_READABILITY"
    report = {"schema": "hellomine3d-pause-notification-oracle-v1", "status": status,
              "normal_input": False, "captured_frames": len(frames), "locale": index["locale"],
              "checks": checks.items, "fixture_failures": len(fixture_failed), "behavior_failures": len(behavior_failed),
              "open": checks.open, "scope_open": SCOPE_OPEN, "glyph_observations": observations,
              "inputs": inputs.identities, "inputs_unchanged": not any(c["name"] == "all-captured-inputs-unchanged" and not c["passed"] for c in checks.items)}
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="Completed original production capture directory")
    parser.add_argument("--output", type=Path, required=True, help="New JSON report path")
    args = parser.parse_args(argv)
    require(not args.output.exists(), "Report path already exists")
    try:
        report = validate_session(args.capture)
    except (ValueError, KeyError, TypeError, OSError, OverflowError, struct.error, zlib.error) as error:
        report = {"schema": "hellomine3d-pause-notification-oracle-v1", "status": "FIXTURE_FAIL",
                  "normal_input": False, "error": str(error), "scope_open": SCOPE_OPEN}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    report["oracle_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(report["status"])
    return {"PASS_NARROW_READABILITY": 0, "BEHAVIOR_FAIL": 1, "FIXTURE_FAIL": 2, "OPEN": 3}[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
