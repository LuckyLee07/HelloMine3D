#version 150

in vec2 boundaryUV;
uniform sampler2D caveBoundaryMask;
out vec4 fragColour;

void main()
{
    if (texture(caveBoundaryMask, boundaryUV).r < 0.5)
        discard;
    // The same unlit underground background used by the terrain fog.
    fragColour = vec4(0.035, 0.043, 0.054, 1.0);
}
