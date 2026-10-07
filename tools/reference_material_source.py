"""Frozen reference albedos: only deterministic linear-light resolution export."""
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "docs/art-sources/reference-visual-v1"
NAMES = ("pale_stone", "rough_stone", "terracotta", "timber", "metal", "lamp_glow")

def linear(c):
    return np.where(c <= .04045, c / 12.92, ((c + .055) / 1.055) ** 2.4)

def srgb(c):
    return np.where(c <= .0031308, c * 12.92, 1.055 * np.maximum(c, 0) ** (1 / 2.4) - .055)

def tiles(edge):
    result = {}
    for name in NAMES[:4]:
        image = np.asarray(Image.open(ART / (name + "-albedo-source.png")).convert("RGB"),
                           dtype=np.float32) / 255
        values = linear(image)
        values = np.stack([np.asarray(Image.fromarray(values[:,:,i], "F").resize(
            (edge,edge), Image.Resampling.BOX)) for i in range(3)], axis=2)
        rgb = np.floor(np.clip(srgb(values), 0, 1) * 255 + .5).astype(np.uint8)
        result[name] = Image.fromarray(np.concatenate((rgb,np.full((edge,edge,1),255,
                                                               dtype=np.uint8)), axis=2))
    result["metal"] = Image.new("RGBA",(edge,edge),(100,96,85,255))
    result["lamp_glow"] = Image.new("RGBA",(edge,edge),(255,228,157,255))
    return result
