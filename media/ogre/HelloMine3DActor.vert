#version 150

in vec4 vertex;

out float actorDistance;
out vec3 actorWorldPosition;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;

void main()
{
    gl_Position = worldViewProj * vertex;
    actorDistance = length((worldView * vertex).xyz);
    actorWorldPosition = (world * vertex).xyz;
}
