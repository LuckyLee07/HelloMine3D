#version 150

in vec3 vDirection;

uniform vec3 skyZenithColour;
uniform vec3 skyHorizonColour;
uniform vec3 sunDirection;
uniform vec3 sunColour;
uniform float sunIntensity;
uniform float moonIntensity;
uniform float starIntensity;
uniform vec3 cloudLightColour;
uniform vec3 cloudShadowColour;
uniform float cloudCoverage;
uniform vec3 fogSunwardColour;
uniform float fogDirectionalStrength;
uniform float cloudLayerEnabled;
uniform float cloudBaseHeight;
uniform float cloudThickness;
uniform float cloudHorizontalScale;
uniform vec2 cloudVelocity;
uniform float cloudMaxDistance;
uniform vec3 cameraPosition;
uniform float globalTime;
uniform float legacyTime;

out vec4 fragColor;

float hash31(vec3 value)
{
    value = fract(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
}

float hash21(vec2 value)
{
    vec3 projected = fract(vec3(value.xyx) * 0.1031);
    projected += dot(projected, projected.yzx + 33.33);
    return fract((projected.x + projected.y) * projected.z);
}

float valueNoise(vec2 value)
{
    vec2 cell = floor(value);
    vec2 blend = fract(value);
    blend = blend * blend * (3.0 - 2.0 * blend);
    float a = hash21(cell);
    float b = hash21(cell + vec2(1.0, 0.0));
    float c = hash21(cell + vec2(0.0, 1.0));
    float d = hash21(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

float cloudNoise(vec2 value)
{
    float result = valueNoise(value) * 0.58;
    result += valueNoise(value * 2.03 + 19.7) * 0.28;
    result += valueNoise(value * 4.07 - 7.3) * 0.14;
    return result;
}

// Interpolate occupied cloud cells, not warped noise coordinates. Four
// shared coarse lattice values define all four fine-cell corners. This keeps
// real, bounded edge coverage without false slivers or nearest-cell popping.
vec4 cloudCellSample(vec2 value, float threshold)
{
    vec2 coarse = floor(value);
    vec2 fine = fract(value) * 12.0;
    vec2 low = floor(fine) / 12.0;
    vec2 high = low + vec2(1.0 / 12.0);
    low = low * low * (3.0 - 2.0 * low);
    high = high * high * (3.0 - 2.0 * high);
    float a = hash21(coarse);
    float b = hash21(coarse + vec2(1.0, 0.0));
    float c = hash21(coarse + vec2(0.0, 1.0));
    float d = hash21(coarse + vec2(1.0, 1.0));
    vec2 lowRow = vec2(mix(a, b, low.x), mix(c, d, low.x));
    vec2 highRow = vec2(mix(a, b, high.x), mix(c, d, high.x));
    vec4 corners = vec4(mix(lowRow.x, lowRow.y, low.y),
                        mix(highRow.x, highRow.y, low.y),
                        mix(lowRow.x, lowRow.y, high.y),
                        mix(highRow.x, highRow.y, high.y));
    vec4 occupied = step(vec4(threshold), corners);
    vec2 edge = smoothstep(0.45, 0.55, fract(fine));
    float coverage = mix(mix(occupied.x, occupied.y, edge.x),
                         mix(occupied.z, occupied.w, edge.x), edge.y);
    vec2 part = fract(value);
    vec2 blend = part * part * (3.0 - 2.0 * part);
    vec2 slope = 6.0 * part * (1.0 - part);
    return vec4(coverage, mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y),
                mix(b - a, d - c, blend.y) * slope.x,
                mix(c - a, d - b, blend.x) * slope.y);
}

vec3 directionalFogColour(vec3 viewDirection)
{
    vec3 normalisedView = normalize(viewDirection);
    vec2 viewHorizontal = normalisedView.xz;
    vec2 sunHorizontal = sunDirection.xz;
    float viewLength = length(viewHorizontal);
    float sunLength = length(sunHorizontal);
    if (viewLength < 0.00001 || sunLength < 0.00001)
    {
        return skyHorizonColour;
    }
    float horizonAmount = 1.0 - smoothstep(
        0.12, 0.65, abs(normalisedView.y));
    float alignment = max(dot(viewHorizontal / viewLength,
                              sunHorizontal / sunLength), 0.0);
    float amount = clamp(fogDirectionalStrength * horizonAmount *
                         alignment * alignment * alignment, 0.0, 1.0);
    return mix(skyHorizonColour, fogSunwardColour, amount);
}

void sampleLegacyClouds(vec3 direction, out float mask,
                        out vec3 colour)
{
    float cloudHorizonFade = smoothstep(0.025, 0.16, direction.y);
    vec2 cloudUv = direction.xz / max(0.20, direction.y + 0.24);
    cloudUv = cloudUv * 2.7 + vec2(legacyTime * 0.006,
                                   legacyTime * 0.0025);
    float clouds = cloudNoise(cloudUv);
    float cloudThreshold = mix(0.70, 0.46, cloudCoverage);
    mask = smoothstep(cloudThreshold,
                      cloudThreshold + 0.13, clouds) *
           cloudHorizonFade * 0.82;
    float cloudBody = smoothstep(cloudThreshold - 0.12,
                                 cloudThreshold + 0.12, clouds);
    colour = mix(cloudShadowColour, cloudLightColour,
                 0.30 + cloudBody * 0.70);
}

void sampleBoundedCloudLayer(vec3 direction, out float mask,
                             out vec3 colour)
{
    mask = 0.0;
    colour = cloudShadowColour;

    float halfThickness = max(cloudThickness * 0.5, 0.5);
    float bottom = cloudBaseHeight - halfThickness;
    float top = cloudBaseHeight + halfThickness;
    bool cameraInside = cameraPosition.y >= bottom &&
                        cameraPosition.y <= top;
    float nearDistance = 0.0;
    float farDistance = 0.0;
    bool visible = false;

    if (abs(direction.y) < 0.00001)
    {
        if (cameraInside)
        {
            visible = true;
            farDistance = cloudMaxDistance;
        }
    }
    else
    {
        float first = (bottom - cameraPosition.y) / direction.y;
        float second = (top - cameraPosition.y) / direction.y;
        nearDistance = max(min(first, second), 0.0);
        farDistance = min(max(first, second), cloudMaxDistance);
        visible = farDistance > nearDistance;
    }

    if (!visible)
    {
        return;
    }

    // The ray entry tends to zero continuously when crossing either face.
    // An inside-only offset would jump to a different part of the noise field.
    float sampleDistance = nearDistance;
    float layerTravel = max(farDistance - nearDistance, 0.0);
    float secondDistance = min(
        farDistance,
        sampleDistance + min(layerTravel, cloudHorizontalScale * 0.75));
    vec2 motion = cloudVelocity * globalTime;
    vec2 firstUv = (cameraPosition.xz +
                    direction.xz * sampleDistance + motion) /
                   max(cloudHorizontalScale, 1.0);
    vec2 secondUv = (cameraPosition.xz +
                     direction.xz * secondDistance + motion) /
                    max(cloudHorizontalScale, 1.0);
    float threshold = mix(0.74, 0.53, cloudCoverage);
    vec4 firstSample = cloudCellSample(firstUv, threshold);
    vec4 secondSample = cloudCellSample(secondUv, threshold);
    vec3 cloudSample = mix(firstSample.yzw, secondSample.yzw, 0.32);
    float density = max(firstSample.y, secondSample.y);
    float body = smoothstep(threshold + 0.02,
                            threshold + 0.22, density);
    float entryEdge = firstSample.x;
    float exitEdge = secondSample.x;
    float edge = max(entryEdge, exitEdge);
    float sideAmount = (1.0 - entryEdge) * exitEdge;
    float distanceFade = 1.0 - smoothstep(
        cloudMaxDistance * 0.72, cloudMaxDistance,
        cameraInside ? 0.0 : nearDistance);
    float interiorBlend = smoothstep(0.0, max(cloudThickness * 0.15, 0.1),
        min(cameraPosition.y - bottom, top - cameraPosition.y));
    float horizonFade = mix(smoothstep(0.012, 0.065, abs(direction.y)),
                            1.0, interiorBlend);
    float opticalDepth = clamp(
        layerTravel / max(cloudThickness, 1.0), 0.0, 3.0);
    // An outward ray just inside the slab has nearly zero cloud travel.
    // Fade that short segment instead of exposing a full-opacity plane.
    float thicknessFade = smoothstep(0.0, 0.12, opticalDepth);
    mask = edge * distanceFade * horizonFade * thicknessFade *
           clamp(0.72 + opticalDepth * 0.10, 0.0, 0.96);

    // Thin rims transmit more sky light; denser centres retain a darker base.
    // Use the existing sun/moon direction and palette so dusk and night keep
    // their environment colours. No new light, shadow map or weather state.
    vec3 roundedNormal = normalize(vec3(-cloudSample.y * 0.85, 0.65,
                                        -cloudSample.z * 0.85));
    float sunFacing = dot(roundedNormal, normalize(sunDirection));
    // Blend both contributions continuously through sunrise/sunset.
    float directionalLight = max(sunFacing, 0.0) * sunIntensity +
                             max(-sunFacing, 0.0) * moonIntensity;
    float topLighting = mix(0.32, 0.72,
                             smoothstep(bottom, top, cameraPosition.y));
    float lightAmount = clamp(
        topLighting + directionalLight * 0.22 + (1.0 - body) * 0.16 +
        sideAmount * 0.30 - body * 0.12 - opticalDepth * 0.025,
        0.12, 1.0);
    colour = mix(cloudShadowColour, cloudLightColour, lightAmount);
}

// A fixed orbit reference remains well-conditioned at noon and midnight.
// Coordinates belong to the celestial body, never to the screen or camera.
vec2 celestialCoordinates(vec3 direction, vec3 bodyDirection, float radius)
{
    vec3 right = normalize(cross(vec3(0.0, 0.0, 1.0), bodyDirection));
    vec3 up = cross(bodyDirection, right);
    float forward = max(dot(direction, bodyDirection), 0.0001);
    return vec2(dot(direction, right), dot(direction, up)) / (forward * radius);
}

float pixelBodyMask(vec2 uv, float alignment, float antialias)
{
    vec2 edge = abs(uv);
    // Cut one square from each corner; retain a readable block silhouette.
    float boundary = max(max(edge.x, edge.y), min(edge.x, edge.y) + 0.25);
    return (1.0 - smoothstep(1.0 - antialias, 1.0 + antialias, boundary)) *
           step(0.0, alignment);
}

vec3 composePixelCelestials(vec3 colour, vec3 direction)
{
    vec3 toSun = normalize(sunDirection);
    float sunAlignment = dot(direction, toSun);
    float moonAlignment = -sunAlignment;
    // Evaluate derivatives before the varying branch. Only the small angular
    // region containing a disc/halo needs projection and pixel surface work.
    vec3 angularPixel = fwidth(direction);
    float antialias = clamp(max(max(angularPixel.x, angularPixel.y), angularPixel.z) /
                             0.046, 0.001, 0.08);
    if (sunAlignment <= 0.965 && moonAlignment <= 0.982)
    {
        return colour;
    }
    vec2 sunUv = celestialCoordinates(direction, toSun, 0.052);
    vec2 moonUv = celestialCoordinates(direction, -toSun, 0.046);
    float sunMask = pixelBodyMask(sunUv, sunAlignment, antialias);
    float moonMask = pixelBodyMask(moonUv, moonAlignment, antialias);

    float sunHalo = smoothstep(0.965, 0.9992, sunAlignment);
    float moonHalo = smoothstep(0.982, 0.9993, moonAlignment);
    colour += sunColour * sunIntensity * sunHalo * 0.14;
    colour += vec3(0.62, 0.72, 0.92) * moonIntensity * moonHalo * 0.08;

    vec2 sunPixel = (floor(sunUv * 8.0) + 0.5) / 8.0;
    float sunCore = 1.0 - step(0.76, max(abs(sunPixel.x), abs(sunPixel.y)));
    vec3 sunSurface = sunColour * mix(vec3(0.96, 0.79, 0.54),
                                      vec3(1.03, 1.01, 0.92), sunCore);
    colour = mix(colour, sunSurface, sunMask * sunIntensity);

    vec2 moonPixel = (floor(moonUv * 8.0) + 0.5) / 8.0;
    vec2 firstCrater = abs(moonPixel - vec2(-0.31, 0.25));
    vec2 secondCrater = abs(moonPixel - vec2(0.37, -0.31));
    vec2 thirdCrater = abs(moonPixel - vec2(-0.44, -0.50));
    float craters = max(max(1.0 - step(0.26, max(firstCrater.x, firstCrater.y)),
                            1.0 - step(0.17, max(secondCrater.x, secondCrater.y))),
                       1.0 - step(0.10, max(thirdCrater.x, thirdCrater.y)));
    float moonRim = step(0.77, max(abs(moonPixel.x), abs(moonPixel.y)));
    vec3 moonSurface = mix(vec3(0.72, 0.80, 0.91), vec3(0.48, 0.59, 0.73),
                           max(craters * 0.65, moonRim * 0.32));
    return mix(colour, moonSurface, moonMask * moonIntensity);
}

void main()
{
    vec3 direction = normalize(vDirection);
    float upperSky = smoothstep(0.0, 0.78, max(direction.y, 0.0));
    vec3 localHorizonColour = directionalFogColour(direction);
    vec3 colour = mix(localHorizonColour, skyZenithColour, upperSky);

    // Keep the lower hemisphere on the exact fog colour so distant terrain
    // fades into the sky without a hard horizon band.
    float horizonFade = smoothstep(-0.24, 0.04, direction.y);
    colour = mix(localHorizonColour, colour, horizonFade);

    vec3 starCell = floor(direction * 420.0);
    float starNoise = hash31(starCell);
    float stars = smoothstep(0.996, 1.0, starNoise) *
                  smoothstep(-0.02, 0.18, direction.y) *
                  starIntensity;
    colour += vec3(0.68, 0.78, 1.0) * stars;

    float cloudMask = 0.0;
    vec3 cloudColour = cloudShadowColour;
    if (cloudLayerEnabled < 0.5)
    {
        // Exact FS2 fallback: an infinite upper-hemisphere projection.
        sampleLegacyClouds(direction, cloudMask, cloudColour);
    }
    else
    {
        // V10C: intersect the camera ray with a world-space cloud slab.
        // The bounded segment creates translation/height parallax without
        // adding weather simulation, collision or a volumetric ray marcher.
        sampleBoundedCloudLayer(direction, cloudMask, cloudColour);
    }
    if (cloudLayerEnabled >= 0.5)
    {
        // Celestial bodies and their halos are behind the same cloud layer
        // as the stars. Cloud optical coverage therefore attenuates all three.
        colour = composePixelCelestials(colour, direction);
        colour = mix(colour, cloudColour, cloudMask);
        fragColor = vec4(clamp(colour, vec3(0.0), vec3(1.35)), 1.0);
        return;
    }

    // Preserve the complete legacy appearance when atmosphere is disabled.
    colour = mix(colour, cloudColour, cloudMask);
    float sunAlignment = dot(direction, normalize(sunDirection));
    float sunHalo = smoothstep(0.965, 0.9992, sunAlignment);
    float sunDisc = smoothstep(0.9988, 0.99975, sunAlignment);
    colour += sunColour * sunIntensity *
              (sunHalo * 0.22 + sunDisc * 1.35);

    vec3 moonDirection = -normalize(sunDirection);
    float moonAlignment = dot(direction, moonDirection);
    float moonHalo = smoothstep(0.982, 0.9993, moonAlignment);
    float moonDisc = smoothstep(0.9990, 0.99972, moonAlignment);
    vec3 moonColour = vec3(0.62, 0.72, 0.92);
    colour += moonColour * moonIntensity *
              (moonHalo * 0.12 + moonDisc * 0.9);

    fragColor = vec4(clamp(colour, vec3(0.0), vec3(1.35)), 1.0);
}
