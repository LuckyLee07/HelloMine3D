#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in float uv2;

out vec2 terrainTileUv;
out vec2 terrainRepeat;
out float terrainLight;

uniform mat4 worldViewProj;

void main()
{
    gl_Position = worldViewProj * vertex;
    terrainTileUv = uv0;
    terrainRepeat = uv1;
    terrainLight = uv2;
}
