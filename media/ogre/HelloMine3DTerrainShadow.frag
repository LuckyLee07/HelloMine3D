#version 150

// The legacy branch keeps authored display colours untouched. HDR scene
// shaders decode colour inputs before lighting/blending; alpha/data stay raw.
uniform float linearHdrMode;
vec3 sceneColour(vec3 authored)
{
    if (linearHdrMode < 0.5) return authored;
    vec3 c = max(authored, vec3(0.0));
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), c));
}


in vec2 terrainTileUv;
in vec2 terrainRepeat;
in float terrainLight;
in vec2 terrainLightSources;
in float terrainDistance;
in vec3 terrainWorldPosition;
in vec3 terrainDerivativePosition;
flat in vec3 terrainNaturalTreeRoot;
in vec4 terrainShadowPosition;

out vec4 fragmentColour;

#ifdef TERRAIN_ARRAY
uniform sampler2DArray terrainArray;
uniform float alphaCutoff;
#else
uniform sampler2D terrainAtlas;
#endif
uniform sampler2D directionalShadowMap;
uniform mat4 directionalShadowViewProj;
uniform float atlasPixels;
uniform float tilePixels;
uniform float tilesPerRow;
uniform float colourSaturation;
uniform float greenSuppression;
uniform float greenRedShift;
uniform float toneGamma;
uniform float environmentLight;
uniform float playerExposure;
uniform vec3 fogColour;
uniform vec3 fogSunwardColour;
uniform vec3 sunDirection;
uniform vec3 sunColour;
uniform float sunIntensity;
uniform float surfaceLightingStrength;
uniform float fogDirectionalStrength;
uniform float fogDensity;
uniform vec3 cameraPosition;
uniform vec2 viewRange;
uniform vec2 viewRangeCentre;
uniform float viewRangeStrength;
uniform float directionalShadowEnabled;
uniform float directionalShadowBias;
uniform float directionalShadowStrength;
uniform float directionalShadowFadeStart;
uniform float directionalShadowFadeEnd;

#ifdef TERRAIN_SURFACE
uniform sampler2DArray terrainNormalArray;
uniform sampler2DArray terrainSurfaceArray;
uniform vec4 localLightPositionRadius[8];
uniform vec4 localLightColourEnergy[8];
uniform int localLightCount;

vec3 authoredFromLinear(vec3 c)
{
    c=max(c,vec3(0.0));
    return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-.055,
               step(vec3(.0031308),c));
}

vec3 mappedSurfaceNormal(vec3 normalData, vec3 face, vec3 dx, vec3 dy,
                         vec2 uvDx, vec2 uvDy)
{
    float determinant=uvDx.x*uvDy.y-uvDx.y*uvDy.x;
    if (abs(determinant)<1e-10 || dot(face,face)<.5) return face;
    vec3 u=(dx*uvDy.y-dy*uvDx.y)/determinant;
    vec3 v=(dy*uvDx.x-dx*uvDy.x)/determinant;
    u-=face*dot(face,u);
    if (dot(u,u)<1e-10) return face;
    u=normalize(u);
    v-=face*dot(face,v)+u*dot(u,v);
    if (dot(v,v)<1e-10) return face;
    v=normalize(v);
    vec3 n=normalData*2.0-1.0;
    return normalize(u*n.x+v*n.y+face*n.z);
}

vec3 surfaceSpecular(vec3 albedo, float roughness, float metalness,
                     vec3 n, vec3 view, vec3 light)
{
    float nl=max(dot(n,light),0.0), nv=max(dot(n,view),.001);
    vec3 sum=view+light;
    if (nl<=0.0 || dot(sum,sum)<1e-8) return vec3(0.0);
    vec3 halfDirection=normalize(sum);
    float nh=max(dot(n,halfDirection),0.0);
    float vh=max(dot(view,halfDirection),0.0);
    float a=roughness*roughness, a2=a*a;
    float denominator=nh*nh*(a2-1.0)+1.0;
    float distribution=a2/(3.14159265*denominator*denominator);
    float k=(roughness+1.0)*(roughness+1.0)/8.0;
    float geometry=(nv/(nv*(1.0-k)+k))*(nl/(nl*(1.0-k)+k));
    vec3 f0=mix(vec3(.04),albedo,metalness);
    vec3 fresnel=f0+(1.0-f0)*pow(1.0-vh,5.0);
    return distribution*geometry*fresnel*nl/(4.0*nv*max(nl,.001));
}

vec3 referenceSurfaceLighting(vec3 albedo, vec3 n, vec3 data,
                              float shadowVisibility)
{
    float roughness=clamp(data.r,.2,1.0), metalness=clamp(data.g,0.0,1.0);
    vec3 delta=cameraPosition-terrainWorldPosition;
    vec3 view=dot(delta,delta)>1e-8?normalize(delta):n;
    vec2 sources=terrainLightSources.x>=0.0
        ?clamp(terrainLightSources,0.0,1.0):vec2(clamp(terrainLight,0.0,1.0),0.0);
    float maximum=max(sources.x,sources.y);
    float ao=mix(.24,1.0,clamp(terrainLight/max(maximum,.0001),0.0,1.0));
    // Propagated block light owns local diffuse energy. Nearby indexed sources
    // add bounded specular direction only; they never replace propagation.
    vec3 indirect=sceneColour(vec3(.80,.88,1.0))*(.035+.36*sources.x*environmentLight)
                 +sceneColour(vec3(1.0,.87,.64))*(.68*sources.y);
    if (playerExposure>=0.0) indirect*=clamp(playerExposure,.12,1.0);
    vec3 diffuse=albedo*(1.0-metalness);
    vec3 lit=diffuse*indirect*ao;
    vec3 sunlight=sceneColour(clamp(sunColour,0.0,1.0))*max(sunIntensity,0.0)*1.35;
    float nl=max(dot(n,sunDirection),0.0);
    lit+=(diffuse*(nl/3.14159265)+surfaceSpecular(albedo,roughness,metalness,n,view,
          sunDirection))*sunlight*sources.x*shadowVisibility*ao;
    for (int i=0;i<8;++i)
    {
        if (i>=localLightCount) break;
        vec3 lightDelta=localLightPositionRadius[i].xyz-terrainWorldPosition;
        float distanceSquared=dot(lightDelta,lightDelta);
        float radius=localLightPositionRadius[i].w;
        if (distanceSquared<1e-8 || radius<=0.0) continue;
        float fade=clamp(1.0-distanceSquared/(radius*radius),0.0,1.0);
        float attenuation=fade*fade/(1.0+distanceSquared*.25);
        vec3 light=lightDelta*inversesqrt(distanceSquared);
        lit+=surfaceSpecular(albedo,roughness,metalness,n,view,light)*
             localLightColourEnergy[i].rgb*localLightColourEnergy[i].w*
             attenuation*sources.y*ao;
    }
    // RME blue is normalized authored emission, with fixed bounded radiance.
    return lit+albedo*clamp(data.b,0.0,1.0)*4.0;
}
#endif


// Retire the finite view-distance boundary into the existing atmospheric
// backdrop, without changing lighting or fog within the near field.
float viewRangeCoverage(vec3 worldPosition)
{
    if (viewRange.y <= viewRange.x) return 1.0;
    vec2 position = worldPosition.xz;
    vec2 range = viewRange;
    if (terrainNaturalTreeRoot.z > 0.0)
    {
        position = terrainNaturalTreeRoot.xy;
        // The production tree planner bounds every crown to six metres.
        // Keep every possible six-metre interaction face at full strength,
        // including the extra metre at the outer voxel corner. RD1 retains
        // roots until 14 m; its outer crown is still limited by residency.
        range.y = max(14.0, viewRange.y - 6.0);
        range.x = max(13.0, max(range.y * 0.5, viewRange.x - 6.0));
    }
    vec2 distance = abs(position - viewRangeCentre);
    float edgeDistance = max(distance.x, distance.y);
    float coverage = 1.0 - smoothstep(range.x, range.y, edgeDistance);
    // Underground retains its original geometry and local-light fog.
    return mix(1.0, coverage, clamp(viewRangeStrength, 0.0, 1.0));
}

void applyViewRangeFade(vec3 worldPosition, vec3 backdropColour)
{
    float coverage = viewRangeCoverage(worldPosition);
    if (coverage <= 0.0) discard;
    fragmentColour.rgb = mix(backdropColour, fragmentColour.rgb, coverage);
}

float directionalShadowVisibility()
{
    // Derivatives must be evaluated before any varying early return. The
    // receiving plane supplies the depth at each PCF tap, allowing a small
    // contact bias without bringing back slope acne on rock or actor faces.
    vec3 projected = terrainShadowPosition.xyz /
        max(terrainShadowPosition.w, 0.00001);
    projected.z = projected.z * 0.5 + 0.5;
    // Differentiate world tangents before projection. Subtracting nearby
    // translated shadow coordinates loses significant bits at grazing angles.
    // The directional camera is orthographic; w=0 excludes its translation.
    vec3 dx = (directionalShadowViewProj * vec4(dFdx(terrainDerivativePosition), 0.0)).xyz;
    vec3 dy = (directionalShadowViewProj * vec4(dFdy(terrainDerivativePosition), 0.0)).xyz;
    dx.z *= 0.5;
    dy.z *= 0.5;
    float determinant = dx.x * dy.y - dx.y * dy.x;
    bool validPlane = abs(determinant) > 1e-12;
    vec2 receiverDepthGradient = vec2(0.0);
    if (validPlane)
        receiverDepthGradient = vec2(dx.z * dy.y - dy.z * dx.y,
                                     dy.z * dx.x - dx.z * dy.x) / determinant;
    // Retain the historical bias only for a degenerate projection, where the
    // plane cannot be recovered. Normal surfaces keep ~4-5 cm of tolerance.
    float receiverBias = directionalShadowBias * (validPlane ? 0.10 : 1.0);
    if (directionalShadowEnabled < 0.5 ||
        directionalShadowStrength <= 0.0001 ||
        terrainShadowPosition.w <= 0.00001)
    {
        return 1.0;
    }
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
            vec2 sampleUv = (base + vec2(x, y) + vec2(0.5)) / mapSize;
            float storedDepth = texture(directionalShadowMap, sampleUv).r;
            float receiverDepth = projected.z +
                dot(receiverDepthGradient, sampleUv - projected.xy);
            float visible = receiverDepth - receiverBias <= storedDepth ? 1.0 : 0.0;
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
        return sceneColour(fogColour);
    }
    vec3 normalisedView = viewDirection / directionLength;
    vec2 viewHorizontal = normalisedView.xz;
    vec2 sunHorizontal = sunDirection.xz;
    float viewLength = length(viewHorizontal);
    float sunLength = length(sunHorizontal);
    if (viewLength < 0.00001 || sunLength < 0.00001)
    {
        return sceneColour(fogColour);
    }
    float horizonAmount = 1.0 - smoothstep(
        0.12, 0.65, abs(normalisedView.y));
    float alignment = max(dot(viewHorizontal / viewLength,
                              sunHorizontal / sunLength), 0.0);
    float amount = clamp(fogDirectionalStrength * horizonAmount *
                         alignment * alignment * alignment, 0.0, 1.0);
    return sceneColour(mix(fogColour, fogSunwardColour, amount));
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

// Broad mineral beds and wind-aligned sand use continuous world coordinates.
// Frequency fades with pixel coverage before the pattern can alias in distance.
vec3 geologyPalette(vec3 colour, bool rock, vec3 face, float footprint)
{
    vec2 ground = terrainWorldPosition.xz;
    float broad = groundNoise(ground * 0.018 + vec2(6.7, -12.1));
    if (rock)
    {
        // Irregular broad mineral deposits replace the old four-metre
        // sine bands. Actual voxel steps remain readable without a repeated
        // painted stripe at every few elevation levels.
        float bed = groundNoise(vec2(dot(ground, vec2(0.035, 0.015)),
            terrainWorldPosition.y * 0.065 + broad * 1.6));
        float resolved = 1.0 - smoothstep(0.20, 0.65, footprint * 0.31);
        float band = mix(0.5, bed, resolved);
        float side = 1.0 - abs(face.y);
        band = mix(0.5, band, 0.35 + side * 0.65);
        vec3 mineral = mix(vec3(0.94, 0.96, 0.98),
                           vec3(1.04, 1.015, 0.97), band);
        float grey = dot(colour, vec3(0.2126, 0.7152, 0.0722));
        return mix(colour, vec3(grey), 0.22) * mineral * mix(0.89, 1.03, broad);
    }
    float sweep = groundNoise(ground * 0.065 + vec2(-4.3, 9.7));
    float drift = 0.30 * sin(dot(ground, vec2(0.19, -0.13))) +
                  (sweep - 0.5) * 0.45;
    float phase = dot(ground, vec2(0.72, 0.38)) + drift;
    float resolved = 1.0 - smoothstep(0.10, 0.32, footprint * 0.84);
    float crest = smoothstep(0.30, 0.95, 0.5 + 0.5 * sin(phase * 6.2831853));
    float localStrength = 0.25 + 0.75 * smoothstep(0.28, 0.72, sweep);
    float ripple = mix(1.0, mix(0.965, 1.04, crest),
                       resolved * localStrength * (0.22 + 0.78 * abs(face.y)));
    return colour * mix(vec3(0.95, 0.93, 0.88), vec3(1.03, 1.02, 0.99), broad) * ripple;
}

vec3 naturalPalette(vec3 colour, vec2 tile, vec3 face, float footprint)
{
    if (surfaceLightingStrength < 0.5) return colour;
    bool ecology = tile.y >= 3.0 && tile.y <= 7.0;
    bool grassTop = (tile.y == 0.0 && tile.x == 0.0) ||
        (ecology && tile.x <= 2.0);
    bool grassSide = (tile.y == 0.0 && tile.x == 1.0) ||
        (ecology && tile.x >= 3.0 && tile.x <= 5.0);
    bool leaves = (tile.y == 0.0 && tile.x == 6.0) ||
        (ecology && tile.x >= 6.0 && tile.x <= 8.0) ||
        (tile.y == 8.0 && (tile.x == 2.0 || tile.x == 5.0));
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
    if (tile.y == 0.0 && (tile.x == 3.0 || tile.x == 7.0))
        return geologyPalette(colour, tile.x == 3.0, face, footprint);
    if (stone)
        return mix(colour, vec3(luminance), 0.12) * vec3(1.025, 1.01, 0.975);
    return colour;
}

// Climate arrives in the unused fractional part of uv0; integer tile
// selection stays intact. All vegetation samples
// the same grassland reference row before receiving this continuous palette.
vec3 ecologyPalette(vec3 colour, vec2 tile)
{
    vec2 climate = vec2((fract(terrainTileUv.x * tilesPerRow) - 0.25) * 2.0,
                        (fract(terrainTileUv.y * tilesPerRow) - 0.5) * 4.0);
    climate = clamp(climate, vec2(0.0, -1.0), vec2(1.0));
    // Different atlas rows lose slightly different low bits while carrying
    // climate in UV fractions. Canonicalize that transport noise before colour
    // arithmetic; maximum rounding error stays below the CPU merge tolerance (1e-5).
    climate = floor(climate * 65536.0 + 0.5) / 65536.0;
    vec3 meadow = vec3(1.02, 1.02, 0.95);
    vec3 wet = climate.y < 0.0
        ? mix(meadow, vec3(0.92, 0.99, 1.04), -climate.y)
        : climate.y < 0.5
            ? mix(meadow, vec3(0.96, 1.01, 0.96), climate.y * 2.0)
            : mix(vec3(0.96, 1.01, 0.96), vec3(0.91, 0.96, 0.94), climate.y * 2.0 - 1.0);
    vec3 tint = wet + climate.x * (vec3(1.12, 0.92, 0.77) - meadow);
    // Keep exposed dirt in the side texture neutral, including filtered edges.
    float plant = tile.x >= 3.0 && tile.x <= 5.0
        ? smoothstep(0.01, 0.04, colour.g - colour.r) : 1.0;
    return colour * mix(vec3(1.0), tint / meadow, plant);
}

void main()
{
    // Evaluate derivatives before alpha discard, including cutout/flora quads.
    float shadowVisibility = directionalShadowVisibility();
    vec3 worldDx = dFdx(terrainWorldPosition);
    vec3 worldDy = dFdy(terrainWorldPosition);
    float footprint = max(length(worldDx), length(worldDy));
    vec3 face = cross(worldDx, worldDy);
    face /= max(length(face), 0.00001);
    face *= gl_FrontFacing ? 1.0 : -1.0;
    vec2 tileIndex = floor(terrainTileUv * tilesPerRow);
    bool blendedPlant = surfaceLightingStrength > 0.5 &&
        tileIndex.y >= 3.0 && tileIndex.y <= 7.0 &&
        (tileIndex.x <= 8.0 || (tileIndex.x >= 12.0 && tileIndex.x <= 14.0));
    vec2 sampleTile = blendedPlant ? vec2(tileIndex.x, 4.0) : tileIndex;
#ifdef TERRAIN_ARRAY
    // Derivatives stay continuous across greedy repeats; each array layer has
    // its own mip chain and wrap addressing, so adjacent tiles never bleed.
    vec2 repeatDx = dFdx(terrainRepeat);
    vec2 repeatDy = dFdy(terrainRepeat);
    float layer = sampleTile.y * tilesPerRow + sampleTile.x;
    vec4 texel = textureGrad(terrainArray,
        vec3(fract(terrainRepeat), layer), repeatDx, repeatDy);
    if (texel.a < alphaCutoff)
#else
    vec2 tilePixel = vec2(0.5) + fract(terrainRepeat) * (tilePixels - 1.0);
    vec2 atlasUv = (sampleTile * tilePixels + tilePixel) / atlasPixels;
    vec4 texel = texture(terrainAtlas, atlasUv);
    if (texel.a == 0.0)
#endif
    {
        discard;
    }
#ifdef TERRAIN_SURFACE
    // Colour texture hardware-decodes sRGB exactly once. The established
    // ecological artistic palette is defined in authored colour coordinates.
    vec3 rawLinearAlbedo=texel.rgb;
    texel.rgb=authoredFromLinear(rawLinearAlbedo);
#endif
    if (blendedPlant) texel.rgb = ecologyPalette(texel.rgb, tileIndex);
    float luminance = dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 balancedColour = mix(
        vec3(luminance), texel.rgb, colourSaturation);
    float greenExcess = max(
        balancedColour.g - max(balancedColour.r, balancedColour.b), 0.0);
    balancedColour.g -= greenExcess * greenSuppression;
    balancedColour.r += greenExcess * greenRedShift;
    balancedColour = pow(
        max(balancedColour, vec3(0.0)), vec3(toneGamma));
    balancedColour = naturalPalette(balancedColour, tileIndex, face, footprint);
    balancedColour = sceneColour(balancedColour);
    float shapedLight = mix(0.24, 1.0, clamp(terrainLight, 0.0, 1.0));
    float environmentExposure = mix(
        0.34, 1.0, clamp(environmentLight, 0.0, 1.0));
    // Player exposure already includes sampled local light and daylight.
    if (playerExposure >= 0.0)
        environmentExposure = clamp(playerExposure, 0.0, 1.0);
    vec3 litColour = balancedColour * shapedLight * environmentExposure *
        shadowVisibility;
    float facingSun = max(dot(face, sunDirection), 0.0);
    float sunlight = clamp(terrainLight, 0.0, 1.0) *
        (0.35 + 0.65 * facingSun) * shadowVisibility;
    vec3 warmLight = mix(vec3(1.0), sceneColour(clamp(sunColour, 0.0, 1.0)), 0.35) * 1.06;
    vec3 lightTint = mix(sceneColour(vec3(0.84, 0.93, 1.0)), warmLight, sunlight);
    litColour *= mix(vec3(1.0), lightTint,
        clamp(sunIntensity * surfaceLightingStrength, 0.0, 1.0));
    // World meshes retain both propagated sources. Only sky light follows
    // time of day and directional shadows; a torch is stable in an enclosed room.
    float skyAvailability = 1.0;
    if (terrainLightSources.x >= 0.0 && playerExposure < 0.0) {
        vec2 sources = clamp(terrainLightSources, 0.0, 1.0);
        skyAvailability = sources.x;
        float sky = sources.x * environmentExposure * shadowVisibility;
        float local = sources.y;
        float sourceMaximum = max(sources.x, sources.y);
        // A small, time-independent floor retains unlit silhouettes. Light
        // level propagation, AO and cardinal shading remain CPU authority.
        float exposure = mix(0.34, max(sky, local) / max(sourceMaximum, 0.00001),
            smoothstep(0.0, 0.20, sourceMaximum));
        float localShare = local / max(sky + local, 0.00001);
        vec3 skyTint = mix(vec3(1.0), lightTint,
            clamp(sunIntensity * surfaceLightingStrength, 0.0, 1.0));
        vec3 localTint = mix(vec3(1.0), sceneColour(vec3(1.04, 0.94, 0.80)),
            clamp(surfaceLightingStrength, 0.0, 1.0));
        vec3 sourceTint = sourceMaximum > 0.00001 ?
            mix(skyTint, localTint, localShare) : sceneColour(vec3(0.90, 0.93, 1.0));
        litColour = balancedColour * shapedLight * exposure * sourceTint;
    }
    litColour += sceneColour(fogColour) * (1.0 - environmentLight) * 0.035 * skyAvailability;
    // The existing luminous Waystone core keeps its turquoise in moonlight.
    // Only its cyan inset emits; the masonry frame still receives AO/shadow.
    if (surfaceLightingStrength > 0.5 && tileIndex == vec2(15.0, 0.0)) {
        float core = clamp((texel.b - texel.r) * 2.0, 0.0, 1.0);
        litColour = max(litColour, sceneColour(texel.rgb) * core * 0.74);
    }
#ifdef TERRAIN_SURFACE
    vec3 normalData=textureGrad(terrainNormalArray,
        vec3(fract(terrainRepeat),layer),repeatDx,repeatDy).rgb;
    vec3 materialData=textureGrad(terrainSurfaceArray,
        vec3(fract(terrainRepeat),layer),repeatDx,repeatDy).rgb;
    vec3 surfaceNormal=mappedSurfaceNormal(normalData,face,worldDx,worldDy,repeatDx,repeatDy);
    vec3 surfaceAlbedo=tileIndex.y==9.0?rawLinearAlbedo:balancedColour;
    litColour=referenceSurfaceLighting(surfaceAlbedo,surfaceNormal,materialData,
        clamp(1.0-(1.0-shadowVisibility)/max(directionalShadowStrength,.0001),0.0,1.0));
#endif
    float fogVisibility = clamp(
        exp(-terrainDistance * terrainDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        terrainWorldPosition - cameraPosition);
    localFogColour = mix(sceneColour(vec3(0.035, 0.043, 0.054)), localFogColour, skyAvailability);
    fragmentColour = vec4(
        mix(localFogColour, litColour, fogVisibility), texel.a);
    applyViewRangeFade(terrainWorldPosition,
        directionalFogColour(terrainWorldPosition - cameraPosition));
}
