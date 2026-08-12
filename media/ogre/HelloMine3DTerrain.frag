#version 150

in vec2 terrainTileUv;
in vec2 terrainRepeat;
in float terrainLight;
in float terrainDistance;

out vec4 fragmentColour;

uniform sampler2D terrainAtlas;
uniform float environmentLight;
uniform vec3 fogColour;
uniform float fogDensity;

void main()
{
    const float tilesPerRow = 16.0;
    const float atlasPixels = 256.0;
    const float tilePixels = 16.0;
    vec2 tileIndex = floor(terrainTileUv * tilesPerRow);
    vec2 tilePixel = vec2(0.5) + fract(terrainRepeat) * (tilePixels - 1.0);
    vec2 atlasUv = (tileIndex * tilePixels + tilePixel) / atlasPixels;
    vec4 texel = texture(terrainAtlas, atlasUv);
    if (texel.a == 0.0)
    {
        discard;
    }
    vec3 litColour = texel.rgb * terrainLight * environmentLight;
    float fogVisibility = clamp(
        exp(-terrainDistance * terrainDistance * fogDensity * fogDensity),
        0.0, 1.0);
    fragmentColour = vec4(mix(fogColour, litColour, fogVisibility), texel.a);
}
