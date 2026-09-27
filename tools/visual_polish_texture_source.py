"""Pack the authored V01 ground sheet on one shared world/UI pixel grid."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/art-sources/visual-polish-20260928/ground-sheet-v1.png'
SOURCE_SIZE = (1254, 1254)
AUTHORED_EDGE = 16
NAMES = ('grass_top_a', 'grass_top_b', 'grass_top_c', 'forest_floor')


def tiles(edge=AUTHORED_EDGE):
    """Sample each independent cell before scaling; never mix adjacent art."""
    with Image.open(SOURCE) as image:
        if image.size != SOURCE_SIZE:
            raise ValueError('V01 ground sheet differs from frozen crop bounds')
        source = image.convert('RGBA')
    if source.getchannel('A').getextrema() != (255, 255):
        raise ValueError('V01 ground sheet must be fully opaque')
    cell = SOURCE_SIZE[0] // 2
    result = {}
    for index, name in enumerate(NAMES):
        x, y = index % 2 * cell, index // 2 * cell
        authored = source.crop((x, y, x + cell, y + cell)).resize(
            (AUTHORED_EDGE, AUTHORED_EDGE), Image.Resampling.NEAREST)
        result[name] = authored.resize((edge, edge), Image.Resampling.NEAREST)
    return result
