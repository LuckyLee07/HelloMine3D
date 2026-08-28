#version 150

in vec4 vertex;

out float actorDistance;
out vec3 actorWorldPosition;
out vec4 actorShadowPosition;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;
uniform mat4 shadowWorldViewProj;

void main()
{
    gl_Position = worldViewProj * vertex;
    actorDistance = length((worldView * vertex).xyz);
    actorWorldPosition = (world * vertex).xyz;
    actorShadowPosition = shadowWorldViewProj * vertex;
}
