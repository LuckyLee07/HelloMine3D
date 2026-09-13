# R1 voxel oak leaf sources

`voxel-oak-a-rgb.png` and `voxel-oak-b-rgb.png` are original square RGB
bitmaps generated for HelloMine3D with OpenAI image generation on 2026-09-13.
The user-provided HelloMine3D cover screenshot guided the leafy green palette,
small square clusters, and sunlight contrast. The requested shape direction
was a full, layered cubic oak canopy. No third-party game asset was used as
input or copied into these images.

These RGB originals use near-black pixels as cutout markers. The deterministic
array builder converts them into hard Alpha before downsampling, at maximum
RGB channel thresholds 16 for A and 12 for B. Both originals and their
derived 128-pixel RGBA masters are retained with SHA-256 in
`../pixel-revision/array-build.json`. The 16 standard oak-leaf semantic layers
use A, B, and a horizontally mirrored A; the compatibility atlas is unchanged.

Earlier cover-style RGBA leaves and the rejected chamfered crown are preserved
separately as failed R1 candidates, not included in this voxel oak array.
