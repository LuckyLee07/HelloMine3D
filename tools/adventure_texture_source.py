"""Compile the frozen adventure material sheet into existing pixel-art slots.

This is deterministic asset packing, not procedural replacement artwork.
Opaque authored black cells in leaf swatches are the explicit cutout key.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/art-sources/adventure-ecology-20260921/material-sheet-v2.png'
NAMES = ('spruce_bark_side', 'spruce_bark_top', 'spruce_leaves', 'birch_bark_side',
         'birch_bark_top', 'birch_leaves', 'snow', 'gravel',
         'clay', 'forest_floor', 'moss_stone', 'silt')
LEAVES = {'spruce_leaves', 'birch_leaves'}
# The authored rows are not exactly equal. Exclude their transition pixels;
# assuming 362-pixel squares leaks the moss-stone swatch into the snow edge.
SOURCE_SIZE = (1448, 1086)
SOURCE_ROWS = ((0, 361), (362, 713), (714, 1086))


def tiles(edge=32):
    source = Image.open(SOURCE).convert('RGBA')
    if source.size != SOURCE_SIZE:
        raise ValueError('Adventure source dimensions differ from frozen crop bounds')
    size = SOURCE_SIZE[0] // 4
    result = {}
    for index, name in enumerate(NAMES):
        x = index % 4 * size
        top, bottom = SOURCE_ROWS[index // 4]
        # Texel-centre sampling retains coarse authored clusters. No edge
        # blending with adjacent materials is allowed during atlas packing.
        tile = source.crop((x, top, x + size, bottom)).resize(
            (edge, edge), Image.Resampling.NEAREST)
        pixels = []
        for r, g, b, _ in tile.getdata():
            alpha = 0 if name in LEAVES and max(r, g, b) <= 12 else 255
            pixels.append((r, g, b, alpha))
        tile.putdata(pixels)
        if name == 'snow' and min(min(p[:3]) for p in pixels) < 180:
            raise ValueError('Snow contains dark texels from a neighbouring swatch')
        if name in LEAVES:
            coverage = sum(p[3] == 255 for p in pixels) / len(pixels)
            if not .75 <= coverage < 1:
                raise ValueError(f'Invalid authored leaf cutout: {name} {coverage}')
        result[name] = tile
    return result
