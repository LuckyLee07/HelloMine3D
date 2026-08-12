#version 150

in vec3 vDirection;

uniform samplerCube skyboxMap;
uniform vec3 skyTint;

out vec4 fragColor;

void main()
{
    vec4 sky = texture(skyboxMap, vDirection);
    fragColor = vec4(sky.rgb * skyTint, sky.a);
}
