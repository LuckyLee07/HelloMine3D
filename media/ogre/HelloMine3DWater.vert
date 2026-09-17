#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in float uv2;

out vec3 waterWorldPosition;
out vec3 waterWorldNormal;
out float waterLight;
out float waterDistance;
out vec2 waterSurfaceData;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;
uniform float globalTime;
uniform float waterDetailStrength;

void main()
{
    vec4 animatedVertex = vertex;
    // GPU vertices are section-local. Shared edges must sample the same
    // world-space wave or adjacent sections separate as they animate.
    vec4 baseWorldPosition = world * vertex;
    float phaseA = globalTime * 0.78 + baseWorldPosition.x * 0.66 +
                   baseWorldPosition.z * 0.21;
    float phaseB = globalTime * 0.53 + baseWorldPosition.z * 0.82 -
                   baseWorldPosition.x * 0.17;
    animatedVertex.y += sin(phaseA) * 0.035 * waterDetailStrength;
    animatedVertex.y += cos(phaseB) * 0.025 * waterDetailStrength;
    animatedVertex.y -= 0.10;

    float slopeX = cos(phaseA) * 0.035 * 0.66 +
                   sin(phaseB) * 0.025 * 0.17;
    float slopeZ = cos(phaseA) * 0.035 * 0.21 -
                   sin(phaseB) * 0.025 * 0.82;
    vec3 localNormal = normalize(vec3(-slopeX * waterDetailStrength, 1.0,
                                     -slopeZ * waterDetailStrength));
    vec4 worldPosition = world * animatedVertex;

    gl_Position = worldViewProj * animatedVertex;
    waterWorldPosition = worldPosition.xyz;
    waterWorldNormal = normalize(mat3(world) * localNormal);
    waterLight = uv2;
    waterDistance = length((worldView * animatedVertex).xyz);
    waterSurfaceData = uv1;
}
