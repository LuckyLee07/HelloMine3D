#version 150

in vec4 vertex;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
out vec2 boundaryUV;
out vec3 boundaryWorldPosition;

void main()
{
    gl_Position = worldViewProj * vertex;
    boundaryUV = uv0;
    boundaryWorldPosition = (world * vertex).xyz;
}
