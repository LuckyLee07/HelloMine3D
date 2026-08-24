#version 150

in vec3 waterWorldPosition;
in vec3 waterWorldNormal;
in float waterLight;
in float waterDistance;

out vec4 fragmentColour;

uniform float environmentLight;
uniform vec3 fogColour;
uniform float fogDensity;
uniform vec3 skyZenithColour;
uniform vec3 skyHorizonColour;
uniform vec3 sunDirection;
uniform vec3 sunColour;
uniform float sunIntensity;
uniform vec3 waterShallowColour;
uniform vec3 waterDeepColour;
uniform vec3 cameraPosition;

void main()
{
    vec3 normal = normalize(waterWorldNormal);
    vec3 viewDirection = normalize(cameraPosition - waterWorldPosition);
    float facing = clamp(dot(normal, viewDirection), 0.0, 1.0);
    float fresnel = 0.08 + 0.82 * pow(1.0 - facing, 3.2);

    float distanceDepth = smoothstep(12.0, 150.0, waterDistance);
    float angleDepth = 1.0 - clamp(normal.y, 0.0, 1.0);
    float depthAmount = clamp(0.18 + distanceDepth * 0.58 +
                              angleDepth * 0.24, 0.0, 1.0);
    vec3 bodyColour = mix(waterShallowColour, waterDeepColour, depthAmount);

    float skyAmount = clamp(normal.y * 0.72 + (1.0 - facing) * 0.28,
                            0.0, 1.0);
    vec3 reflectedSky = mix(skyHorizonColour, skyZenithColour, skyAmount);
    vec3 colour = mix(bodyColour, reflectedSky, fresnel * 0.72);

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
    colour = mix(fogColour, colour, fogVisibility);
    float alpha = cameraBelowSurface ? 0.94 : mix(0.72, 0.90, fresnel);
    fragmentColour = vec4(clamp(colour, 0.0, 1.0), alpha);
}
