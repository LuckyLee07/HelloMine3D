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
        ('dirt', None, 'oak_bark_side', 'oak_bark_top'),
    SOURCE.with_name('foliage-sand-sheet-v1.png'):
        ('oak_leaves_a', 'oak_leaves_b', 'sand', 'tall_grass'),
    SOURCE.with_name('birch-spruce-wood-v1.png'):
        ('spruce_bark_side', 'spruce_bark_top', 'birch_bark_side', 'birch_bark_top'),
    SOURCE.with_name('birch-spruce-stone-v1.png'):
        ('spruce_leaves', 'birch_leaves', None, 'moss_stone'),
    SOURCE.with_name('snow-sediment-v1.png'):
        ('snow', 'gravel', 'clay', 'silt'),
}
SINGLE_TILES = {SOURCE.with_name('stone-planes-v2.png'): 'stone'}
SOURCES = (*SHEETS, *SINGLE_TILES)
CUTOUT_KEY_MAX = 12
ADVENTURE_OVERRIDES = ('forest_floor', 'spruce_bark_side', 'spruce_bark_top',
                       'spruce_leaves', 'birch_bark_side', 'birch_bark_top',
                       'birch_leaves', 'moss_stone', 'snow', 'gravel', 'clay', 'silt')
SHARED_BASES = ('grass_top', 'grass_side', 'forest_floor', 'dirt', 'stone',
                'oak_bark_side', 'oak_bark_top', 'sand', 'oak_leaves', 'tall_grass',
                *ADVENTURE_OVERRIDES[1:])


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
            if name is None:
                continue
            x, y = index % 2 * cell, index // 2 * cell
            authored = source.crop((x, y, x + cell, y + cell)).resize(
                (AUTHORED_EDGE, AUTHORED_EDGE), Image.Resampling.NEAREST)
            if 'leaves' in name or name == 'tall_grass':
                authored.putdata([(*pixel[:3], 0 if max(pixel[:3]) <= CUTOUT_KEY_MAX else 255)
                                  for pixel in authored.getdata()])
            result[name] = authored.resize((edge, edge), Image.Resampling.NEAREST)
    for path, name in SINGLE_TILES.items():
        with Image.open(path) as image:
            if image.size != SOURCE_SIZE:
                raise ValueError(f'V01 tile differs from frozen bounds: {path}')
            authored = image.convert('RGBA')
        if authored.getchannel('A').getextrema() != (255, 255):
            raise ValueError(f'V01 opaque tile contains transparency: {path}')
        result[name] = authored.resize((AUTHORED_EDGE, AUTHORED_EDGE),
            Image.Resampling.NEAREST).resize((edge, edge), Image.Resampling.NEAREST)
    return result
