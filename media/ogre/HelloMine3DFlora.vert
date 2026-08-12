#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in float uv2;

out vec2 terrainTileUv;
out vec2 terrainRepeat;
out float terrainLight;
out float terrainDistance;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform float globalTime;

void main()
{
    vec4 animatedVertex = vertex;
    animatedVertex.x +=
        sin((globalTime + vertex.z + vertex.y) * 1.8) / 15.0;
    animatedVertex.z -=
        cos((globalTime + vertex.x + vertex.y) * 1.8) / 15.0;

    gl_Position = worldViewProj * animatedVertex;
    terrainTileUv = uv0;
    terrainRepeat = uv1;
    terrainLight = uv2;
    terrainDistance = length((worldView * animatedVertex).xyz);
}
