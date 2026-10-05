"""Independent CPU checks of original resident Fern buffers, without a renderer.

This module accepts explicit raw storage and layout values. The packet adapter
must supply these from the actual observer protocol, not invented facts.
"""
import math
import struct

BIOME_ROWS = (3, 4, 5, 6, 7, 6, 6, 3, 7, 7)


def transform(matrix, point):
    p = (*point, 1.)
    q = [sum(matrix[r*4+c]*p[c] for c in range(4)) for r in range(4)]
    if abs(q[3]-1) > 1e-5:
        raise ValueError('World matrix must be affine')
    return q[:3]


def multiply(left, right):
    return [sum(left[r*4+k]*right[k*4+c] for k in range(4))
            for r in range(4) for c in range(4)]


def column_to_row(matrix):
    return [matrix[c*4+r] for r in range(4) for c in range(4)]


def read_original(vertex_bytes, index_bytes, vertex_start, vertex_count,
                  index_start, index_count, elements):
    """Require the existing packed44B declaration, and preserve all raw bytes."""
    required = [(1, 0, 0, 3), (7, 0, 12, 2), (7, 1, 20, 2),
                (7, 2, 28, 3), (7, 3, 40, 1)]
    if len(vertex_bytes) % 44 or len(index_bytes) % 4:
        raise ValueError('Original buffers are not whole44B/u32 storage')
    if any(type(x) is not int or x < 0 for x in
           [vertex_start, vertex_count, index_start, index_count]):
        raise ValueError('Original operation ranges must be nonnegative integers')
    if not vertex_count or not index_count or index_count % 3:
        raise ValueError('Original operation must contain indexed triangles')
    if (vertex_start+vertex_count)*44 > len(vertex_bytes) or (
            index_start+index_count)*4 > len(index_bytes):
        raise ValueError('Original operation exceeds actual storage')
    if len(elements) != 5:
        raise ValueError('Exactly five original declared attributes required')
    seen = set()
    for element in elements:
        value = (element['semantic'], element['semantic_index'],
                 element['offset'], element['components'])
        if value not in required or value in seen or element['source'] != 0 or (
                element['type'] != element['components']-1 or
                element['bytes'] != element['components']*4):
            raise ValueError('Declaration is not the existing44B production layout')
        seen.add(value)
    vertices = list(struct.iter_unpack('<11f', vertex_bytes))
    if any(not math.isfinite(c) for v in vertices for c in v):
        raise ValueError('Nonfinite original vertex component')
    indices = list(struct.iter_unpack('<I', index_bytes))
    indices = [x[0] for x in indices]
    if any(x >= vertex_count for x in indices[index_start:index_start+index_count]):
        raise ValueError('Original IBO index outside operation vertex range')
    return vertices, indices


def audit_source(vertices, indices, vertex_start, vertex_count, index_start,
                 index_count, matrix, block, biome, sun, local_light,
                 claimed_ranges=None):
    """Recover source ranges from native positions and IBO, then audit them.

    World block id/metadata/availability must be independently supported by the
    caller. No provided range or material fact is taken as expected geometry.
    """
    if len(matrix) != 16 or not all(math.isfinite(x) for x in matrix):
        raise ValueError('World matrix must contain16 finite values')
    if len(block) != 3 or any(type(x) is not int for x in block):
        raise ValueError('World block source must use integral coordinates')
    if type(biome) is not int or not 0 <= biome < len(BIOME_ROWS):
        raise ValueError('Unknown source biome')
    if any(type(x) is not int or not 0 <= x <= 15 for x in [sun, local_light]):
        raise ValueError('Actual World light level must be0..15')
    world_positions = {i: transform(matrix, vertices[i][:3])
                       for i in range(vertex_start, vertex_start+vertex_count)}
    selected = [i for i, p in world_positions.items()
                if all(0 < p[c]-block[c] < 1 for c in range(3))]
    if len(selected) != 24 or selected != list(range(selected[0], selected[0]+24)):
        raise ValueError('Native positions do not contain one contiguous24-vertex Fern')
    selected_set = set(selected)
    selected_index_positions = []
    for i in range(index_start, index_start+index_count, 3):
        triangle = [vertex_start+x for x in indices[i:i+3]]
        inside = sum(x in selected_set for x in triangle)
        if inside not in [0, 3]:
            raise ValueError('Native triangle crosses source block range')
        if inside == 3:
            selected_index_positions.extend(range(i, i+3))
    if len(selected_index_positions) != 36 or selected_index_positions != list(
            range(selected_index_positions[0], selected_index_positions[0]+36)):
        raise ValueError('Native IBO does not contain one contiguous36-index Fern')
    ranges = [selected[0], selected[-1]+1, selected_index_positions[0], selected_index_positions[-1]+1]
    if claimed_ranges is not None and ranges != list(claimed_ranges):
        raise ValueError('Observed source ranges differ from independent native reconstruction')
    local = [[world_positions[i][c]-block[c] for c in range(3)] for i in selected]
    low, high = min(p[1] for p in local), max(p[1] for p in local)
    close = lambda a, b: abs(a-b) <= 2e-5
    roots = [i for i, p in zip(selected, local) if close(p[1], low)]
    if len(roots) != 6 or not close(low, .035) or not close(high, .495):
        raise ValueError('Native six-root scale1 Fern height distribution differs')
    if any(not close(world_positions[i][0]-block[0], .5) or not close(
            world_positions[i][2]-block[2], .5) for i in roots):
        raise ValueError('Native six original roots are not at source block centre')
    # Geometry properties independent of the production six-face generator.
    for face in range(6):
        face_ids = set(selected[face*4:face*4+4])
        triangles = [[vertex_start+x for x in indices[i:i+3]]
                     for i in range(ranges[2]+face*6, ranges[2]+face*6+6, 3)]
        if any(len(set(t)) != 3 or not set(t) <= face_ids for t in triangles):
            raise ValueError('Native face triangle is degenerate or escapes its four vertices')
        if len(set(triangles[0]) & set(triangles[1])) != 2 or set(
                triangles[0]) | set(triangles[1]) != face_ids:
            raise ValueError('Native two triangles do not cover one four-vertex leaf face')
    cells = set()
    for i in selected:
        v = vertices[i]
        cells.add((math.floor(v[3]*16), math.floor(v[4]*16)))
        if not close(v[6]+world_positions[i][1]-block[1], 1):
            raise ValueError('Native repeatV does not match existing Fern physical-height texture mapping')
        if v[5] not in [0., 1.] or not 0 <= v[6] <= 1:
            raise ValueError('Native repeat coordinates outside existing Fern source range')
        if not 0 <= v[7] <= 1 or not close(v[8], sun/15) or not close(v[9]-1, local_light/15) or v[10] != 0:
            raise ValueError('Native copied light/tag differs from independent World observation')
    if len(cells) != 1:
        raise ValueError('Native Fern source crosses tile identity cells')
    tile = next(iter(cells))
    if not 0 <= tile[0] <= 2 or tile[1] != BIOME_ROWS[biome]:
        raise ValueError('Native Fern does not use fixed Grass Top ecology identity')
    return {'ranges': ranges, 'roots': roots, 'tile': list(tile),
            'local_min_y': low, 'local_max_y': high,
            'root_repeat_v': sorted(set(vertices[i][6] for i in roots))}
