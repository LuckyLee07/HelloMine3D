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


in vec3 waterWorldPosition;
in vec3 waterWorldNormal;
in float waterLight;
in vec2 waterLightSources;
in float waterDistance;
in vec2 waterSurfaceData;
in vec2 waterSurfaceDrift;

out vec4 fragmentColour;

uniform float environmentLight;
uniform vec3 fogColour;
uniform vec3 fogSunwardColour;
uniform float fogDirectionalStrength;
uniform float fogDensity;
uniform vec3 skyZenithColour;
uniform vec3 skyHorizonColour;
uniform vec3 sunDirection;
uniform vec3 sunColour;
uniform float sunIntensity;
uniform vec3 waterShallowColour;
uniform vec3 waterDeepColour;
uniform vec3 cameraPosition;
uniform vec2 viewRange;
uniform vec2 viewRangeCentre;
uniform float viewRangeStrength;
uniform float globalTime;
uniform float waterDetailStrength;

// The RTT already stores lit linear radiance. Never decode or tone-map it here.
uniform sampler2D planarReflectionTexture;
uniform mat4 planarReflectionViewProj;
uniform float planarReflectionEnabled;
uniform float planarReflectionPlaneY;
uniform vec2 planarReflectionTexelSize;


// Only the final view-distance band loses coverage. Keep near-field lighting
// and atmospheric fog unchanged; the sky is visible through retired pixels.
float viewRangeCoverage(vec3 worldPosition)
{
    if (viewRange.y <= viewRange.x) return 1.0;
    vec2 distance = abs(worldPosition.xz - viewRangeCentre);
    float edgeDistance = max(distance.x, distance.y);
    float coverage = 1.0 - smoothstep(viewRange.x, viewRange.y, edgeDistance);
    // Underground retains its original geometry and local-light fog.
    return mix(1.0, coverage, clamp(viewRangeStrength, 0.0, 1.0));
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

float surfaceStreak(vec2 position)
{
    return sin(position.x * 3.1 + sin(position.y * 1.7)) *
           sin(position.y * 4.3 - position.x * 0.8);
}

// Integrate the positive sin^12 lobe over a pixel's actual phase footprint.
// World metres alone do not bound shore*22: a one-metre shoreline can project
// narrow bright peaks into subpixels even while world detail remains visible.
// These periodic primitives have mean removed; their magnitudes stay bounded
// across negative phases, many cycles, and long-running animation. A fixed
// maximum of four primitive evaluations preserves the lobe's average energy
// (0.11279296875) without textures, history, or additional render targets.
vec2 shoreRipplePrimitives(float phase)
{
    const float pi = 3.14159265358979323846;
    const float mean = 0.11279296875;
    float x = mod(phase, 2.0 * pi);
    if (x >= pi)
    {
        float q = x - pi;
        return vec2(mean * (0.5 * pi - q),
                    0.5 * mean * q * (pi - q));
    }
    // sin^12 on its positive half-cycle is a finite cosine polynomial.
    const float coefficients[6] = float[6](
        -0.38671875, 0.24169921875, -0.107421875,
         0.0322265625, -0.005859375, 0.00048828125);
    float c = cos(2.0 * x);
    float cosinePrevious = 1.0;
    float cosineCurrent = c;
    float sinePrevious = 0.0;
    float sineCurrent = sin(2.0 * x);
    vec2 value = vec2(mean * (x - 0.5 * pi),
                     0.5 * mean * x * (x - pi));
    for (int i = 0; i < 6; ++i)
    {
        float harmonic = 2.0 * float(i + 1);
        value.x += coefficients[i] * sineCurrent / harmonic;
        value.y += coefficients[i] * (1.0 - cosineCurrent) /
                   (harmonic * harmonic);
        float cosineNext = 2.0 * c * cosineCurrent - cosinePrevious;
        float sineNext = 2.0 * c * sineCurrent - sinePrevious;
        cosinePrevious = cosineCurrent;
        cosineCurrent = cosineNext;
        sinePrevious = sineCurrent;
        sineCurrent = sineNext;
    }
    return value;
}

float shoreRippleLineAverage(float phase, float width)
{
    return 0.11279296875 +
        (shoreRipplePrimitives(phase + 0.5 * width).x -
         shoreRipplePrimitives(phase - 0.5 * width).x) / width;
}

float filteredShoreRipple(float phase)
{
    float raw = pow(max(sin(phase), 0.0), 12.0);
    vec2 phasePixel = abs(vec2(dFdx(phase), dFdy(phase)));
    float footprint = phasePixel.x + phasePixel.y;
    if (footprint <= 0.05) return raw;
    // Wrap only after taking derivatives; wrapping before them invents a
    // discontinuity at every 2pi boundary. Both footprint axes are symmetric.
    const float pi = 3.14159265358979323846;
    phase = mod(phase, 2.0 * pi);
    float major = max(phasePixel.x, phasePixel.y);
    float minor = min(phasePixel.x, phasePixel.y);
    float average;
    if (minor < 0.025)
    {
        // The two-point Gaussian minor-axis integral avoids dividing a
        // second difference by a nearly zero area. Its finite error is fourth
        // order in a minor footprint below 0.025 radians.
        float offset = minor * 0.28867513459481288225;
        average = 0.5 * (shoreRippleLineAverage(phase - offset, major) +
                         shoreRippleLineAverage(phase + offset, major));
    }
    else
    {
        vec2 halfPixel = phasePixel * 0.5;
        average = 0.11279296875 +
            (shoreRipplePrimitives(phase + halfPixel.x + halfPixel.y).y -
             shoreRipplePrimitives(phase + halfPixel.x - halfPixel.y).y -
             shoreRipplePrimitives(phase - halfPixel.x + halfPixel.y).y +
             shoreRipplePrimitives(phase - halfPixel.x - halfPixel.y).y) /
            (phasePixel.x * phasePixel.y);
    }
    return mix(raw, clamp(average, 0.0, 1.0),
               smoothstep(0.05, 0.10, footprint));
}

vec3 planarReflection(vec3 approximate, vec3 normal, float fresnel,
                      float depthAmount, float detailVisibility)
{
    // Derivatives must precede the per-fragment early returns below. The
    // shading normal deliberately keeps the old wave approximation on sides;
    // only the geometric sheet may sample this horizontal planar reflection.
    vec3 geometricNormal = cross(dFdx(waterWorldPosition), dFdy(waterWorldPosition));
    float geometricLength = length(geometricNormal);
    if (any(isnan(geometricNormal)) || any(isinf(geometricNormal)) ||
        isnan(geometricLength) || isinf(geometricLength) || geometricLength <= 0.0 ||
        abs(geometricNormal.y) < 0.5 * geometricLength) return approximate;

    // A single mean plane serves only its animated sheet. Other levels,
    // underwater/crossing views and legacy/off paths keep their approximation.
    if (planarReflectionEnabled < 0.5 || linearHdrMode < 0.5 ||
        abs(waterWorldPosition.y - planarReflectionPlaneY) > 0.16 ||
        cameraPosition.y <= planarReflectionPlaneY + 0.15) return approximate;
    vec4 projected = planarReflectionViewProj * vec4(waterWorldPosition, 1.0);
    if (any(isnan(projected)) || any(isinf(projected)) || projected.w <= 0.0001)
        return approximate;
    vec2 uv = projected.xy / projected.w * 0.5 + 0.5;
    vec2 guard = max(planarReflectionTexelSize * 1.5, vec2(0.00001));
    if (any(lessThan(uv, guard)) || any(greaterThan(uv, vec2(1.0) - guard)))
        return approximate;
    vec2 warp = clamp(normal.xz * 0.018 + waterSurfaceDrift *
        sin(globalTime * 0.7 + dot(waterWorldPosition.xz, vec2(0.13, 0.09))) * 0.001,
        vec2(-0.012), vec2(0.012));
    warp *= clamp(waterDetailStrength, 0.0, 1.0) * detailVisibility;
    vec2 sampledUv = uv + warp;
    if (any(lessThan(sampledUv, guard)) || any(greaterThan(sampledUv, vec2(1.0) - guard)))
        return approximate;
    vec3 radiance = texture(planarReflectionTexture, sampledUv).rgb;
    if (any(isnan(radiance)) || any(isinf(radiance))) return approximate;
    vec2 edge = min(sampledUv - guard, vec2(1.0) - guard - sampledUv);
    float edgeFade = smoothstep(0.0, 0.035, min(edge.x, edge.y));
    float amount = fresnel * 0.72 * mix(0.55, 1.0, depthAmount) * edgeFade;
    return mix(approximate, max(radiance, vec3(0.0)), amount);
}

void main()
{
    vec3 normal = normalize(waterWorldNormal);
    vec3 viewDirection = normalize(cameraPosition - waterWorldPosition);
    float facing = clamp(dot(normal, viewDirection), 0.0, 1.0);
    float fresnel = 0.08 + 0.82 * pow(1.0 - facing, 3.2);

    // Snapshot metres of actual water below this surface. Distance is reserved
    // for atmospheric fog, so an unchanged pool keeps its depth as we move.
    float depth = clamp(waterSurfaceData.x, 0.0, 8.0);
    float depthAmount = 1.0 - exp(-depth * 0.32);
    float skyAvailability = waterLightSources.x >= 0.0 ? clamp(waterLightSources.x, 0.0, 1.0) : 1.0;
    // The environment palette already contains outdoor daylight. A sealed
    // pool needs a stable material colour before its propagated local light
    // is applied; otherwise torch-lit water still goes dark every night.
    vec3 shallowColour = mix(sceneColour(vec3(0.12, 0.43, 0.53)), sceneColour(waterShallowColour), skyAvailability);
    vec3 deepColour = mix(sceneColour(vec3(0.018, 0.15, 0.24)), sceneColour(waterDeepColour), skyAvailability);
    vec3 bodyColour = mix(shallowColour, deepColour, depthAmount);

    float skyAmount = clamp(normal.y * 0.72 + (1.0 - facing) * 0.28,
                            0.0, 1.0);
    vec3 reflectedSky = mix(bodyColour * 0.72, mix(sceneColour(skyHorizonColour), sceneColour(skyZenithColour), skyAmount), skyAvailability);
    vec3 colour = mix(bodyColour, reflectedSky, fresnel * 0.72);

    float motion = clamp(length(waterSurfaceDrift), 0.0, 1.0);
    float footprint = max(length(dFdx(waterWorldPosition.xz)),
                          length(dFdy(waterWorldPosition.xz)));
    float detailVisibility = 1.0 - smoothstep(0.20, 0.80, footprint);

    float shore = smoothstep(0.04, 0.62, waterSurfaceData.y);
    float ripplePhase = shore * 22.0 - globalTime * 2.4 +
        dot(waterWorldPosition.xz, vec2(0.12, 0.08));
    float ripple = pow(max(sin(ripplePhase), 0.0), 12.0) *
        shore * (1.0 - shore) * waterDetailStrength *
        mix(0.35, 1.0, motion) * detailVisibility;
    // Keep the complete legacy ripple expression and evaluation unchanged.
    if (linearHdrMode > 0.5)
    {
        ripple = filteredShoreRipple(ripplePhase) *
            shore * (1.0 - shore) * waterDetailStrength *
            mix(0.35, 1.0, motion) * detailVisibility;
    }
    colour *= 1.0 - shore * 0.08 * waterDetailStrength;
    colour += mix(shallowColour, sceneColour(vec3(0.73, 0.85, 0.81)), 0.65) * ripple * 0.38;

    // Two overlapping advection phases reset only at zero weight. Their
    // bounded offsets avoid long-session stretching or a visible time seam.
    float driftPhase = fract(globalTime * 0.15);
    float driftBlend = 1.0 - abs(driftPhase * 2.0 - 1.0);
    vec2 drift = waterSurfaceDrift * 1.8;
    float driftA = surfaceStreak(waterWorldPosition.xz - drift * driftPhase);
    float driftB = surfaceStreak(waterWorldPosition.xz - drift * fract(driftPhase + 0.5));
    float streak = mix(driftB, driftA, driftBlend);
    colour *= 1.0 + streak * 0.035 * waterDetailStrength *
        motion * detailVisibility;

    vec3 halfDirection = normalize(viewDirection + normalize(sunDirection));
    float sunSparkle = pow(max(dot(normal, halfDirection), 0.0), 48.0) *
                       sunIntensity * skyAvailability;
    colour += sceneColour(sunColour) * sunSparkle * 0.20;

    float diffuseLight = mix(0.70, 1.0, clamp(waterLight, 0.0, 1.0));
    float exposure = mix(0.48, 1.0, environmentLight);
    if (waterLightSources.x >= 0.0) {
        vec2 sources = clamp(waterLightSources, 0.0, 1.0);
        float sourceMaximum = max(sources.x, sources.y);
        exposure = mix(0.34, max(sources.x * exposure, sources.y) / max(sourceMaximum, 0.00001),
            smoothstep(0.0, 0.20, sourceMaximum));
    }
    colour *= diffuseLight * exposure;

    float eyeHeight = cameraPosition.y - waterWorldPosition.y;
    float aboveSurface = smoothstep(-0.20, 0.20, eyeHeight);
    colour = mix(mix(deepColour * 0.72 * exposure, colour, 0.20),
                 colour, aboveSurface);

    // Blend after body illumination: reflected scene radiance is already lit.
    colour = planarReflection(colour, normal, fresnel, depthAmount, detailVisibility);

    float fogVisibility = clamp(
        exp(-waterDistance * waterDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        waterWorldPosition - cameraPosition);
    localFogColour = mix(sceneColour(vec3(0.035, 0.043, 0.054)), localFogColour, skyAvailability);
    colour = mix(localFogColour, colour, fogVisibility);
    float surfaceAlpha = clamp(mix(0.36, 0.84, depthAmount) + fresnel * 0.12,
                               0.36, 0.94);
    float alpha = mix(0.90, surfaceAlpha, aboveSurface);
    // When the eye crosses this sheet, the 0.1 m near plane cuts its silhouette
    // into a screen-wide strip. Fade only this narrow crossing collar; resident
    // water-medium fog still describes immersion and depth absorption is kept
    // outside it. Approach from below is longer because the sheet covers most
    // of the sky. Shared surface heights preserve chunk continuity.
    float crossingWidth = mix(1.0, 0.35, aboveSurface);
    alpha *= smoothstep(0.10, crossingWidth, abs(eyeHeight));
    fragmentColour = vec4(linearHdrMode > 0.5 ? max(colour, vec3(0.0)) : clamp(colour, 0.0, 1.0),
        alpha * viewRangeCoverage(waterWorldPosition));
}
