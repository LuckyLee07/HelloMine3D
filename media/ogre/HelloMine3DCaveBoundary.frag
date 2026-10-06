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
uniform sampler2D caveBoundaryMask;
out vec4 fragColour;

void main()
{
    if (texture(caveBoundaryMask, boundaryUV).r < 0.5)
        discard;
    // The same unlit underground background used by the terrain fog.
    fragColour = vec4(sceneColour(vec3(0.035, 0.043, 0.054)), 1.0);
}
