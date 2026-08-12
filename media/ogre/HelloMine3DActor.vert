#version 150

in vec4 vertex;

out float actorDistance;

uniform mat4 worldViewProj;
uniform mat4 worldView;

void main()
{
    gl_Position = worldViewProj * vertex;
    actorDistance = length((worldView * vertex).xyz);
}
