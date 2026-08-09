#version 150

in vec2 terrainUv;
in float terrainLight;

out vec4 fragmentColour;

uniform sampler2D terrainAtlas;

void main()
{
    fragmentColour = texture(terrainAtlas, terrainUv) * terrainLight;
    if (fragmentColour.a == 0.0)
    {
        discard;
    }
}
