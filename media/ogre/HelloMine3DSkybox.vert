#version 150

in vec4 vertex;

uniform mat4 worldViewProj;

out vec3 vDirection;

void main()
{
    gl_Position = worldViewProj * vertex;
    vDirection = vertex.xyz;
}
