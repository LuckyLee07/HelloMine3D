#version 150
in vec2 particleTileUv;
in vec2 particleUv;
in vec4 particleColour;
in float particleDistance;
out vec4 fragmentColour;
#ifdef TERRAIN_ARRAY
uniform sampler2DArray terrainArray;
#else
uniform sampler2D terrainAtlas;
uniform float atlasPixels;
uniform float tilePixels;
#endif
uniform float tilesPerRow;
uniform float environmentLight;
uniform vec3 fogColour;
uniform float fogDensity;
void main()
{
    vec2 tile = floor(particleTileUv * tilesPerRow);
#ifdef TERRAIN_ARRAY
    vec4 texel = texture(terrainArray,
        vec3(particleUv, tile.y * tilesPerRow + tile.x));
#else
    vec2 pixel = vec2(0.5) + particleUv * (tilePixels - 1.0);
    vec4 texel = texture(terrainAtlas, (tile * tilePixels + pixel) / atlasPixels);
#endif
    if (texel.a < 0.1) discard;
    vec3 colour = texel.rgb * particleColour.rgb * mix(0.3, 1.0, environmentLight);
    float visibility = exp(-particleDistance * particleDistance * fogDensity * fogDensity);
    fragmentColour = vec4(mix(fogColour, colour, visibility), texel.a * particleColour.a);
}
