#version 150

in vec4 vertex;
in vec2 uv0;
in float uv1;

out vec2 terrainUv;
out float terrainLight;

uniform mat4 worldViewProj;
uniform float globalTime;

void main()
{
    vec4 animatedVertex = vertex;
    animatedVertex.x +=
        sin((globalTime + vertex.z + vertex.y) * 1.8) / 15.0;
    animatedVertex.z -=
        cos((globalTime + vertex.x + vertex.y) * 1.8) / 15.0;

    gl_Position = worldViewProj * animatedVertex;
    terrainUv = uv0;
    terrainLight = uv1;
}
