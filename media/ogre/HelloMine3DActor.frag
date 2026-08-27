#version 150

in float actorDistance;
in vec3 actorWorldPosition;

out vec4 fragmentColour;

uniform vec4 actorTint;
uniform float environmentLight;
uniform vec3 fogColour;
uniform vec3 fogSunwardColour;
uniform vec3 sunDirection;
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
    float environmentExposure = mix(
        0.34, 1.0, clamp(environmentLight, 0.0, 1.0));
    float fogVisibility = clamp(
        exp(-actorDistance * actorDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        actorWorldPosition - cameraPosition);
    fragmentColour = vec4(
        mix(localFogColour, actorTint.rgb * environmentExposure,
            fogVisibility),
        actorTint.a);
}
