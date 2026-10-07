#version 150

// The legacy branch keeps authored display colours untouched. HDR scene
// shaders decode colour inputs before lighting/blending; alpha/data stay raw.
uniform float linearHdrMode;
vec3 sceneColour(vec3 authored)
{
    if (linearHdrMode < 0.5) return authored;
    vec3 c = max(authored, vec3(0.0));
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), c));
}


in vec2 boundaryUV;
in vec3 boundaryWorldPosition;
uniform vec2 viewRange;
uniform vec2 viewRangeCentre;
uniform float viewRangeStrength;
uniform vec3 fogColour;
uniform sampler2D caveBoundaryMask;
out vec4 fragColour;

float boundaryCoverage(vec3 worldPosition)
{
    if (viewRangeStrength <= 0.0 || viewRange.y <= viewRange.x) return 1.0;
    vec2 distance = abs(worldPosition.xz - viewRangeCentre);
    float edgeDistance = max(distance.x, distance.y);
    float coverage = 1.0 - smoothstep(viewRange.x, viewRange.y, edgeDistance);
    return mix(1.0, coverage, clamp(viewRangeStrength, 0.0, 1.0));
}

void main()
{
    if (texture(caveBoundaryMask, boundaryUV).r < 0.5)
        discard;
    // Retire only the background representation alongside ordinary geometry.
    // The existing dark-air mask, demand and underground colour stay intact.
    float coverage = boundaryCoverage(boundaryWorldPosition);
    if (coverage <= 0.0) discard;
    vec3 colour = sceneColour(vec3(0.035, 0.043, 0.054));
    if (coverage < 1.0)
        colour = mix(sceneColour(fogColour), colour, coverage);
    fragColour = vec4(colour, 1.0);
}
