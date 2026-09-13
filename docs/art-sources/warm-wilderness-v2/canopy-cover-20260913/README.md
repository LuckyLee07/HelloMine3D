# R1 cover-style oak leaf source

`cover-leaves-a.png` and `cover-leaves-b.png` are original RGBA bitmap assets
generated with OpenAI image generation on 2026-09-13 for HelloMine3D. The
user-supplied HelloMine3D cover screenshot informed pixel-cluster size, green
palette and sunlight contrast; no pixels from that screenshot are embedded in
these leaf tiles. A's opaque coverage is about 85.9%; B's is about 87.0%.

The generation brief asked for a square repeating cube-face texture with small
angular pixel leaf clusters, dark recesses, restrained yellow-green highlights
and 10–15% genuinely transparent gaps. B has broader blocks and is used as a
secondary variation; a horizontally mirrored A supplies the third existing
ecology variant. Rejected attempts without actual alpha were not integrated.

The existing texture-array builder downsamples these sources in premultiplied
linear light to 128-pixel masters and the game's 64-pixel layer array. The
legacy atlas is intentionally unchanged for compatibility rendering.
