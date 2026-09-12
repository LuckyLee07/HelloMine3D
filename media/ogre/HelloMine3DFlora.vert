#version 150

in vec4 vertex;
in vec2 uv0;
in vec2 uv1;
in float uv2;

out vec2 terrainTileUv;
out vec2 terrainRepeat;
out float terrainLight;
out float terrainDistance;
out vec3 terrainWorldPosition;

uniform mat4 worldViewProj;
uniform mat4 worldView;
uniform mat4 world;
uniform float globalTime;

vec2 floraWind(vec2 position, float time)
{
    // A smooth world-space field keeps nearby plants related without
    // repeating the same motion in every section. Gusts modulate the sway.
    float broadPhase = dot(position, vec2(0.041, 0.027));
    float gust = 0.5 + 0.5 * sin(time * 0.55 - broadPhase);
    // Keep quiet wind visible, with a readable ~2.9 second primary sway.
    float strength = mix(0.055, 0.085, gust * gust);
    float drift = 0.45 * sin(time * 0.35 - broadPhase);
    float sway = 0.65 * sin(time * 2.20 +
                           dot(position, vec2(0.18, 0.13)) + drift) +
                 0.35 * sin(time * 3.46 +
                            dot(position, vec2(-0.31, 0.21)));
    float crossSway = 0.25 * sin(time * 1.66 +
                                dot(position, vec2(0.23, -0.19)) + 0.7);
    return strength * (vec2(0.88, 0.48) * sway +
                       vec2(-0.48, 0.88) * crossSway);
}

void main()
{
    vec4 animatedVertex = vertex;
    vec3 baseWorldPosition = (world * vertex).xyz;
    // Cross shapes use repeat V=1 at their roots and V=0 at their tips.
    // Scale by physical height as well so young crops do not bend as far.
    float tipWeight = clamp(1.0 - uv1.y, 0.0, 1.0);
    float heightAboveRoot = baseWorldPosition.y -
        floor(baseWorldPosition.y - tipWeight * 0.001);
    float bendWeight = tipWeight * tipWeight * heightAboveRoot;
    animatedVertex.xz += floraWind(baseWorldPosition.xz, globalTime) *
                          bendWeight;

    gl_Position = worldViewProj * animatedVertex;
    terrainTileUv = uv0;
    terrainRepeat = uv1;
    terrainLight = uv2;
    terrainDistance = length((worldView * animatedVertex).xyz);
    terrainWorldPosition = (world * animatedVertex).xyz;
}
