#version 150

in vec4 vertex;
in vec2 uv0;
in float uv1;

out vec2 terrainUv;
out float terrainLight;

uniform mat4 worldViewProj;

void main()
{
    gl_Position = worldViewProj * vertex;
    terrainUv = uv0;
    terrainLight = uv1;
}
