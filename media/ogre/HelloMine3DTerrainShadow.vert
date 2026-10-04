#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in vec3 uv2;
in float uv3;

out vec2 terrainTileUv;
out vec2 terrainRepeat;
out float terrainLight;
out vec2 terrainLightSources;
out float terrainDistance;
out vec3 terrainWorldPosition;
flat out vec3 terrainNaturalTreeRoot;
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
    // One root tag is copied to every face of a planned natural tree. Tags
    // use section-local X/Z plus six, packed into two five-bit fields.
    float rootCode = max(0.0, uv3 - 1.0);
    vec2 localRoot = vec2(floor(rootCode / 32.0), mod(rootCode, 32.0)) - 6.0;
    terrainNaturalTreeRoot = vec3((world * vec4(localRoot.x, 0.0, localRoot.y, 1.0)).xz, uv3);
    terrainShadowPosition = shadowWorldViewProj * vertex;
}
