"""Compile authored V01 materials on one shared world/UI pixel grid."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/art-sources/visual-polish-20260928/ground-sheet-v1.png'
SOURCE_SIZE = (1254, 1254)
AUTHORED_EDGE = 16
NAMES = ('grass_top_a', 'grass_top_b', 'grass_top_c', 'forest_floor')
SHEETS = {
    SOURCE: NAMES,
    SOURCE.with_name('earth-wood-sheet-v1.png'):
        ('dirt', 'stone', 'oak_bark_side', 'oak_bark_top'),
    SOURCE.with_name('foliage-sand-sheet-v1.png'):
        ('oak_leaves_a', 'oak_leaves_b', 'sand', 'tall_grass'),
}
CUTOUT_KEY_MAX = 12
SHARED_BASES = ('grass_top', 'grass_side', 'forest_floor', 'dirt', 'stone',
                'oak_bark_side', 'oak_bark_top', 'sand', 'oak_leaves', 'tall_grass')


def tiles(edge=AUTHORED_EDGE):
    """Sample each independent cell before scaling; never mix adjacent art."""
    cell = SOURCE_SIZE[0] // 2
    result = {}
    for path, names in SHEETS.items():
        with Image.open(path) as image:
            if image.size != SOURCE_SIZE:
                raise ValueError(f'V01 sheet differs from frozen crop bounds: {path}')
            source = image.convert('RGBA')
        if source.getchannel('A').getextrema() != (255, 255):
            raise ValueError(f'V01 source sheet must be opaque with RGB cutout keys: {path}')
        for index, name in enumerate(names):
            x, y = index % 2 * cell, index // 2 * cell
            authored = source.crop((x, y, x + cell, y + cell)).resize(
                (AUTHORED_EDGE, AUTHORED_EDGE), Image.Resampling.NEAREST)
            if name.startswith('oak_leaves') or name == 'tall_grass':
                authored.putdata([(*pixel[:3], 0 if max(pixel[:3]) <= CUTOUT_KEY_MAX else 255)
                                  for pixel in authored.getdata()])
            result[name] = authored.resize((edge, edge), Image.Resampling.NEAREST)
    return result
