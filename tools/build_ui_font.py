#!/usr/bin/env python3
"""Build the OFL UI font at a fixed readable weight for stb_truetype.

The original Noto variable font defaults to Thin (100). stb does not apply
variable-font axes. Preserve the source and license; distribute a named static
500-weight instance instead. Requires fontTools, only at asset-build time.
"""
import hashlib
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont


def build():
    root = Path(__file__).resolve().parents[1]
    source = root / 'media/fonts/NotoSansSC-VF.ttf'
    target = root / 'media/fonts/HelloMineUI-Medium.ttf'
    font = TTFont(source, recalcTimestamp=False)
    instance = instantiateVariableFont(font, {'wght': 500}, inplace=False)
    names = {1: 'HelloMine UI', 2: 'Medium', 3: 'HelloMineUI-Medium-1.0',
             4: 'HelloMine UI Medium', 6: 'HelloMineUI-Medium',
             16: 'HelloMine UI', 17: 'Medium'}
    for record in instance['name'].names:
        if record.nameID in names:
            record.string = names[record.nameID].encode(record.getEncoding())
    instance['OS/2'].usWeightClass = 500
    instance.recalcTimestamp = False
    instance.save(target)
    verified = TTFont(target)
    assert 'fvar' not in verified and verified['OS/2'].usWeightClass == 500
    assert set(verified.getBestCmap()) == set(font.getBestCmap())
    print(target, hashlib.sha256(target.read_bytes()).hexdigest())


if __name__ == '__main__':
    build()
