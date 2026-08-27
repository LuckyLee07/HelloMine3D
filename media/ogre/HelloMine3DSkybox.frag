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

    float sampleDistance = cameraInside
        ? min(farDistance * 0.08, cloudHorizontalScale * 1.25)
        : nearDistance;
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
    float firstDensity = cloudNoise(firstUv);
    float secondDensity = cloudNoise(secondUv + vec2(0.31, -0.17));
    float density = max(firstDensity, secondDensity * 0.94);
    float threshold = mix(0.70, 0.46, cloudCoverage);
    float body = smoothstep(threshold - 0.12,
                            threshold + 0.12, density);
    float edge = smoothstep(threshold, threshold + 0.13, density);
    float distanceFade = 1.0 - smoothstep(
        cloudMaxDistance * 0.72, cloudMaxDistance,
        cameraInside ? 0.0 : nearDistance);
    float horizonFade = cameraInside
        ? 1.0
        : smoothstep(0.012, 0.065, abs(direction.y));
    float opticalDepth = clamp(
        layerTravel / max(cloudThickness, 1.0), 0.0, 3.0);
    mask = edge * distanceFade * horizonFade *
           clamp(0.66 + opticalDepth * 0.10, 0.0, 0.92);

    float topLighting = cameraPosition.y > top
        ? 0.88
        : (cameraPosition.y < bottom ? 0.24 : 0.48);
    float lightAmount = clamp(
        topLighting + body * 0.34 - opticalDepth * 0.035,
        0.12, 1.0);
    colour = mix(cloudShadowColour, cloudLightColour, lightAmount);
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
