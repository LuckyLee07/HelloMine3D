#version 150

in vec3 vDirection;

uniform samplerCube skyboxMap;

out vec4 fragColor;

void main()
{
    fragColor = texture(skyboxMap, vDirection);
}
