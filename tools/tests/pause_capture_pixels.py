"""Bounded PNG and original ImDrawData readers; no renderer or font replay."""
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

MAX_FILE = 64 * 1024 * 1024
MAX_TOTAL = 64 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def finite(values):
    return all(isinstance(v, (int, float)) and math.isfinite(v) for v in values)


class Inputs:
    def __init__(self, base):
        self.base = Path(base).resolve()
        self.identities = {}
        self.total = 0

    def read(self, filename, digest=None):
        path = (self.base / filename).resolve()
        require(path.is_relative_to(self.base), "Input escapes capture directory")
        size = path.stat().st_size
        require(0 <= size <= MAX_FILE, "Input exceeds byte budget")
        data = path.read_bytes()
        require(len(data) == size, "Input changed during read")
        sha = hashlib.sha256(data).hexdigest()
        require(digest is None or digest == sha, "Recorded SHA differs: " + str(path))
        record = {"bytes": len(data), "sha256": sha}
        require(str(path) not in self.identities or self.identities[str(path)] == record,
                "Repeated input changed")
        if str(path) not in self.identities:
            self.total += len(data)
            require(self.total <= MAX_TOTAL, "Session exceeds input byte budget")
            self.identities[str(path)] = record
        return data

    def json(self, filename):
        return json.loads(self.read(filename))

    def sidecar(self, record):
        data = self.read(record["file"], record.get("sha256"))
        require(len(data) == record["bytes"], "Sidecar extent differs")
        return data

    def unchanged(self):
        return all(Path(p).stat().st_size == r["bytes"] and
                   hashlib.sha256(Path(p).read_bytes()).hexdigest() == r["sha256"]
                   for p, r in self.identities.items())


class Image:
    def __init__(self, width, height, rgba):
        self.width, self.height, self.rgba = width, height, rgba

    def pixel(self, x, y):
        require(0 <= x < self.width and 0 <= y < self.height, "Pixel out of bounds")
        offset = 4 * (y * self.width + x)
        return tuple(self.rgba[offset:offset + 4])


def decode_png(data):
    """Decode bounded RGB8/RGBA8, verify every chunk CRC, keep top-left pixels."""
    require(data[:8] == b"\x89PNG\r\n\x1a\n", "PNG signature differs")
    offset, header, compressed, ended = 8, None, bytearray(), False
    while offset + 12 <= len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        require(offset + size + 12 <= len(data), "Truncated PNG chunk")
        kind, body = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
        require(zlib.crc32(kind + body) & 0xffffffff ==
                struct.unpack_from(">I", data, offset + 8 + size)[0], "PNG CRC differs")
        if kind == b"IHDR":
            require(header is None and size == 13, "Invalid PNG header")
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            compressed.extend(body)
        elif kind == b"IEND":
            require(size == 0, "Invalid PNG end")
            ended = True
            offset += size + 12
            break
        offset += size + 12
    require(ended and header is not None and offset == len(data), "Incomplete PNG")
    width, height, bits, colour, method, filtering, interlace = header
    require(0 < width <= 8192 and 0 < height <= 8192 and width * height * 4 <= MAX_FILE,
            "PNG dimensions exceed bounds")
    require(bits == 8 and colour in (2, 6) and (method, filtering, interlace) == (0, 0, 0),
            "PNG must be non-interlaced RGB8/RGBA8")
    channels = 4 if colour == 6 else 3
    stride, extent = width * channels, height * (width * channels + 1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed, extent + 1)
    require(len(raw) == extent and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, "PNG decompressed extent differs")
    previous, rgba = bytearray(stride), bytearray()
    for y in range(height):
        filter_id = raw[y * (stride + 1)]
        require(filter_id <= 4, "Invalid PNG filter")
        row = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if filter_id != 0:
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                above = previous[x]
                diagonal = previous[x - channels] if x >= channels else 0
                if filter_id == 4:
                    p = left + above - diagonal
                    distances = (abs(p - left), abs(p - above), abs(p - diagonal))
                    predictor = (left, above, diagonal)[distances.index(min(distances))]
                else:
                    predictor = (0, left, above, (left + above) // 2)[filter_id]
                row[x] = (row[x] + predictor) & 255
        if channels == 4:
            rgba.extend(row)
        else:
            for x in range(width):
                rgba.extend(row[x * channels:x * channels + 3])
                rgba.append(255)
        previous = row
    return Image(width, height, bytes(rgba))


def triangle_contains(points, x, y):
    a, b, c = points
    d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
    if abs(d) < 1e-10:
        return False
    u = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / d
    v = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / d
    return min(u, v, 1 - u - v) >= -1e-6


class DrawData:
    """Read original storage and painter order, respecting IdxOffset/VtxOffset."""
    def __init__(self, record, image, inputs):
        self.image, self.record, self.lists, self.triangles = image, record, [], []
        self.unknown_callback_serials = []
        stride, po, uo, co = (record[k] for k in
                              ("vertex_stride", "pos_offset", "uv_offset", "colour_offset"))
        require(16 <= stride <= 64 and 0 <= po <= stride - 8 and
                0 <= uo <= stride - 8 and 0 <= co <= stride - 4, "Invalid ImDrawVert layout")
        shifts = record["colour_shifts"]
        require(sorted(shifts) == [0, 8, 16, 24], "Invalid colour packing")
        self.display, self.scale = record["display_pos"], record["framebuffer_scale"]
        require(len(self.display) == len(self.scale) == 2 and
                finite(self.display + self.scale + record["display_size"]) and min(self.scale) > 0,
                "Invalid UI display or framebuffer scale")
        expected = [round(record["display_size"][i] * self.scale[i]) for i in range(2)]
        require(expected == [image.width, image.height], "PNG is not the exact rendered viewport")
        require(record["index_type"] in ("uint16", "uint32"), "Unknown ImDrawIdx type")
        code = "H" if record["index_type"] == "uint16" else "I"
        index_size = struct.calcsize(code)
        require(len(record["lists"]) <= 64, "Draw-list budget exceeded")
        unknown_seen = False
        raw_total = 0
        for li, item in enumerate(record["lists"]):
            vb, ib = inputs.sidecar(item["vertices"]), inputs.sidecar(item["indices"])
            raw_total += len(vb) + len(ib)
            require(raw_total <= 4 * 1024 * 1024, "Frame raw UI byte budget exceeded")
            count = item["vertices"]["count"]
            require(0 <= count <= 200000 and len(vb) == count * stride and
                    len(ib) == item["indices"]["count"] * index_size, "Raw UI extent differs")
            indices = struct.unpack("<" + str(len(ib) // index_size) + code, ib)
            vertices = []
            for i in range(count):
                position = struct.unpack_from("<2f", vb, i * stride + po)
                uv = struct.unpack_from("<2f", vb, i * stride + uo)
                colour = struct.unpack_from("<I", vb, i * stride + co)[0]
                require(finite(position + uv), "Non-finite UI vertex")
                vertices.append({"position": position, "uv": uv,
                                 "tint": tuple((colour >> s) & 255 for s in shifts)})
            self.lists.append({"vertices": vertices, "indices": indices})
            require(len(item["commands"]) <= 10000, "Draw command budget exceeded")
            for ci, command in enumerate(item["commands"]):
                if command["callback"] != "none":
                    unknown_seen |= command["callback"] not in ("reset", "nearest", "restore")
                    if command["callback"] == "unknown":
                        self.unknown_callback_serials.append(len(self.triangles))
                    continue
                start, count, vo = (command[k] for k in ("index_offset", "count", "vertex_offset"))
                require(0 <= start <= len(indices) and 0 <= count <= len(indices) - start and
                        count % 3 == 0 and vo >= 0, "Draw command index range differs")
                require(len(command["clip"]) == 4 and finite(command["clip"]), "Invalid command clip")
                clip = [self.point(command["clip"][:2]), self.point(command["clip"][2:])]
                x0, y0, x1, y1 = *clip[0], *clip[1]
                bottom = int(image.height - y1)
                effective = (int(x0), image.height - bottom - int(y1 - y0),
                             int(x0) + int(x1 - x0), image.height - bottom)
                for at in range(start, start + count, 3):
                    ids = tuple(index + vo for index in indices[at:at + 3])
                    require(all(0 <= i < len(vertices) for i in ids), "UI index outside original VBO")
                    points = tuple(self.point(vertices[i]["position"]) for i in ids)
                    self.triangles.append({"list": li, "command": ci, "index": at,
                                           "vertices": ids, "points": points, "clip": effective,
                                           "texture_id": str(command["texture_id"]),
                                           "unknown_callback_before": unknown_seen,
                                           "bounds": (min(p[0] for p in points), min(p[1] for p in points),
                                                      max(p[0] for p in points), max(p[1] for p in points))})
        require(len(self.triangles) <= 100000, "Triangle budget exceeded")

    def point(self, point):
        return tuple((point[i] - self.display[i]) * self.scale[i] for i in range(2))

    def covered_after(self, serial, x, y):
        for tri in self.triangles[serial + 1:]:
            b, c = tri["bounds"], tri["clip"]
            if max(b[0], c[0]) <= x < min(b[2], c[2]) and \
               max(b[1], c[1]) <= y < min(b[3], c[3]) and triangle_contains(tri["points"], x, y):
                return True
        return False

    def marked_quads(self, marker):
        li, vb, ve, ib, ie = (marker[k] for k in
                              ("list", "vertex_begin", "vertex_end", "index_begin", "index_end"))
        require(0 <= li < len(self.lists), "Marker refers to absent actual draw list")
        require(0 <= vb <= ve <= len(self.lists[li]["vertices"]) and
                0 <= ib <= ie <= len(self.lists[li]["indices"]) and
                (ve - vb) % 4 == 0 and (ie - ib) == (ve - vb) // 4 * 6,
                "AddText marker raw range differs")
        tris = [(s, t) for s, t in enumerate(self.triangles)
                if t["list"] == li and ib <= t["index"] < ie]
        require(len(tris) * 3 == ie - ib and all(t["index"] + 3 <= ie and
                all(vb <= i < ve for i in t["vertices"]) for _, t in tris),
                "Marker command/index ownership differs")
        quads = []
        for first in range(vb, ve, 4):
            pair = [(s, t) for s, t in tris if set(t["vertices"]) <= set(range(first, first + 4))]
            require(len(pair) == 2 and {frozenset(t["vertices"]) for _, t in pair} ==
                    {frozenset((first, first + 1, first + 2)),
                     frozenset((first, first + 2, first + 3))}, "AddText quad topology differs")
            quads.append({"vertices": self.lists[li]["vertices"][first:first + 4],
                          "triangles": pair})
        return quads
