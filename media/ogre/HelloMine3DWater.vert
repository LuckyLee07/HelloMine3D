#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in vec3 uv2;
// Water reuses the existing single-float owner slot: 0 animates normally,
// 1 pins a new opaque-boundary cut vertex. Other material owners are separate.
in float uv3;

out vec3 waterWorldPosition;
out vec3 waterWorldNormal;
out float waterLight;
out vec2 waterLightSources;
out float waterDistance;
out vec2 waterSurfaceData;
out vec2 waterSurfaceDrift;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;
uniform float globalTime;
uniform float waterDetailStrength;
// Optional compiled capability: legacy Water overrides keep their old mesh.
uniform float waterBoundaryPinsV1;

void main()
{
    vec4 animatedVertex = vertex;
    // GPU vertices are section-local. Shared edges must sample the same
    // world-space wave or adjacent sections separate as they animate.
    vec4 baseWorldPosition = world * vertex;
    // The snapshot velocity distinguishes open sea from river, still lake and
    // sheltered wetland. Shared corners carry identical speeds at chunk seams.
    float motion = clamp(length(uv0), 0.0, 1.0);
    float waveScale = mix(0.025, 1.0, smoothstep(0.04, 1.0, motion));
    // Every original top corner touching an opaque neighbour has raw shore
    // >= 0.25, also on its separate top face and across section boundaries.
    // Keep that contact at the original -0.10 surface offset: its fixed 0.90
    // height stays above all eighth-grid cuts (at most 0.875). Open-water
    // corners retain their old wave, giving a shared linear fade to the bank.
    // The optional guard leaves complete older shader/mesh paths unchanged.
    if (waterBoundaryPinsV1 > 0.5 && uv1.y > 0.0)
        waveScale = 0.0;
    float phaseA = globalTime * 0.78 + baseWorldPosition.x * 0.66 +
                   baseWorldPosition.z * 0.21;
    float phaseB = globalTime * 0.53 + baseWorldPosition.z * 0.82 -
                   baseWorldPosition.x * 0.17;
    // A cut boundary lies on the opaque shape in real world space. Moving
    // it down by the surface offset would recreate the co-planar overlap.
    // Ordinary shore corners retain the -0.10 surface offset above. Open
    // water (raw shore 0) and a disabled guard keep their complete old wave.
    bool fixedBoundary = waterBoundaryPinsV1 > 0.5 && uv3 > 0.5;
    if (!fixedBoundary)
    {
        animatedVertex.y += sin(phaseA) * 0.035 * waveScale * waterDetailStrength;
        animatedVertex.y += cos(phaseB) * 0.025 * waveScale * waterDetailStrength;
        animatedVertex.y -= 0.10;
    }

    float slopeX = cos(phaseA) * 0.035 * 0.66 +
                   sin(phaseB) * 0.025 * 0.17;
    float slopeZ = cos(phaseA) * 0.035 * 0.21 -
                   sin(phaseB) * 0.025 * 0.82;
    vec3 localNormal = normalize(vec3(-slopeX * waveScale * waterDetailStrength, 1.0,
                                     -slopeZ * waveScale * waterDetailStrength));
    vec4 worldPosition = world * animatedVertex;

    gl_Position = worldViewProj * animatedVertex;
    waterWorldPosition = worldPosition.xyz;
    waterWorldNormal = normalize(mat3(world) * localNormal);
    waterLight = uv2.x;
    waterLightSources = uv2.z >= 1.0 ? vec2(uv2.y, uv2.z - 1.0) : vec2(-1.0);
    waterDistance = length((worldView * animatedVertex).xyz);
    waterSurfaceData = uv1;
    waterSurfaceDrift = uv0;
}
