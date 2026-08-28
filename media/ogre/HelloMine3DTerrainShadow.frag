#version 150

in vec2 terrainTileUv;
in vec2 terrainRepeat;
in float terrainLight;
in float terrainDistance;
in vec3 terrainWorldPosition;
in vec4 terrainShadowPosition;

out vec4 fragmentColour;

uniform sampler2D terrainAtlas;
uniform sampler2D directionalShadowMap;
uniform float atlasPixels;
uniform float tilePixels;
uniform float tilesPerRow;
uniform float colourSaturation;
uniform float greenSuppression;
uniform float greenRedShift;
uniform float toneGamma;
uniform float environmentLight;
uniform vec3 fogColour;
uniform vec3 fogSunwardColour;
uniform vec3 sunDirection;
uniform float fogDirectionalStrength;
uniform float fogDensity;
uniform vec3 cameraPosition;
uniform float directionalShadowEnabled;
uniform float directionalShadowBias;
uniform float directionalShadowStrength;
uniform float directionalShadowFadeStart;
uniform float directionalShadowFadeEnd;

float directionalShadowVisibility()
{
    if (directionalShadowEnabled < 0.5 ||
        directionalShadowStrength <= 0.0001 ||
        terrainShadowPosition.w <= 0.00001)
    {
        return 1.0;
    }
    vec3 projected = terrainShadowPosition.xyz /
        terrainShadowPosition.w;
    projected.z = projected.z * 0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0 ||
        projected.y <= 0.0 || projected.y >= 1.0 ||
        projected.z <= 0.0 || projected.z >= 1.0)
    {
        return 1.0;
    }

    vec2 texel = 1.0 / vec2(textureSize(directionalShadowMap, 0));
    float litSamples = 0.0;
    for (int y = 0; y <= 1; ++y)
    {
        for (int x = 0; x <= 1; ++x)
        {
            float storedDepth = texture(
                directionalShadowMap,
                projected.xy +
                    (vec2(x, y) - vec2(0.5)) * texel).r;
            litSamples += projected.z - directionalShadowBias <= storedDepth
                ? 1.0 : 0.0;
        }
    }
    float pcfVisibility = litSamples / 4.0;
    float distanceFade = 1.0 - smoothstep(
        directionalShadowFadeStart, directionalShadowFadeEnd,
        terrainDistance);
    return mix(1.0, pcfVisibility,
               directionalShadowStrength * distanceFade);
}

vec3 directionalFogColour(vec3 viewDirection)
{
    float directionLength = length(viewDirection);
    if (directionLength < 0.00001)
    {
        return fogColour;
    }
    vec3 normalisedView = viewDirection / directionLength;
    vec2 viewHorizontal = normalisedView.xz;
    vec2 sunHorizontal = sunDirection.xz;
    float viewLength = length(viewHorizontal);
    float sunLength = length(sunHorizontal);
    if (viewLength < 0.00001 || sunLength < 0.00001)
    {
        return fogColour;
    }
    float horizonAmount = 1.0 - smoothstep(
        0.12, 0.65, abs(normalisedView.y));
    float alignment = max(dot(viewHorizontal / viewLength,
                              sunHorizontal / sunLength), 0.0);
    float amount = clamp(fogDirectionalStrength * horizonAmount *
                         alignment * alignment * alignment, 0.0, 1.0);
    return mix(fogColour, fogSunwardColour, amount);
}

void main()
{
    vec2 tileIndex = floor(terrainTileUv * tilesPerRow);
    vec2 tilePixel = vec2(0.5) + fract(terrainRepeat) * (tilePixels - 1.0);
    vec2 atlasUv = (tileIndex * tilePixels + tilePixel) / atlasPixels;
    vec4 texel = texture(terrainAtlas, atlasUv);
    if (texel.a == 0.0)
    {
        discard;
    }
    float luminance = dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 balancedColour = mix(
        vec3(luminance), texel.rgb, colourSaturation);
    float greenExcess = max(
        balancedColour.g - max(balancedColour.r, balancedColour.b), 0.0);
    balancedColour.g -= greenExcess * greenSuppression;
    balancedColour.r += greenExcess * greenRedShift;
    balancedColour = pow(
        max(balancedColour, vec3(0.0)), vec3(toneGamma));
    float shapedLight = mix(0.24, 1.0, clamp(terrainLight, 0.0, 1.0));
    float environmentExposure = mix(
        0.34, 1.0, clamp(environmentLight, 0.0, 1.0));
    vec3 litColour = balancedColour * shapedLight * environmentExposure *
        directionalShadowVisibility();
    litColour += fogColour * (1.0 - environmentLight) * 0.035;
    float fogVisibility = clamp(
        exp(-terrainDistance * terrainDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        terrainWorldPosition - cameraPosition);
    fragmentColour = vec4(
        mix(localFogColour, litColour, fogVisibility), texel.a);
}
