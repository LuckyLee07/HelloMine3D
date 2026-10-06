#version 150
in vec2 postUv;
uniform sampler2D sceneTexture;
uniform float exposure;
out vec4 fragmentColour;

vec3 displayEncode(vec3 linearColour)
{
    vec3 c = max(linearColour, vec3(0.0));
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), c));
}
vec3 toneMap(vec3 radiance)
{
    vec3 x = max(radiance * exposure, vec3(0.0));
    return clamp((x * (2.51 * x + 0.03)) /
                 (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main()
{
    vec4 scene = texture(sceneTexture, postUv);
    fragmentColour = vec4(displayEncode(toneMap(scene.rgb)), scene.a);
}
