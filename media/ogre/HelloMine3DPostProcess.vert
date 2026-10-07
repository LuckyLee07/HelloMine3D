#version 150

in vec4 vertex;
in vec2 uv0;

uniform mat4 worldViewProj;

out vec2 postUv;

void main()
{
    // Ogre folds the output target orientation into this identity quad matrix.
    // A resolve may write another compositor RTT instead of the final window.
    gl_Position = worldViewProj * vertex;
    postUv = uv0;
}
