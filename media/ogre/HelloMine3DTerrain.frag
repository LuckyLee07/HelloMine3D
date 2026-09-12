#version 150

in vec2 terrainTileUv;
in vec2 terrainRepeat;
in float terrainLight;
in float terrainDistance;
in vec3 terrainWorldPosition;

out vec4 fragmentColour;

#ifdef TERRAIN_ARRAY
uniform sampler2DArray terrainArray;
uniform float alphaCutoff;
#else
uniform sampler2D terrainAtlas;
#endif
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
uniform vec3 sunColour;
uniform float sunIntensity;
uniform float surfaceLightingStrength;
uniform float fogDirectionalStrength;
uniform float fogDensity;
uniform vec3 cameraPosition;

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
    // Evaluate derivatives before alpha discard, including cutout/flora quads.
    vec3 face = cross(dFdx(terrainWorldPosition), dFdy(terrainWorldPosition));
    face /= max(length(face), 0.00001);
    face *= gl_FrontFacing ? 1.0 : -1.0;
    vec2 tileIndex = floor(terrainTileUv * tilesPerRow);
#ifdef TERRAIN_ARRAY
    // Derivatives stay continuous across greedy repeats; each array layer has
    // its own mip chain and wrap addressing, so adjacent tiles never bleed.
    vec2 repeatDx = dFdx(terrainRepeat);
    vec2 repeatDy = dFdy(terrainRepeat);
    float layer = tileIndex.y * tilesPerRow + tileIndex.x;
    vec4 texel = textureGrad(terrainArray,
        vec3(fract(terrainRepeat), layer), repeatDx, repeatDy);
    if (texel.a < alphaCutoff)
#else
    vec2 tilePixel = vec2(0.5) + fract(terrainRepeat) * (tilePixels - 1.0);
    vec2 atlasUv = (tileIndex * tilePixels + tilePixel) / atlasPixels;
    vec4 texel = texture(terrainAtlas, atlasUv);
    if (texel.a == 0.0)
#endif
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
    vec3 litColour = balancedColour * shapedLight * environmentExposure;
    float facingSun = max(dot(face, sunDirection), 0.0);
    float sunlight = clamp(terrainLight, 0.0, 1.0) *
        (0.35 + 0.65 * facingSun);
    vec3 warmLight = mix(vec3(1.0), clamp(sunColour, 0.0, 1.0), 0.35) * 1.06;
    vec3 lightTint = mix(vec3(0.84, 0.93, 1.0), warmLight, sunlight);
    litColour *= mix(vec3(1.0), lightTint,
        clamp(sunIntensity * surfaceLightingStrength, 0.0, 1.0));
    litColour += fogColour * (1.0 - environmentLight) * 0.035;
    float fogVisibility = clamp(
        exp(-terrainDistance * terrainDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        terrainWorldPosition - cameraPosition);
    fragmentColour = vec4(
        mix(localFogColour, litColour, fogVisibility), texel.a);
}
