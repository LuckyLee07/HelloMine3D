#version 150

in float actorDistance;
in vec3 actorWorldPosition;
in vec3 actorLocalPosition;

out vec4 fragmentColour;

uniform vec4 actorTint;
uniform vec4 actorPartData;
uniform float actorSurfaceStrength;
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

// All cues are derived from the copied actor pose: role, guardian, windup.
// The local front face is -Z, matching the registered enemy profiles.
vec3 readableActorSurface()
{
    if (actorSurfaceStrength < 0.5) return actorTint.rgb;
    vec3 faceNormal = normalize(cross(dFdx(actorWorldPosition), dFdy(actorWorldPosition)));
    float shade = 0.68 + 0.22 * max(faceNormal.y, 0.0) +
                  0.10 * max(dot(faceNormal, normalize(vec3(-0.4, 0.6, -0.5))), 0.0);
    float role = actorPartData.x;
    float front = step(actorLocalPosition.z, -0.499);
    vec3 base = actorTint.rgb;
    if (role > 4.5 && role < 6.5) base *= 0.60; // Feet remain distinct from torso.
    if (role > 2.5 && role < 4.5) base *= 0.84;
    if (role > 0.5 && role < 1.5) {
        float belt = 1.0 - step(0.075, abs(actorLocalPosition.y + 0.28));
        base = mix(base, base * 0.48, belt);
    }
    if (role > 1.5 && role < 2.5) {
        base = mix(base, vec3(0.62, 0.65, 0.58), 0.34);
        float brow = front * (1.0 - step(0.08, abs(actorLocalPosition.y - 0.18)));
        base = mix(base, base * 0.43, brow);
        float eyes = front * (1.0 - step(0.10, abs(abs(actorLocalPosition.x) - 0.23))) *
                     (1.0 - step(0.055, abs(actorLocalPosition.y - 0.05)));
        vec3 eyeColour = mix(vec3(0.90, 0.79, 0.47), vec3(1.0, 0.31, 0.12), actorPartData.z);
        base = mix(base * shade, eyeColour, eyes);
        return base;
    }
    if (role > 7.5 && actorPartData.y > 0.5)
        return mix(vec3(0.18, 0.58, 0.64), vec3(0.61, 0.90, 0.88), actorPartData.z);
    return base * shade;
}


// A small eye/core floor makes facing and windup legible at night. Fog and
// depth testing still occlude it; it is neither a point light nor an outline.
vec3 actorCueEmission()
{
    if (actorSurfaceStrength < 0.5) return vec3(0.0);
    if (actorPartData.x > 1.5 && actorPartData.x < 2.5) {
        float eyes = step(actorLocalPosition.z, -0.499) *
            (1.0 - step(0.10, abs(abs(actorLocalPosition.x) - 0.23))) *
            (1.0 - step(0.055, abs(actorLocalPosition.y - 0.05)));
        return mix(vec3(0.90, 0.79, 0.47), vec3(1.0, 0.31, 0.12), actorPartData.z) * eyes * 0.64;
    }
    if (actorPartData.x > 7.5 && actorPartData.y > 0.5)
        return vec3(0.18, 0.58, 0.64) * 0.38;
    return vec3(0.0);
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
        mix(localFogColour, max(readableActorSurface() * environmentExposure, actorCueEmission()),
            fogVisibility),
        actorTint.a);
}
