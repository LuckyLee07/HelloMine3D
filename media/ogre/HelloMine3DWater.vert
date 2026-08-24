#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in float uv2;

out vec3 waterWorldPosition;
out vec3 waterWorldNormal;
out float waterLight;
out float waterDistance;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;
uniform float globalTime;

void main()
{
    vec4 animatedVertex = vertex;
    float phaseA = globalTime * 0.78 + vertex.x * 0.66 + vertex.z * 0.21;
    float phaseB = globalTime * 0.53 + vertex.z * 0.82 - vertex.x * 0.17;
    animatedVertex.y += sin(phaseA) * 0.035;
    animatedVertex.y += cos(phaseB) * 0.025;
    animatedVertex.y -= 0.10;

    float slopeX = cos(phaseA) * 0.035 * 0.66 +
                   sin(phaseB) * 0.025 * 0.17;
    float slopeZ = cos(phaseA) * 0.035 * 0.21 -
                   sin(phaseB) * 0.025 * 0.82;
    vec3 localNormal = normalize(vec3(-slopeX, 1.0, -slopeZ));
    vec4 worldPosition = world * animatedVertex;

    gl_Position = worldViewProj * animatedVertex;
    waterWorldPosition = worldPosition.xyz;
    waterWorldNormal = normalize(mat3(world) * localNormal);
    waterLight = uv2;
    waterDistance = length((worldView * animatedVertex).xyz);
}
