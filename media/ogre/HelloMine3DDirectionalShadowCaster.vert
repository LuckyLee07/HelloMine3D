#version 150

in vec4 vertex;
in float uv3;

flat out vec3 casterNaturalTreeRoot;
uniform mat4 worldViewProj;
uniform mat4 world;

void main()
{
    gl_Position = worldViewProj * vertex;
    float rootCode = max(0.0, uv3 - 1.0);
    vec2 localRoot = vec2(floor(rootCode / 32.0), mod(rootCode, 32.0)) - 6.0;
    casterNaturalTreeRoot = vec3((world * vec4(localRoot.x, 0.0, localRoot.y, 1.0)).xz, uv3);
}
