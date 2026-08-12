#version 150

in float actorDistance;

out vec4 fragmentColour;

uniform vec4 actorTint;
uniform float environmentLight;
uniform vec3 fogColour;
uniform float fogDensity;

void main()
{
    float fogVisibility = clamp(
        exp(-actorDistance * actorDistance * fogDensity * fogDensity),
        0.0, 1.0);
    fragmentColour = vec4(
        mix(fogColour, actorTint.rgb * environmentLight, fogVisibility),
        actorTint.a);
}
