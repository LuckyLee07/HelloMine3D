#version 150

in vec3 waterWorldPosition;
in vec3 waterWorldNormal;
in float waterLight;
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
uniform float globalTime;
uniform float waterDetailStrength;

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

float surfaceStreak(vec2 position)
{
    return sin(position.x * 3.1 + sin(position.y * 1.7)) *
           sin(position.y * 4.3 - position.x * 0.8);
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
    vec3 bodyColour = mix(waterShallowColour, waterDeepColour, depthAmount);

    float skyAmount = clamp(normal.y * 0.72 + (1.0 - facing) * 0.28,
                            0.0, 1.0);
    vec3 reflectedSky = mix(skyHorizonColour, skyZenithColour, skyAmount);
    vec3 colour = mix(bodyColour, reflectedSky, fresnel * 0.72);

    float shore = smoothstep(0.04, 0.62, waterSurfaceData.y);
    float ripplePhase = shore * 22.0 - globalTime * 2.4 +
        dot(waterWorldPosition.xz, vec2(0.12, 0.08));
    float ripple = pow(max(sin(ripplePhase), 0.0), 12.0) *
        shore * (1.0 - shore) * waterDetailStrength;
    colour *= 1.0 - shore * 0.08 * waterDetailStrength;
    colour += mix(waterShallowColour, vec3(0.73, 0.85, 0.81), 0.65) * ripple * 0.38;

    // Two overlapping advection phases reset only at zero weight. Their
    // bounded offsets avoid long-session stretching or a visible time seam.
    float driftPhase = fract(globalTime * 0.15);
    float driftBlend = 1.0 - abs(driftPhase * 2.0 - 1.0);
    vec2 drift = waterSurfaceDrift * 1.8;
    float driftA = surfaceStreak(waterWorldPosition.xz - drift * driftPhase);
    float driftB = surfaceStreak(waterWorldPosition.xz - drift * fract(driftPhase + 0.5));
    float streak = mix(driftB, driftA, driftBlend);
    colour *= 1.0 + streak * 0.035 * waterDetailStrength *
        clamp(length(waterSurfaceDrift), 0.0, 1.0);

    vec3 halfDirection = normalize(viewDirection + normalize(sunDirection));
    float sunSparkle = pow(max(dot(normal, halfDirection), 0.0), 96.0) *
                       sunIntensity;
    colour += sunColour * sunSparkle * 0.82;

    float diffuseLight = mix(0.70, 1.0, clamp(waterLight, 0.0, 1.0));
    colour *= diffuseLight * mix(0.48, 1.0, environmentLight);

    bool cameraBelowSurface = cameraPosition.y < waterWorldPosition.y + 0.12;
    if (cameraBelowSurface)
    {
        colour = mix(waterDeepColour * 0.72, colour, 0.20);
    }

    float fogVisibility = clamp(
        exp(-waterDistance * waterDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        waterWorldPosition - cameraPosition);
    colour = mix(localFogColour, colour, fogVisibility);
    float alpha = cameraBelowSurface ? 0.90 :
        clamp(mix(0.36, 0.84, depthAmount) + fresnel * 0.12, 0.36, 0.94);
    fragmentColour = vec4(clamp(colour, 0.0, 1.0), alpha);
}
