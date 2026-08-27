#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in float uv2;

out vec2 terrainTileUv;
out vec2 terrainRepeat;
out float terrainLight;
out float terrainDistance;
out vec3 terrainWorldPosition;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;

void main()
{
    gl_Position = worldViewProj * vertex;
    terrainTileUv = uv0;
    terrainRepeat = uv1;
    terrainLight = uv2;
    terrainDistance = length((worldView * vertex).xyz);
    terrainWorldPosition = (world * vertex).xyz;
}
