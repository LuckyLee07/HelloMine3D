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
    float luminance = dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 balancedColour = mix(vec3(luminance), texel.rgb, 0.62);
    float greenExcess = max(
        balancedColour.g - max(balancedColour.r, balancedColour.b), 0.0);
    balancedColour.g -= greenExcess * 0.22;
    balancedColour.r += greenExcess * 0.07;
    balancedColour = pow(max(balancedColour, vec3(0.0)), vec3(1.05));
    float shapedLight = mix(0.24, 1.0, clamp(terrainLight, 0.0, 1.0));
    float environmentExposure = mix(
        0.34, 1.0, clamp(environmentLight, 0.0, 1.0));
    vec3 litColour = balancedColour * shapedLight * environmentExposure;
    litColour += fogColour * (1.0 - environmentLight) * 0.035;
    float fogVisibility = clamp(
        exp(-terrainDistance * terrainDistance * fogDensity * fogDensity),
        0.0, 1.0);
    fragmentColour = vec4(mix(fogColour, litColour, fogVisibility), texel.a);
}
