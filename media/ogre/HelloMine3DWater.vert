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
    animatedVertex.y += sin((globalTime + vertex.x) * 1.5) / 8.8;
    animatedVertex.y += cos((globalTime + vertex.z) * 1.5) / 8.1;
    animatedVertex.y -= 0.2;

    gl_Position = worldViewProj * animatedVertex;
    terrainUv = uv0;
    terrainLight = uv1;
}
