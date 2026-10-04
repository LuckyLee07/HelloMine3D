#version 150

flat in vec3 casterNaturalTreeRoot;
out vec4 fragmentColour;
uniform vec2 viewRange;
uniform vec2 viewRangeCentre;
uniform float viewRangeStrength;

void main()
{
    // Keep existing caster geometry for every non-tree object. A completely
    // retired natural tree must also stop contributing directional shadows.
    if (casterNaturalTreeRoot.z > 0.0 && viewRange.y > viewRange.x)
    {
        vec2 distance = abs(casterNaturalTreeRoot.xy - viewRangeCentre);
        float end = max(14.0, viewRange.y - 6.0);
        float start = max(13.0, max(end * 0.5, viewRange.x - 6.0));
        float coverage = 1.0 - smoothstep(start, end, max(distance.x, distance.y));
        if (mix(1.0, coverage, clamp(viewRangeStrength, 0.0, 1.0)) <= 0.0) discard;
    }
    fragmentColour = vec4(gl_FragCoord.zzz, 1.0);
}
