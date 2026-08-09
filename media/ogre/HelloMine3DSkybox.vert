#version 150

in vec4 vertex;
in vec3 uv0;

uniform mat4 worldViewProj;

out vec3 vDirection;

void main()
{
    gl_Position = worldViewProj * vertex;
    vDirection = uv0;
}
