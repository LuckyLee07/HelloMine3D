#version 150

in vec2 terrainTileUv;
in vec2 terrainRepeat;
in float terrainLight;

out vec4 fragmentColour;

uniform sampler2D terrainAtlas;

void main()
{
    const float tilesPerRow = 16.0;
    const float atlasPixels = 256.0;
    const float tilePixels = 16.0;
    vec2 tileIndex = floor(terrainTileUv * tilesPerRow);
    vec2 tilePixel = vec2(0.5) + fract(terrainRepeat) * (tilePixels - 1.0);
    vec2 atlasUv = (tileIndex * tilePixels + tilePixel) / atlasPixels;
    fragmentColour = texture(terrainAtlas, atlasUv) * terrainLight;
    if (fragmentColour.a == 0.0)
    {
        discard;
    }
}
