#version 150

out vec4 fragmentColour;

void main()
{
    fragmentColour = vec4(gl_FragCoord.zzz, 1.0);
}
