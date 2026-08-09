#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in float uv2;

out vec2 terrainTileUv;
out vec2 terrainRepeat;
out float terrainLight;

uniform mat4 worldViewProj;
uniform float globalTime;

void main()
{
    vec4 animatedVertex = vertex;
    animatedVertex.y += sin((globalTime + vertex.x) * 1.5) / 8.8;
    animatedVertex.y += cos((globalTime + vertex.z) * 1.5) / 8.1;
    animatedVertex.y -= 0.2;

    gl_Position = worldViewProj * animatedVertex;
    terrainTileUv = uv0;
    terrainRepeat = uv1;
    terrainLight = uv2;
}
