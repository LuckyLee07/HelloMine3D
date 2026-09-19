#version 150

in vec2 terrainTileUv;
in vec2 terrainRepeat;
in float terrainLight;
in float terrainDistance;
in vec3 terrainWorldPosition;
in vec4 terrainShadowPosition;

out vec4 fragmentColour;

#ifdef TERRAIN_ARRAY
uniform sampler2DArray terrainArray;
uniform float alphaCutoff;
#else
uniform sampler2D terrainAtlas;
#endif
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
uniform vec3 sunColour;
uniform float sunIntensity;
uniform float surfaceLightingStrength;
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

    // A continuous quadratic 3x3 kernel spreads a caster rasterization step
    // across neighbouring texels instead of flashing a whole receiver patch.
    // Compare depths before weighting so filtering cannot erase thin blockers.
    vec2 mapSize = vec2(textureSize(directionalShadowMap, 0));
    vec2 samplePosition = projected.xy * mapSize;
    vec2 base = floor(samplePosition);
    vec2 blend = fract(samplePosition);
    vec2 low = 0.5 * (vec2(1.0) - blend) * (vec2(1.0) - blend);
    vec2 middle = vec2(0.75) - (blend - vec2(0.5)) * (blend - vec2(0.5));
    vec2 high = 0.5 * blend * blend;
    vec3 weightX = vec3(low.x, middle.x, high.x);
    vec3 weightY = vec3(low.y, middle.y, high.y);
    float pcfVisibility = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 weight = vec2(weightX[x + 1], weightY[y + 1]);
            float storedDepth = texture(directionalShadowMap,
                (base + vec2(x, y) + vec2(0.5)) / mapSize).r;
            float visible = projected.z - directionalShadowBias <= storedDepth ? 1.0 : 0.0;
            pcfVisibility += visible * weight.x * weight.y;
        }
    }
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

// Low-frequency world-space colour breaks up the tiled ground without
// changing geometry, block identity, light propagation or saved worlds.
float groundNoise(vec2 position)
{
    vec2 cell = floor(position);
    vec2 f = fract(position);
    f = f * f * (3.0 - 2.0 * f);
    vec4 corners = vec4(dot(cell, vec2(127.1, 311.7)),
        dot(cell + vec2(1.0, 0.0), vec2(127.1, 311.7)),
        dot(cell + vec2(0.0, 1.0), vec2(127.1, 311.7)),
        dot(cell + vec2(1.0, 1.0), vec2(127.1, 311.7)));
    vec4 values = fract(sin(corners) * 43758.5453);
    return mix(mix(values.x, values.y, f.x), mix(values.z, values.w, f.x), f.y);
}

vec3 naturalPalette(vec3 colour, vec2 tile)
{
    if (surfaceLightingStrength < 0.5) return colour;
    bool ecology = tile.y >= 3.0 && tile.y <= 7.0;
    bool grassTop = (tile.y == 0.0 && tile.x == 0.0) ||
        (ecology && tile.x <= 2.0);
    bool grassSide = (tile.y == 0.0 && tile.x == 1.0) ||
        (ecology && tile.x >= 3.0 && tile.x <= 5.0);
    bool leaves = (tile.y == 0.0 && tile.x == 6.0) ||
        (ecology && tile.x >= 6.0 && tile.x <= 8.0);
    bool tallGrass = (tile.y == 0.0 && tile.x == 11.0) ||
        (ecology && tile.x >= 12.0 && tile.x <= 14.0);
    bool flower = tile.y == 0.0 && tile.x == 10.0;
    bool greenFringe = grassSide && colour.g > colour.r * 1.03;
    bool flowerStem = flower && colour.g > colour.r;
    float luminance = dot(colour, vec3(0.2126, 0.7152, 0.0722));
    if (grassTop || greenFringe || leaves || tallGrass || flowerStem)
    {
        // Ground and foliage share the same broad field across chunk edges.
        // Preserve biome hues and cutout alpha; only compress fine contrast.
        float large = groundNoise(terrainWorldPosition.xz * 0.022);
        float local = groundNoise(terrainWorldPosition.xz * 0.085 + vec2(17.3, -9.1));
        float patch = large * 0.72 + local * 0.28;
        float quiet = mix(luminance, leaves ? 0.37 : 0.43, leaves ? 0.18 : 0.22);
        vec3 sage = quiet * vec3(0.89, 1.06, 0.78);
        float blend = leaves ? 0.32 : (tallGrass || flowerStem ? 0.48 : 0.40);
        return mix(colour, sage, blend) * mix(vec3(0.85, 0.93, 0.91),
                                             vec3(1.04, 1.01, 0.89), patch);
    }
    if (flower)
    {
        // Petals remain a local accent instead of a saturated red beacon.
        return mix(colour, luminance * vec3(1.70, 0.65, 0.58), 0.28);
    }
    bool earth = grassSide || (tile.y == 0.0 && tile.x == 2.0);
    bool wood = (tile.y == 0.0 && (tile.x == 4.0 || tile.x == 5.0)) ||
        (tile.y == 1.0 && tile.x == 5.0);
    bool stone = (tile.y == 0.0 && tile.x == 3.0) ||
        (tile.y == 1.0 && tile.x == 7.0);
    if (earth || wood)
    {
        vec3 quiet = mix(colour, vec3(luminance), 0.10);
        return mix(quiet, vec3(0.36, 0.29, 0.21), earth ? 0.07 : 0.04);
    }
    if (stone)
        return mix(colour, vec3(luminance), 0.12) * vec3(1.025, 1.01, 0.975);
    return colour;
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
    balancedColour = naturalPalette(balancedColour, tileIndex);
    float shapedLight = mix(0.24, 1.0, clamp(terrainLight, 0.0, 1.0));
    float environmentExposure = mix(
        0.34, 1.0, clamp(environmentLight, 0.0, 1.0));
    float shadowVisibility = directionalShadowVisibility();
    vec3 litColour = balancedColour * shapedLight * environmentExposure *
        shadowVisibility;
    float facingSun = max(dot(face, sunDirection), 0.0);
    float sunlight = clamp(terrainLight, 0.0, 1.0) *
        (0.35 + 0.65 * facingSun) * shadowVisibility;
    vec3 warmLight = mix(vec3(1.0), clamp(sunColour, 0.0, 1.0), 0.35) * 1.06;
    vec3 lightTint = mix(vec3(0.84, 0.93, 1.0), warmLight, sunlight);
    litColour *= mix(vec3(1.0), lightTint,
        clamp(sunIntensity * surfaceLightingStrength, 0.0, 1.0));
    litColour += fogColour * (1.0 - environmentLight) * 0.035;
    // The existing luminous Waystone core keeps its turquoise in moonlight.
    // Only its cyan inset emits; the masonry frame still receives AO/shadow.
    if (surfaceLightingStrength > 0.5 && tileIndex == vec2(15.0, 0.0)) {
        float core = clamp((texel.b - texel.r) * 2.0, 0.0, 1.0);
        litColour = max(litColour, texel.rgb * core * 0.74);
    }
    float fogVisibility = clamp(
        exp(-terrainDistance * terrainDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        terrainWorldPosition - cameraPosition);
    fragmentColour = vec4(
        mix(localFogColour, litColour, fogVisibility), texel.a);
}
