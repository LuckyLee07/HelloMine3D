#version 150

in float actorDistance;
in vec3 actorWorldPosition;
in vec4 actorShadowPosition;

out vec4 fragmentColour;

uniform vec4 actorTint;
uniform sampler2D directionalShadowMap;
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
        actorShadowPosition.w <= 0.00001)
    {
        return 1.0;
    }
    vec3 projected = actorShadowPosition.xyz / actorShadowPosition.w;
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
        actorDistance);
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
    float environmentExposure = mix(
        0.34, 1.0, clamp(environmentLight, 0.0, 1.0));
    float fogVisibility = clamp(
        exp(-actorDistance * actorDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        actorWorldPosition - cameraPosition);
    fragmentColour = vec4(
        mix(localFogColour, actorTint.rgb * environmentExposure *
            directionalShadowVisibility(),
            fogVisibility),
        actorTint.a);
}
