#!/usr/bin/env python3
"""Independent CPU oracle for bounded actual-client material captures.

This tool reads evidence; it never creates GL objects or imports a production
consumer/builder. Pixel probes are subsidiary evidence. Identity coverage is
exactly four fixed materials x six production paths x two rendering modes.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib


MATERIALS = {
    "OakPlank": (35, 22, (5, 1)),
    "Cobblestone": (36, 23, (7, 1)),
    "Chest": (15, 16, (0, 1)),
    "Workbench": (18, 18, (1, 1)),
}
FACE_CONTROL = ("OakBark", 4, 4, (4, 0), (5, 0))
PATHS = ("world", "held_first", "held_third", "drop", "inventory", "map_3d")
MODES = ("standard", "compatibility")
REFERENCE_TILES = {
    **{name: item[2] for name, item in MATERIALS.items()},
    "OakBark-side": (4, 0),
    "OakBark-top": (5, 0),
}
MAX_FILE = 64 * 1024 * 1024
MAX_TOTAL = 512 * 1024 * 1024


class OracleError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise OracleError(message)


def finite(values):
    return all(isinstance(v, (int, float)) and not isinstance(v, bool)
               and math.isfinite(v) for v in values)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class Inputs:
    """Bounded reads and fresh before/after fingerprints, including sidecars."""

    def __init__(self):
        self.identities = {}
        self.total = 0

    def read(self, filename, base=None, maximum=MAX_FILE, digest=None):
        path = Path(filename)
        if not path.is_absolute() and base is not None:
            path = Path(base) / path
        path = path.resolve(strict=True)
        size = path.stat().st_size
        require(0 < size <= maximum, f"Bounded file size: {path} ({size})")
        data = path.read_bytes()
        require(len(data) == size, f"Input changed while reading: {path}")
        actual = sha256(data)
        if digest is not None:
            require(actual == digest, f"Recorded source/sidecar SHA differs: {path}")
        prior = self.identities.get(str(path))
        if prior is not None:
            require(prior["sha256"] == actual, f"Repeated input changed: {path}")
        else:
            self.total += len(data)
            require(self.total <= MAX_TOTAL, "Captured input byte budget exceeded")
            self.identities[str(path)] = {"bytes": len(data), "sha256": actual}
        return data

    def json(self, filename, base=None):
        return json.loads(self.read(filename, base, maximum=8 * 1024 * 1024))

    def unchanged(self):
        return all(Path(p).stat().st_size == d["bytes"] and
                   sha256(Path(p).read_bytes()) == d["sha256"]
                   for p, d in self.identities.items())


class Checks:
    def __init__(self):
        self.items = []

    def add(self, name, passed, detail=None, group="metadata", chain=None):
        item = {"name": name, "passed": bool(passed), "group": group}
        if detail is not None:
            item["detail"] = detail
        if chain is not None:
            item["chain"] = list(chain)
        self.items.append(item)
        return bool(passed)

    @property
    def failures(self):
        return [item for item in self.items if not item["passed"]]


def paeth(left, above, diagonal):
    prediction = left + above - diagonal
    distances = (abs(prediction - left), abs(prediction - above),
                 abs(prediction - diagonal))
    return (left, above, diagonal)[distances.index(min(distances))]


class Image:
    def __init__(self, width, height, rgba, origin="top_left"):
        require(isinstance(width, int) and isinstance(height, int) and
                1 <= width <= 8192 and 1 <= height <= 8192,
                "Invalid image dimensions")
        require(len(rgba) == width * height * 4, "RGBA8 byte extent differs")
        require(origin in ("top_left", "bottom_left", "native_storage"),
                "Image row origin must be explicit")
        self.width, self.height, self.rgba, self.origin = width, height, rgba, origin

    def pixel(self, x, y):
        require(0 <= x < self.width and 0 <= y < self.height,
                "Pixel outside captured image")
        if self.origin == "bottom_left":
            y = self.height - 1 - y
        offset = (y * self.width + x) * 4
        return tuple(self.rgba[offset:offset + 4])

    def tile(self, x, y, edge=16):
        require((x + 1) * edge <= self.width and (y + 1) * edge <= self.height,
                "Independent semantic tile is outside source image")
        return [self.pixel(x * edge + xx, y * edge + yy)
                for yy in range(edge) for xx in range(edge)]


def decode_png(data):
    """Decode the actual default RGBA8/RGB8 PNG without a producer decoder."""
    require(data[:8] == b"\x89PNG\r\n\x1a\n", "PNG signature differs")
    offset, header, compressed, ended = 8, None, bytearray(), False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        require(offset + length + 12 <= len(data), "Truncated PNG chunk")
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        require(zlib.crc32(kind + payload) & 0xffffffff == crc,
                f"PNG chunk CRC differs: {kind!r}")
        if kind == b"IHDR":
            require(header is None and length == 13, "PNG duplicate/invalid IHDR")
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            require(length == 0, "PNG invalid IEND")
            ended = True
            offset += 12
            break
        offset += length + 12
    require(header is not None and ended and offset == len(data),
            "PNG missing header/end or trailing bytes")
    width, height, bits, colour, method, filtering, interlace = header
    require(1 <= width <= 8192 and 1 <= height <= 8192 and
            width * height * 4 <= MAX_FILE, "PNG dimensions exceed byte budget")
    require(bits == 8 and colour in (2, 6) and
            (method, filtering, interlace) == (0, 0, 0),
            "Source must be non-interlaced RGB8/RGBA8 PNG")
    channels = 4 if colour == 6 else 3
    stride = width * channels
    expected = height * (stride + 1)
    stream = zlib.decompressobj()
    raw = stream.decompress(compressed, expected + 1)
    require(len(raw) == expected and stream.eof and not stream.unused_data and
            not stream.unconsumed_tail, "PNG decompressed extent differs")
    previous = bytearray(stride)
    rgba = bytearray()
    for y in range(height):
        kind = raw[y * (stride + 1)]
        require(kind <= 4, "PNG filter outside specification")
        row = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            above = previous[i]
            diagonal = previous[i - channels] if i >= channels else 0
            predictor = (0, left, above, (left + above) // 2,
                         paeth(left, above, diagonal))[kind]
            row[i] = (row[i] + predictor) & 255
        if channels == 4:
            rgba.extend(row)
        else:
            for x in range(width):
                rgba.extend(row[x * 3:x * 3 + 3])
                rgba.append(255)
        previous = row
    return Image(width, height, bytes(rgba))


def srgb_decode(byte):
    value = byte / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def srgb_encode(value):
    encoded = 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055
    return min(255, max(0, int(math.floor(encoded * 255 + 0.5))))


def source_mip(tile, edge):
    """Opaque fixed-cell reference from final 16px atlas, not builder import."""
    require(len(tile) == 256 and all(pixel[3] == 255 for pixel in tile),
            "Selected independent reference cell must be opaque")
    require(edge in (64, 32, 16, 8, 4, 2, 1), "Unexpected default mip edge")
    expected = bytearray()
    for y in range(edge):
        for x in range(edge):
            if edge >= 16:
                value = tile[(y * 16 // edge) * 16 + x * 16 // edge]
            else:
                side = 16 // edge
                pixels = [tile[yy * 16 + xx]
                          for yy in range(y * side, (y + 1) * side)
                          for xx in range(x * side, (x + 1) * side)]
                value = tuple(srgb_encode(sum(srgb_decode(p[c]) for p in pixels) /
                                          len(pixels)) for c in range(3)) + (255,)
            expected.extend(value)
    return bytes(expected)


class Hmt:
    def __init__(self, data):
        require(36 <= len(data) <= 24 * 1024 * 1024, "HMT bounded extent differs")
        magic, version, self.edge, self.layers, self.mips, size, checksum = struct.unpack(
            "<8sIIIIIQ", data[:36])
        require((magic, version, self.edge, self.layers, self.mips) ==
                (b"HMTARRAY", 1, 64, 256, 7), "Default HMT header differs")
        require(size == len(data) - 36 == sum((64 >> n) ** 2 * 256 * 4 for n in range(7)),
                "HMT payload extent or trailing bytes differ")
        value = 14695981039346656037
        for byte in data[36:]:
            value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
        require(value == checksum, "HMT independent FNV64 differs")
        self.data = data

    def layer(self, mip, layer):
        require(0 <= mip < 7 and 0 <= layer < 256, "HMT mip/layer outside bounds")
        offset = 36 + sum((64 >> n) ** 2 * 256 * 4 for n in range(mip))
        size = (64 >> mip) ** 2 * 4
        return self.data[offset + layer * size:offset + (layer + 1) * size]


def validate_sources(atlas, hmt, checks, prefix="RESOURCE"):
    checks.add(prefix + "/default-atlas-dimensions", (atlas.width, atlas.height) == (256, 256),
               group="source_metadata")
    references = {}
    for name, (tx, ty) in REFERENCE_TILES.items():
        tile = atlas.tile(tx, ty)
        checks.add(f"{prefix}/{name}/opaque-source", all(p[3] == 255 for p in tile),
                   group="source_metadata")
        for mip in range(7):
            expected = source_mip(tile, 64 >> mip)
            actual = hmt.layer(mip, ty * 16 + tx)
            delta = max(abs(a - b) for a, b in zip(actual, expected))
            alpha = all(actual[i] == 255 for i in range(3, len(actual), 4))
            checks.add(f"{prefix}/{name}/source-mip-{mip}", delta <= 1 and alpha,
                       {"maximum_byte_delta": delta, "alpha255": alpha}, group="source_mips")
            references[(tx, ty, mip)] = expected
    return references


def barycentric(points, x, y):
    """Screen-space barycentrics; no copied consumer UV/projection formula."""
    a, b, c = points
    determinant = ((b[1] - c[1]) * (a[0] - c[0]) +
                   (c[0] - b[0]) * (a[1] - c[1]))
    if abs(determinant) < 1e-9:
        return None
    u = ((b[1] - c[1]) * (x - c[0]) +
         (c[0] - b[0]) * (y - c[1])) / determinant
    v = ((c[1] - a[1]) * (x - c[0]) +
         (a[0] - c[0]) * (y - c[1])) / determinant
    return u, v, 1.0 - u - v


def interpolate(values, weights, reciprocal_w=None):
    if reciprocal_w is not None:
        denominator = sum(a * b for a, b in zip(weights, reciprocal_w))
        require(abs(denominator) > 1e-12, "Invalid perspective interpolation extent")
        weights = [a * b / denominator for a, b in zip(weights, reciprocal_w)]
    return tuple(sum(weight * value[channel] for weight, value in zip(weights, values))
                 for channel in range(len(values[0])))


def mat_point(matrix, position):
    require(len(matrix) == 16 and finite(matrix) and finite(position),
            "Captured matrix/position must be finite row-major values")
    source = tuple(position) + (1.0,)
    return tuple(sum(matrix[row * 4 + column] * source[column]
                     for column in range(4)) for row in range(4))


def difference(actual, expected):
    require(len(actual) == len(expected), "Compared captured pixel extent differs")
    return max(abs(a - b) for a, b in zip(actual, expected))


def interior_candidates(points, clip, width, height):
    """Bounded candidates from real triangles, avoiding geometric edges."""
    x0 = max(0, math.ceil(max(clip[0], min(p[0] for p in points))))
    y0 = max(0, math.ceil(max(clip[1], min(p[1] for p in points))))
    x1 = min(width, math.floor(min(clip[2], max(p[0] for p in points))))
    y1 = min(height, math.floor(min(clip[3], max(p[1] for p in points))))
    if x1 <= x0 or y1 <= y0:
        return
    # At most 256 sites/triangle; this avoids scanning large full-screen meshes.
    dx, dy = max(1, (x1 - x0) // 16), max(1, (y1 - y0) // 16)
    for y in range(y0, y1, dy):
        for x in range(x0, x1, dx):
            weights = barycentric(points, x + 0.5, y + 0.5)
            if weights is not None and min(weights) >= 0.04:
                yield x, y, weights


def unpack_indices(data, kind):
    require(kind in ("u16", "u32"), "Actual index type must be u16/u32")
    size, code = (2, "H") if kind == "u16" else (4, "I")
    require(len(data) % size == 0 and len(data) <= 16 * 1024 * 1024,
            "Index byte extent/type differs")
    return tuple(value[0] for value in struct.iter_unpack("<" + code, data))


def element_values(data, stride, offset, components, vertex):
    require(1 <= stride <= 256 and 0 <= offset and 1 <= components <= 4 and
            offset + components * 4 <= stride and vertex >= 0,
            "Actual vertex declaration bounds differ")
    begin = vertex * stride + offset
    require(begin + components * 4 <= len(data), "Vertex outside actual buffer")
    values = struct.unpack_from("<" + "f" * components, data, begin)
    require(finite(values), "Actual vertex component is non-finite")
    return values


def gl_enum(value):
    values = {"GL_TEXTURE_2D": 0x0de1, "GL_TEXTURE_2D_ARRAY": 0x8c1a,
              "GL_RGBA8": 0x8058, "GL_RGBA": 0x1908,
              "GL_NEAREST": 0x2600, "GL_LINEAR": 0x2601,
              "GL_NEAREST_MIPMAP_LINEAR": 0x2702,
              "GL_REPEAT": 0x2901, "GL_CLAMP_TO_EDGE": 0x812f}
    return values.get(value, value)


def normalized_mode(value):
    return {"std": "standard", "compat": "compatibility"}.get(value, value)


def facts_value(facts, key, default=None):
    # The C++ facts structure uses camelCase; its JSON writes snake_case.
    camel = key.split("_")[0] + "".join(p.title() for p in key.split("_")[1:])
    return facts.get(key, facts.get(camel, default))


def material_key(facts):
    identifier = facts_value(facts, "material_id")
    for name, (material, _, _) in MATERIALS.items():
        if identifier == material:
            return name
    return "OakBark" if identifier == 4 else None


def independent_tile(name, vertical=False):
    return ((5, 0) if vertical else (4, 0)) if name == "OakBark" else MATERIALS[name][2]


def sidecar(inputs, record, base, multiplier=1):
    data = inputs.read(record["file"], base, digest=record.get("sha256"))
    require(len(data) == record["bytes"] * multiplier, "Sidecar declared byte extent differs")
    return data


class Texture:
    def __init__(self, record, inputs, base):
        self.record = record
        self.identifier = str(record["id"])
        self.target = gl_enum(record["target"])
        self.mips = {}
        require(record["gl_id"] > 0, "Texture has no actual native GL identity")
        for mip in record["mips"]:
            require(mip["format"] == "rgba8" and mip["origin"] == "native_storage",
                    "Texture readback must be native-storage RGBA8")
            level = mip["level"]
            require(level not in self.mips and 0 <= level <= 13, "Duplicate/unbounded texture mip")
            data = sidecar(inputs, mip, base)
            require(len(data) == mip["width"] * mip["height"] * mip["depth"] * 4,
                    "Native texture dimensions differ from bytes")
            self.mips[level] = (mip, data)

    def sample(self, level, layer, x, y):
        info, data = self.mips[level]
        require(0 <= layer < info["depth"] and 0 <= x < info["width"] and
                0 <= y < info["height"], "Native texture sample outside storage")
        offset = ((layer * info["height"] + y) * info["width"] + x) * 4
        return tuple(data[offset:offset + 4])


def validate_texture(texture, atlas, hmt, checks, prefix):
    record = texture.record
    checks.add(prefix + "/actual-state-restored", record.get("state_restored") is True,
               group="capture_integrity")
    base_ok = record["base_level"] == 0 and record["max_level"] >= 0
    checks.add(prefix + "/actual-base-level", base_ok, group="native_upload")
    if texture.target == 0x0de1:
        info, data = texture.mips.get(0, ({}, b""))
        dimensions = (info.get("width"), info.get("height"), info.get("depth"))
        good = dimensions == (256, 256, 1) and data == atlas.rgba
        checks.add(prefix + "/atlas-native-level0", good,
                   {"dimensions": dimensions, "native_bytes": len(data)}, group="native_upload")
    elif texture.target == 0x8c1a:
        for level in range(7):
            info, actual = texture.mips.get(level, ({}, b""))
            edge = 64 >> level
            expected = b"".join(hmt.layer(level, layer) for layer in range(256))
            good = (info.get("width"), info.get("height"), info.get("depth")) == (edge, edge, 256)
            good = good and actual == expected
            checks.add(prefix + f"/array-native-mip-{level}", good,
                       {"native_bytes": len(actual)}, group="native_upload")
    else:
        checks.add(prefix + "/supported-actual-target", False,
                   {"target": texture.target}, group="native_upload")
    for level, (info, _) in texture.mips.items():
        checks.add(prefix + f"/linear-rgba8-storage-{level}",
                   gl_enum(info["internal_format"]) in (0x8058, 0x1908), group="native_upload")


def resource_path(record):
    if isinstance(record, str):
        return record, None
    return record.get("resolved_file", record.get("file")), record.get("sha256")


def read_resources(index, inputs, base, checks):
    resources = index["resources"]
    sources = {source["logical"]: source for source in resources["sources"]}
    logicals = {"profile": "media/materials/Base.terrain-material",
               "atlas": "media/textures/DefaultPack.png",
               "array": "media/textures/WarmWilderness64.hmt",
               "layout": "media/materials/Base.terrain-atlas"}
    paths = {}
    for name, logical in logicals.items():
        source = sources[logical]
        paths[name] = sidecar(inputs, source, base)
        if Path(source["actual"]).exists():
            actual = inputs.read(source["actual"])
            checks.add("RESOURCE/" + name + "/effective-source-copy", actual == paths[name],
                       group="source_metadata")
    profile = resources
    dimensions = tuple(facts_value(profile, k) for k in
                       ("profile_version", "atlas_pixels", "tile_pixels", "tiles_per_row",
                        "array_layer_pixels"))
    checks.add("RESOURCE/frozen-default-profile", dimensions == (2, 256, 16, 16, 64),
               {"observed": dimensions}, group="source_metadata")
    lines = paths["profile"].decode("utf-8").splitlines()
    require(lines[0] == "# HelloMine3D terrain material parameters v2", "Effective default profile header differs")
    entries = {}
    for line in lines[1:]:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, value = line.split("=", 1)
        require(key not in entries, "Duplicate effective profile key")
        entries[key] = value
    checks.add("RESOURCE/effective-default-profile-values",
               all(entries.get(k) == v for k, v in
                   {"atlas_pixels": "256", "tile_pixels": "16", "tiles_per_row": "16",
                    "array_layer_pixels": "64", "atlas_texture": "media/textures/DefaultPack.png",
                    "array_texture": "media/textures/WarmWilderness64.hmt", "leaf_geometry": "cube"}.items()),
               group="source_metadata")
    layout = {}
    for line in paths["layout"].decode("utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        values = line.split("|")
        layout[values[0]] = (int(values[1]), int(values[2]), values[3])
    semantics = {"OakPlank": "oak_planks", "Cobblestone": "cobblestone", "Chest": "chest",
                 "Workbench": "workbench", "OakBark-side": "oak_bark_side", "OakBark-top": "oak_bark_top"}
    for name, tile in REFERENCE_TILES.items():
        checks.add("RESOURCE/" + name + "/fixed-semantic-layout",
                   layout.get(semantics[name]) == tile + ("opaque",), group="source_metadata")
    atlas, hmt = decode_png(paths["atlas"]), Hmt(paths["array"])
    validate_sources(atlas, hmt, checks)
    inputs.read(resources["effective_manifest"], base)
    return atlas, hmt


class Mesh:
    def __init__(self, record, inputs, base):
        self.record, self.buffers, self.elements = record, {}, {}
        for buf in record["vertex_buffers"]:
            require(buf["gl_id"] > 0, "Original vertex buffer is not a real GL buffer")
            data = sidecar(inputs, buf, base)
            require(len(data) == buf["stride"] * buf["vertices"], "Original VBO byte extent differs")
            self.buffers[buf["source"]] = (buf, data)
        for item in record["elements"]:
            semantic = item["semantic"]
            if semantic in (1, 7):
                require(item["type"] == item["components"] - 1 and
                        item["bytes"] == item["components"] * 4,
                        "Captured required vertex element is not FLOATn")
                self.elements[(semantic, item["semantic_index"])] = item
        require(record["index_buffer"]["gl_id"] > 0, "Original IBO is not a real GL buffer")
        self.indices = unpack_indices(sidecar(inputs, record["index_buffer"], base), record["index_type"])
        require(record["index_start"] + record["index_count"] <= len(self.indices),
                "RenderOperation index range outside actual buffer")

    def attribute(self, index, semantic, number=0):
        require(0 <= index < self.record["vertex_count"], "Actual index outside vertex count")
        item = self.elements[(semantic, number)]
        buf, data = self.buffers[item["source"]]
        return element_values(data, buf["stride"], item["offset"], item["components"],
                              self.record["vertex_start"] + index)

    def face(self, first):
        require(first % 6 == 0 and 0 <= first and first + 6 <= self.record["index_count"],
                "Replay is not an original bounded six-index face")
        start = self.record["index_start"] + first
        indices = self.indices[start:start + 6]
        require(len(set(indices)) == 4, "Original replay face is not a four-corner quad")
        return indices


def face_vertical(mesh, indices):
    a, b, c = [mesh.attribute(i, 1) for i in indices[:3]]
    u, v = [b[i] - a[i] for i in range(3)], [c[i] - a[i] for i in range(3)]
    normal = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    require(max(abs(x) for x in normal) > 1e-8, "Original face is degenerate")
    return abs(normal[1]) > max(abs(normal[0]), abs(normal[2])) + 1e-7


def source_sample(atlas, hmt, tile, repeat, array):
    u, v = repeat[0] % 1.0, repeat[1] % 1.0
    if array:
        x, y = min(63, math.floor(u * 64)), min(63, math.floor(v * 64))
        raw = hmt.layer(0, tile[1] * 16 + tile[0])
        offset = (y * 64 + x) * 4
        return tuple(raw[offset:offset + 4])
    x, y = math.floor(0.5 + u * 15), math.floor(0.5 + v * 15)
    return atlas.pixel(tile[0] * 16 + x, tile[1] * 16 + y)


def validate_operation(record, mode, inputs, base, textures, atlas, hmt, checks):
    facts = record["facts"]
    name, path = material_key(facts), facts_value(facts, "consumer")
    if name is None:
        return None
    require(path in ("world", "held_third", "drop"), "Unknown Ogre consumer marker")
    chain = (mode, path, name)
    prefix = "/".join(chain)
    checks.add(prefix + "/actual-state-restored", record.get("state_restored") is True,
               group="capture_integrity", chain=chain)
    block = 4 if name == "OakBark" else MATERIALS[name][1]
    metadata = checks.add(prefix + "/actual-material-block-facts", facts_value(facts, "block_id") == block,
                          group="metadata", chain=chain)
    if path == "drop":
        metadata &= checks.add(prefix + "/actual-item-snapshot", facts_value(facts, "actor_id", 0) > 0 and
                               facts_value(facts, "amount", 0) > 0, group="metadata", chain=chain)
    if path == "held_third":
        metadata &= checks.add(prefix + "/actual-player-slot", facts_value(facts, "slot", -1) >= 0 and
                               facts_value(facts, "amount", 0) > 0, group="metadata", chain=chain)
    if path == "world":
        metadata &= checks.add(prefix + "/actual-resident-revision", facts_value(facts, "revision", 0) > 0,
                               group="metadata", chain=chain)
    mesh = Mesh(record, inputs, base)
    actual_pass = record["pass"]
    expected_name = "HelloMine3D/PlayerHeld" if path == "held_third" else "HelloMine3D/Terrain"
    expected_program = "HelloMine3D/TerrainArrayFragment" if mode == "standard" else "HelloMine3D/TerrainFragment"
    metadata &= checks.add(prefix + "/original-production-pass", record["material_name"] == expected_name and
                           actual_pass["fragment_program"] == expected_program,
                           {"material": record["material_name"], "fragment": actual_pass["fragment_program"]},
                           group="binding", chain=chain)
    metadata &= checks.add(prefix + "/actual-compiled-Ogre-programs",
                           actual_pass["vertex_shader_gl_id"] > 0 and actual_pass["fragment_shader_gl_id"] > 0,
                           group="binding", chain=chain)
    units = actual_pass["texture_units"]
    unit = next((u for u in units if u["unit"] == 0), None)
    require(unit is not None, "Original production pass has no actual texture unit0")
    texture = textures[str(unit["texture_id"])]
    target = 0x8c1a if mode == "standard" else 0x0de1
    metadata &= checks.add(prefix + "/original-TUS-target", texture.target == target,
                           group="binding", chain=chain)
    metadata &= checks.add(prefix + "/original-TUS-point-filter",
                           unit["filter_min"] == 1 and unit["filter_mag"] == 1 and unit["coord_set"] == 0,
                           group="binding", chain=chain)
    replay_passes, pixel_probes = [], 0
    face_kinds = set()
    for number, replay in enumerate(record["replays"]):
        indices = mesh.face(replay["first_index"])
        vertical = face_vertical(mesh, indices)
        face_kinds.add("vertical" if vertical else "side")
        tile = independent_tile(name, vertical)
        actual_tiles = [mesh.attribute(i, 7, 0) for i in set(indices)]
        routed = all(tuple(math.floor(v * 16) for v in uv) == tile for uv in actual_tiles)
        metadata &= checks.add(prefix + f"/face-{number}/fixed-semantic-tile", routed,
                               {"expected_tile": tile, "actual_uv0": actual_tiles}, chain=chain)
        if path == "world":
            origin = tuple(facts_value(facts, "world_" + axis) for axis in "xyz")
            positions = [mat_point(record["world_transform_row_major"], mesh.attribute(i, 1))
                         for i in set(indices)]
            metadata &= checks.add(prefix + f"/face-{number}/actual-placed-block-position",
                                   all(origin[c] - 0.001 <= p[c] <= origin[c] + 1.001
                                       for p in positions for c in range(3)), chain=chain)
        if path != "world":
            metadata &= checks.add(prefix + f"/face-{number}/explicit-root-zero",
                                   all(mesh.attribute(i, 7, 3) == (0.0,) for i in set(indices)), chain=chain)
        uniforms = replay["uniforms"]
        neutral = {"surfaceLightingStrength": 0, "colourSaturation": 1,
                   "greenSuppression": 0, "greenRedShift": 0, "toneGamma": 1,
                   "environmentLight": 1, "playerExposure": 1, "sunIntensity": 0,
                   "fogDensity": 0, "viewRangeStrength": 0}
        good_uniforms = all(uniforms.get(k) == v for k, v in neutral.items())
        same_texture = str(replay["texture_id"]) == texture.identifier
        sampler = replay["sampler"]
        point_sampler = sampler["id"] > 0 and all(gl_enum(sampler[k]) == 0x2600
                                                  for k in ("min_filter", "mag_filter"))
        checks.add(prefix + f"/face-{number}/neutral-original-binding", good_uniforms and same_texture and
                   point_sampler and replay["index_count"] == 6 and replay["program"] > 0,
                   group="ogre_sampling", chain=chain)
        require(replay["format"] == "rgba32f" and replay["origin"] == "bottom_left",
                "Neutral original-buffer replay must be bottom-left RGBA32F")
        data = sidecar(inputs, replay, base)
        width, height = replay["width"], replay["height"]
        require(8 <= width <= 512 and 8 <= height <= 512 and len(data) == width * height * 16,
                "Replay RGBA32F byte extent differs")
        matrix = replay["world_view_projection_row_major"]
        samples, colours = [], set()
        for tri_start in (0, 3):
            tri = indices[tri_start:tri_start + 3]
            positions = [mesh.attribute(i, 1) for i in tri]
            clip = [mat_point(matrix, p) for p in positions]
            require(all(p[3] > 0 for p in clip), "Replay triangle is behind the projection")
            points = [((p[0] / p[3] + 1) * width / 2,
                       (1 - p[1] / p[3]) * height / 2) for p in clip]
            reciprocal = [1 / p[3] for p in clip]
            repeats = [mesh.attribute(i, 7, 1) for i in tri]
            lights = [mesh.attribute(i, 7, 2) for i in tri]
            for x, y, weights in interior_candidates(points, (0, 0, width, height), width, height):
                repeat = interpolate(repeats, weights, reciprocal)
                # Avoid nearest-texel boundary ambiguity in float32 interpolation.
                edge = 64 if mode == "standard" else 15
                coords = [(v % 1) * edge + (0 if mode == "standard" else 0.5) for v in repeat]
                if any(min(v % 1, 1 - v % 1) < 0.04 for v in coords):
                    continue
                light = min(1, max(0, interpolate(lights, weights, reciprocal)[0]))
                texel = source_sample(atlas, hmt, tile, repeat, mode == "standard")
                if texel[:3] in colours:
                    continue
                expected = tuple(c / 255 * (0.24 + 0.76 * light) for c in texel[:3]) + (1.0,)
                offset = ((height - 1 - y) * width + x) * 16
                actual = struct.unpack_from("<4f", data, offset)
                valid_actual = finite(actual)
                delta = difference(actual, expected) if valid_actual else None
                samples.append({"pixel": [x, y], "source_rgba": texel,
                                "actual": [v if math.isfinite(v) else None for v in actual],
                                "expected": expected, "maximum_delta": delta,
                                "passed": valid_actual and delta <= 0.0003})
                colours.add(texel[:3])
                if len(samples) >= 4:
                    break
            if len(samples) >= 4:
                break
        good = good_uniforms and same_texture and point_sampler and len(samples) >= 4 and all(p["passed"] for p in samples)
        checks.add(prefix + f"/face-{number}/actual-original-buffer-pixels", good,
                   {"probes": samples}, group="ogre_sampling", chain=chain)
        replay_passes.append(good)
        pixel_probes += len(samples)
    good_sampling = bool(replay_passes) and all(replay_passes)
    if path in ("held_third", "drop"):
        good_sampling &= len(replay_passes) == 6
        metadata &= checks.add(prefix + "/all-six-original-faces", len(replay_passes) == 6,
                               {"captured_faces": len(replay_passes)}, chain=chain)
    if name == "OakBark":
        metadata &= checks.add(prefix + "/independent-top-side-face-control",
                               face_kinds == {"vertical", "side"}, chain=chain)
    return chain, metadata, good_sampling, pixel_probes, {"face_kinds": sorted(face_kinds)}


class UiData:
    def __init__(self, record, frame, inputs, base, map_record=None, world_sources=None):
        self.record, self.frame, self.triangles = record, frame, []
        self.sampler_records = record["callback_samplers"]
        self.map_record, self.world_sources = map_record, world_sources or {}
        stride, pos_offset, uv_offset, colour_offset = (record[k] for k in
                                                       ("vertex_stride", "pos_offset", "uv_offset", "colour_offset"))
        require(16 <= stride <= 64 and pos_offset + 8 <= stride and uv_offset + 8 <= stride and
                colour_offset + 4 <= stride, "Actual ImDrawVert layout outside bounds")
        shifts = record["colour_shifts"]
        require(sorted(shifts) == [0, 8, 16, 24], "Actual ImU32 channel packing differs")
        display, scale = record["display_pos"], record["framebuffer_scale"]
        require(finite(display + scale + record["display_size"]) and min(scale) > 0,
                "Actual UI display/scale must be finite and positive")
        state = "linear"
        for li, item in enumerate(record["lists"]):
            vertices = sidecar(inputs, item["vertices"], base)
            indices = unpack_indices(sidecar(inputs, item["indices"], base), record["index_type"])
            require(len(vertices) == item["vertices"]["count"] * stride and
                    len(indices) == item["indices"]["count"], "Actual UI byte counts differ")
            parsed = []
            for vertex in range(item["vertices"]["count"]):
                x, y = element_values(vertices, stride, pos_offset, 2, vertex)
                uv = element_values(vertices, stride, uv_offset, 2, vertex)
                colour = struct.unpack_from("<I", vertices, vertex * stride + colour_offset)[0]
                tint = tuple((colour >> shift) & 255 for shift in shifts)
                parsed.append(((x - display[0]) * scale[0], (y - display[1]) * scale[1], uv, tint))
            for ci, command in enumerate(item["commands"]):
                callback = command["callback"]
                if callback != "none":
                    state = "nearest" if callback == "nearest" else "linear" if callback in ("restore", "reset") else "unknown"
                    continue
                start, count, vo = command["index_offset"], command["count"], command["vertex_offset"]
                require(start + count <= len(indices) and count % 3 == 0,
                        "Actual ImDrawCmd index range differs")
                raw_clip = command["clip"]
                require(len(raw_clip) == 4 and finite(raw_clip), "Actual clip rect is invalid")
                x0 = (raw_clip[0] - display[0]) * scale[0]
                y0 = (raw_clip[1] - display[1]) * scale[1]
                x1 = (raw_clip[2] - display[0]) * scale[0]
                y1 = (raw_clip[3] - display[1]) * scale[1]
                # glScissor truncates its integer arguments. The readback is a
                # viewport-sized image, expressed here in top-left pixel space.
                bottom = int(frame.height - y1)
                clip = (int(x0), frame.height - bottom - int(y1 - y0),
                        int(x0) + int(x1 - x0), frame.height - bottom)
                for first in range(start, start + count, 3):
                    actual = tuple(i + vo for i in indices[first:first + 3])
                    require(all(0 <= i < len(parsed) for i in actual), "Actual UI index outside VBO")
                    tri = [parsed[i] for i in actual]
                    points = [p[:2] for p in tri]
                    self.triangles.append({"list": li, "command": ci, "index": first,
                                           "vertices": actual, "points": points, "uv": [p[2] for p in tri],
                                           "tint": [p[3] for p in tri], "clip": clip,
                                           "texture_gl": int(command["texture_id"]), "sampler": state,
                                           "bounds": (min(p[0] for p in points), min(p[1] for p in points),
                                                      max(p[0] for p in points), max(p[1] for p in points))})
        require(len(self.triangles) <= 100000, "Actual UI triangle budget exceeded")

    def occluded(self, serial, x, y):
        for tri in self.triangles[serial + 1:]:
            lo_x, lo_y, hi_x, hi_y = tri["bounds"]
            clip = tri["clip"]
            if not (max(lo_x, clip[0]) <= x <= min(hi_x, clip[2]) and
                    max(lo_y, clip[1]) <= y <= min(hi_y, clip[3])):
                continue
            weights = barycentric(tri["points"], x, y)
            if weights is not None and min(weights) > 1e-5:
                return True
        return False


def validate_ui(ui, marker, mode, textures, atlas, checks):
    facts = marker["facts"]
    name, path = material_key(facts), facts_value(facts, "consumer")
    if name is None:
        return None
    require(path in ("held_first", "inventory", "map_3d"), "Unknown actual UI consumer marker")
    chain = (mode, path, name)
    prefix = "/".join(chain)
    block = 4 if name == "OakBark" else MATERIALS[name][1]
    metadata = checks.add(prefix + "/actual-material-block-facts", facts_value(facts, "block_id") == block,
                          group="metadata", chain=chain)
    if path in ("held_first", "inventory"):
        metadata &= checks.add(prefix + "/actual-player-slot", facts_value(facts, "slot", -1) >= 0 and
                               facts_value(facts, "amount", 0) > 0, group="metadata", chain=chain)
    matches_captured_world = False
    if path == "map_3d":
        query = marker["map_query"]
        good_query = (query is not None and query["known"] and query["sample_available"] and
                      query["block_id"] == block)
        if query is not None:
            good_query = (good_query and query["cell"] == facts_value(facts, "map_cell") and
                          query["height"] == facts_value(facts, "height") and facts_value(facts, "known") and
                          0 < query["query_revision"] <= facts_value(facts, "revision", 0))
        metadata &= checks.add(prefix + "/actual-resident-map-query", good_query,
                               {"source_query": query}, group="map_source", chain=chain)
        batch = ui.map_record["batch"]
        count_x, count_z, step = (batch[k] for k in ("count_x", "count_z", "step"))
        grid_ok = (0 < count_x <= 129 and 0 < count_z <= 129 and count_x % 2 == 1 and
                   count_z % 2 == 1 and step > 0 and batch["size_matched"] and
                   batch["query_count"] == batch["sample_count"] <= 256)
        if query is not None and grid_ok:
            grid_ok = (0 <= query["cell"] < count_x * count_z and query in ui.map_record["samples"] and
                       query["world_x"] == batch["centre_x"] + (query["cell"] % count_x - count_x // 2) * step and
                       query["world_z"] == batch["centre_z"] + (query["cell"] // count_x - count_z // 2) * step)
            matches_captured_world = ((query["world_x"], query["height"], query["world_z"])
                                      in ui.world_sources.get(name, set()))
        metadata &= checks.add(prefix + "/actual-map-source-grid", grid_ok,
                               {"batch": batch, "matches_captured_world": matches_captured_world},
                               group="map_source", chain=chain)
        # A selected observation at a captured fixture column cannot masquerade
        # as an unrelated natural same-material mark by changing its height/id.
        # Other columns remain valid diagnostics, but cannot close this fixture.
        column = []
        if query is not None:
            column = [(world_name, y, 4 if world_name == "OakBark" else MATERIALS[world_name][1])
                      for world_name, positions in ui.world_sources.items()
                      for x, y, z in positions
                      if (x, z) == (query["world_x"], query["world_z"])]
        column_ok = query is not None and (not column or
                    (name, query["height"], query["block_id"]) in column)
        metadata &= checks.add(prefix + "/actual-captured-fixture-column", column_ok,
                               {"captured_column_sources": column, "source_query": query},
                               group="map_source", chain=chain)
    tile = independent_tile(name, path == "map_3d" and bool(facts_value(facts, "top")))
    selected = [(serial, tri) for serial, tri in enumerate(ui.triangles)
                if tri["list"] == marker["list"] and marker["index_begin"] <= tri["index"] and
                tri["index"] + 3 <= marker["index_end"]]
    extent_ok = (marker["vertex_end"] - marker["vertex_begin"] == 4 and
                 marker["index_end"] - marker["index_begin"] == 6 and len(selected) == 2 and
                 all(marker["vertex_begin"] <= index < marker["vertex_end"]
                     for _, tri in selected for index in tri["vertices"]))
    metadata &= checks.add(prefix + "/actual-semantic-draw-range", extent_ok,
                           {"triangle_count": len(selected)}, group="metadata", chain=chain)
    all_uvs = [uv for _, tri in selected for uv in tri["uv"]]
    routed = bool(all_uvs) and all((math.floor(uv[0] * 16), math.floor(uv[1] * 16)) == tile
                                  for uv in all_uvs)
    metadata &= checks.add(prefix + "/fixed-semantic-source-cell", routed,
                           {"expected_tile": tile, "actual_uv": all_uvs}, chain=chain)
    # These four opaque cube consumers submit the complete fixed 16px cell.
    # Checking just floor(UV*16) would accept a cropped/inset pattern inside it.
    # Pixel centres are specified independently from the captured UV values.
    endpoints = [(tile[c] * 16 + offset) / 256 for c in (0, 1)
                 for offset in (0.5, 15.5)]
    corners = [(u, v) for u in endpoints[:2] for v in endpoints[2:]]
    full_cell = bool(all_uvs) and all(any(difference(uv, corner) <= 1e-7 for corner in corners)
                                    for uv in all_uvs)
    full_cell = full_cell and all(any(difference(uv, corner) <= 1e-7 for uv in all_uvs)
                                      for corner in corners)
    metadata &= checks.add(prefix + "/fixed-complete-cell-pixel-centres", full_cell,
                           {"expected_corners": corners}, chain=chain)
    gl_ids = {tri["texture_gl"] for _, tri in selected}
    bound = len(gl_ids) == 1
    actual_texture = next((texture for texture in textures.values()
                           if texture.record["gl_id"] in gl_ids), None)
    bound = bound and actual_texture is not None and actual_texture.target == 0x0de1
    nearest = bool(selected) and all(tri["sampler"] == "nearest" for _, tri in selected)
    actual_sampler = ui.sampler_records["nearest"]
    nearest = nearest and actual_sampler["id"] > 0 and all(gl_enum(actual_sampler[k]) == 0x2600
                                                          for k in ("min_filter", "mag_filter"))
    metadata &= checks.add(prefix + "/actual-UI-atlas-nearest-binding", bound and nearest,
                           {"gl_ids": sorted(gl_ids), "callback_sampler": [t["sampler"] for _, t in selected]},
                           group="binding", chain=chain)
    probes, colours, excluded = [], set(), 0
    for serial, tri in selected:
        for x, y, weights in interior_candidates(tri["points"], tri["clip"], ui.frame.width, ui.frame.height):
            if ui.occluded(serial, x + 0.5, y + 0.5):
                excluded += 1
                continue
            uv, tint = interpolate(tri["uv"], weights), interpolate(tri["tint"], weights)
            px, py = uv[0] * 256, uv[1] * 256
            if min(px % 1, 1 - px % 1, py % 1, 1 - py % 1) < 0.04:
                continue
            tx, ty = math.floor(px), math.floor(py)
            if not (tile[0] * 16 <= tx < (tile[0] + 1) * 16 and
                    tile[1] * 16 <= ty < (tile[1] + 1) * 16):
                continue
            texel = atlas.pixel(tx, ty)
            if texel[:3] in colours:
                continue
            opaque = abs(tint[3] - 255) < 1e-4 and texel[3] == 255
            expected = tuple(texel[c] * tint[c] / 255 for c in range(3)) + (255,)
            actual = ui.frame.pixel(x, y)
            delta = difference(actual, expected)
            probes.append({"pixel": [x, y], "source_texel": [tx, ty], "source_rgba": texel,
                           "actual_tint": tint, "actual": actual, "expected": expected,
                           "maximum_delta": delta, "passed": opaque and delta <= 2.1})
            colours.add(texel[:3])
            if len(probes) >= 4:
                break
        if len(probes) >= 4:
            break
    visible = len(probes) >= 4
    observed_pixels_correct = all(p["passed"] for p in probes)
    sampling = bound and nearest and visible and observed_pixels_correct
    checks.add(prefix + "/actual-backend-frame-pixels", observed_pixels_correct and
               (not visible or (bound and nearest)),
               {"visibility": "VISIBLE" if visible else "INSUFFICIENT_VISIBLE_PIXELS",
                "later_geometry_exclusions": excluded, "probes": probes}, group="ui_sampling", chain=chain)
    # An occluded mark is retained, but cannot close a chain. Another actual
    # visible mark/frame must provide four independent source-colour probes.
    return chain, metadata, sampling, len(probes), {"face_kind": "top" if
            path == "map_3d" and facts_value(facts, "top") else "side",
            "matches_captured_world": matches_captured_world}


def validate_session(filename, inputs, checks):
    filename = filename / "index.json" if filename.is_dir() else filename
    base, index = filename.parent, inputs.json(filename)
    require(index["schema"] == "hellomine3d-material-identity-capture-v1", "Actual observer schema differs")
    checks.add(str(base) + "/terminal-session", index["status"] == "CAPTURED" and
               0 < len(index["frames"]) <= 24 and index["written_bytes"] <= MAX_TOTAL,
               group="capture_integrity")
    atlas, hmt = read_resources(index, inputs, base, checks)
    textures = {}
    for ref in index["textures"]:
        record = inputs.json(ref["file"], base)
        texture = Texture(record, inputs, base)
        require(str(ref["id"]) == texture.identifier and texture.identifier not in textures,
                "Actual texture identity reference differs")
        textures[texture.identifier] = texture
    outcomes, validated, modes, world_sources = [], set(), set(), {}
    for framefile in index["frames"]:
        frame = inputs.json(framefile, base)
        require(frame["schema"] == index["schema"], "Frame schema differs from session")
        mode = normalized_mode(frame["mode"])
        require(mode in MODES, "Captured mode is not standard/compatibility")
        modes.add(mode)
        checks.add(f"{mode}/{frame['phase']}/{frame['frame']}/strict-GL",
                   not frame["gl_errors"], group="capture_integrity")
        checks.add(f"{mode}/{frame['phase']}/{frame['frame']}/actual-state-restored",
                   frame.get("state_restored") is True, group="capture_integrity")
        uses_array = index["resources"]["uses_array"]
        checks.add(mode + "/actual-frozen-rendering-mode", uses_array == (mode == "standard"), group="binding")
        for operation in frame["operations"]:
            for unit in operation["pass"]["texture_units"]:
                if unit["unit"] == 0 and str(unit["texture_id"]) not in validated:
                    texture = textures[str(unit["texture_id"])]
                    validate_texture(texture, atlas, hmt, checks, mode + "/" + texture.identifier)
                    validated.add(texture.identifier)
            # Loaded shader source is retained as a real input identity. Source
            # selection/mode is tested separately from neutral rendered pixels.
            inputs.read(operation["pass"]["vertex_source"], base)
            inputs.read(operation["pass"]["fragment_source"], base)
            result = validate_operation(operation, mode, inputs, base, textures, atlas, hmt, checks)
            if result:
                outcomes.append(result)
                if result[0][1] == "world" and result[1]:
                    facts = operation["facts"]
                    world_sources.setdefault(result[0][2], set()).add(
                        tuple(facts_value(facts, "world_" + axis) for axis in "xyz"))
        capture = frame["framebuffer"]
        require(capture is not None and capture["format"] == "rgba8", "Actual backend framebuffer was not captured")
        image = Image(capture["width"], capture["height"], sidecar(inputs, capture, base), capture["origin"])
        display = frame["ui"]["display_size"]
        scale = frame["ui"]["framebuffer_scale"]
        checks.add(f"{mode}/{frame['phase']}/{frame['frame']}/actual-backend-frame-extent",
                   image.width == int(display[0] * scale[0]) and image.height == int(display[1] * scale[1]),
                   group="ui_sampling")
        checks.add(f"{mode}/{frame['phase']}/{frame['frame']}/UI-linear-framebuffer",
                   not capture["framebuffer_srgb"], group="ui_sampling")
        ui = UiData(frame["ui"], image, inputs, base, frame["map"], world_sources)
        for marker in frame["ui"]["ranges"]:
            result = validate_ui(ui, marker, mode, textures, atlas, checks)
            if result:
                outcomes.append(result)
                ids = {tri["texture_gl"] for tri in ui.triangles if tri["list"] == marker["list"] and
                       marker["index_begin"] <= tri["index"] < marker["index_end"]}
                for texture in textures.values():
                    if texture.record["gl_id"] in ids and texture.identifier not in validated:
                        validate_texture(texture, atlas, hmt, checks, mode + "/" + texture.identifier)
                        validated.add(texture.identifier)
    require(len(modes) == 1, "One frozen capture process cannot change rendering modes")
    return outcomes


def coverage(outcomes, checks):
    records, verified = [], 0
    for mode in MODES:
        for path in PATHS:
            for name in MATERIALS:
                key = mode, path, name
                matches = [item for item in outcomes if item[0] == key]
                metadata = bool(matches) and all(item[1] for item in matches)
                sampling = any(item[2] for item in matches)
                if path == "map_3d":
                    sampling = any(item[1] and item[2] and item[4]["matches_captured_world"]
                                   for item in matches)
                sampling = sampling and not any(not item["passed"] and item.get("chain") == list(key) and
                                                item["group"] in ("ui_sampling", "ogre_sampling")
                                                for item in checks.items)
                passed = metadata and sampling
                verified += passed
                checks.add("CHAIN/" + "/".join(key), passed,
                           {"actual_capture_records": len(matches), "metadata": metadata, "sampling": sampling},
                           group="identity_chain", chain=key)
                records.append({"mode": mode, "consumer": path, "material": name,
                                "metadata": "PASS" if metadata else "OPEN",
                                "sampling": "PASS" if sampling else "OPEN",
                                "status": "PASS" if passed else "OPEN",
                                "pixel_probes": sum(item[3] for item in matches)})
    for mode in MODES:
        controls = [item for item in outcomes if item[0] == (mode, "world", "OakBark")]
        checks.add("CONTROL/" + mode + "/OakBark-original-world-faces",
                   bool(controls) and all(item[1] and item[2] for item in controls), group="face_control")
        controls = [item for item in outcomes if item[0] == (mode, "map_3d", "OakBark")]
        sampled_faces = {item[4]["face_kind"] for item in controls
                         if item[1] and item[2] and item[4]["matches_captured_world"]}
        checks.add("CONTROL/" + mode + "/OakBark-actual-map-query-and-draw",
                   bool(controls) and all(item[1] for item in controls) and sampled_faces == {"top", "side"},
                   {"sampled_face_kinds": sorted(sampled_faces)},
                   group="face_control")
    return records, verified


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", action="append", type=Path, default=[],
                        help="Actual observer frame/manifest JSON; repeat for both modes")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--source-preflight", type=Path, metavar="REPOSITORY",
                        help="Only CPU source reference; explicitly not 48-chain acceptance")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output must be new; preserve old evidence")
    inputs, checks = Inputs(), Checks()
    error, chains, verified = None, [], 0
    try:
        inputs.read(Path(__file__))
        if args.source_preflight is not None:
            require(not args.capture, "Source preflight cannot stand in for captured chains")
            root = args.source_preflight.resolve()
            atlas = decode_png(inputs.read(root / "media/textures/DefaultPack.png"))
            hmt = Hmt(inputs.read(root / "media/textures/WarmWilderness64.hmt"))
            validate_sources(atlas, hmt, checks)
            scope = "CPU_SOURCE_REFERENCE_ONLY_NOT_CAPTURE_ACCEPTANCE"
        else:
            require(bool(args.capture), "Actual-client capture sessions are required for 48-chain acceptance")
            outcomes = []
            for filename in args.capture:
                outcomes.extend(validate_session(filename, inputs, checks))
            chains, verified = coverage(outcomes, checks)
            scope = "ACTUAL_CLIENT_CAPTURE_48_IDENTITY_CHAINS"
        checks.add("INPUTS/unchanged", inputs.unchanged(), group="input_integrity")
        code = 1 if checks.failures else 0
    except (OracleError, OSError, ValueError, KeyError, IndexError, TypeError, struct.error) as exc:
        scope, error, code = "INCOMPLETE_CAPTURE", str(exc), 2
    groups = {}
    for item in checks.items:
        group = groups.setdefault(item["group"], {"checks": 0, "failures": 0})
        group["checks"] += 1
        group["failures"] += not item["passed"]
    source_names = {item["name"] for item in checks.items if item["group"] == "source_mips"}
    source_verified = sum(all(item["passed"] for item in checks.items if item["name"] == name)
                          for name in source_names)
    report = {"schema": "hellomine3d-material-identity-oracle-v1", "scope": scope,
              "status": "PASS" if code == 0 else "FAIL", "exit_code": code,
              "chain_count_expected": 48, "chain_count_verified": verified,
              "full48_closed": verified == 48 and code == 0,
              "chains": chains,
              "source_reference_cases_expected": 42, "source_reference_cases_verified": source_verified,
              "groups": groups,
              "ordinary_input": "NOT_RUN", "continuous_flicker": "NOT_RUN",
              "checks": checks.items, "failures": checks.failures,
              "inputs": inputs.identities, "error": error}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    require(not args.output.exists(), "Output must be new; preserve old evidence")
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print(f"[MATERIAL_IDENTITY_ORACLE] scope={scope} checks={len(checks.items)} "
          f"failures={len(checks.failures)} chains={verified}/48 exit={code}")
    return code


if __name__ == "__main__":
    sys.exit(main())
