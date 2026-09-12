#version 150
in vec4 vertex;
in vec4 colour;
in vec2 uv0;
in vec2 uv1;
out vec2 particleTileUv;
out vec2 particleUv;
out vec4 particleColour;
out float particleDistance;
uniform mat4 worldViewProj;
uniform mat4 worldView;
void main()
{
    gl_Position = worldViewProj * vertex;
    particleTileUv = uv0;
    particleUv = uv1;
    particleColour = colour;
    particleDistance = length((worldView * vertex).xyz);
}
