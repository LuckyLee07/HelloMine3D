#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in vec3 uv2;

out vec2 terrainTileUv;
out vec2 terrainRepeat;
out float terrainLight;
out vec2 terrainLightSources;
out float terrainDistance;
out vec3 terrainWorldPosition;
out vec4 terrainShadowPosition;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;
uniform mat4 shadowWorldViewProj;

void main()
{
    gl_Position = worldViewProj * vertex;
    terrainTileUv = uv0;
    terrainRepeat = uv1;
    terrainLight = uv2.x;
    // A positive Z tag distinguishes copied world light from legacy/manual meshes.
    terrainLightSources = uv2.z >= 1.0 ? vec2(uv2.y, uv2.z - 1.0) : vec2(-1.0);
    terrainDistance = length((worldView * vertex).xyz);
    terrainWorldPosition = (world * vertex).xyz;
    terrainShadowPosition = shadowWorldViewProj * vertex;
}
