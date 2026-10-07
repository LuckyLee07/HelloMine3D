#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in vec3 uv2;

out vec3 waterWorldPosition;
out vec3 waterWorldNormal;
out float waterLight;
out vec2 waterLightSources;
out float waterDistance;
out vec2 waterSurfaceData;
out vec2 waterSurfaceDrift;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 world;
uniform vec3 cameraPosition;
uniform float globalTime;
uniform float waterDetailStrength;

void main()
{
    // GPU vertices are section-local. Shared edges must sample the same
    // world-space wave or adjacent sections separate as they animate.
    vec4 baseWorldPosition = world * vertex;
    // The snapshot velocity distinguishes open sea from river, still lake and
    // sheltered wetland. Shared corners carry identical speeds at chunk seams.
    float motion = clamp(length(uv0), 0.0, 1.0);
    float waveScale = mix(0.025, 1.0, smoothstep(0.04, 1.0, motion));
    float phaseA = globalTime * 0.78 + baseWorldPosition.x * 0.66 +
                   baseWorldPosition.z * 0.21;
    float phaseB = globalTime * 0.53 + baseWorldPosition.z * 0.82 -
                   baseWorldPosition.x * 0.17;
    // Keep displacement separate from section-local Y. The same surface can
    // be represented by local Y=1 or Y=17, whose rounded sums otherwise differ.
    float waveOffset = sin(phaseA) * 0.035 * waveScale * waterDetailStrength;
    waveOffset += cos(phaseB) * 0.025 * waveScale * waterDetailStrength;
    waveOffset -= 0.10;

    float slopeX = cos(phaseA) * 0.035 * 0.66 +
                   sin(phaseB) * 0.025 * 0.17;
    float slopeZ = cos(phaseA) * 0.035 * 0.21 -
                   sin(phaseB) * 0.025 * 0.82;
    vec3 localNormal = normalize(vec3(-slopeX * waveScale * waterDetailStrength, 1.0,
                                     -slopeZ * waveScale * waterDetailStrength));
    vec3 worldOffset = mat3(world) * vec3(0.0, waveOffset, 0.0);
    vec3 worldPosition = baseWorldPosition.xyz + worldOffset;
    // All sections project a shared corner through the same camera-relative
    // arithmetic. Avoid section-specific precombined matrices and avoid large
    // world-space products followed by cancellation of the camera translation.
    vec3 cameraRelativePosition = (baseWorldPosition.xyz - cameraPosition) + worldOffset;
    vec3 viewPosition = mat3(view) * cameraRelativePosition;

    gl_Position = projection * vec4(viewPosition, 1.0);
    waterWorldPosition = worldPosition;
    waterWorldNormal = normalize(mat3(world) * localNormal);
    waterLight = uv2.x;
    waterLightSources = uv2.z >= 1.0 ? vec2(uv2.y, uv2.z - 1.0) : vec2(-1.0);
    waterDistance = length(viewPosition);
    waterSurfaceData = uv1;
    waterSurfaceDrift = uv0;
}
