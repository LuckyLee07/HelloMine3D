#version 150

in vec4 vertex;
in vec2 uv0;

out vec2 postUv;

void main()
{
    gl_Position = vertex;
    postUv = uv0;
}
