#version 150

out vec4 fragmentColour;

uniform vec4 actorTint;

void main()
{
    fragmentColour = actorTint;
}
