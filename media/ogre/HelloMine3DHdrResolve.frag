#version 150
in vec2 postUv;
uniform sampler2D sceneTexture;
uniform float exposure;
uniform float spatialAaStrength;
uniform vec4 inverseTextureSize;
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
// A bounded spatial resolve in display space. At most nine scene samples,
// no history or additional target; the HUD is submitted after this compositor.
vec3 resolvedAt(vec2 uv)
{
    return displayEncode(toneMap(texture(sceneTexture, uv).rgb));
}
float luma(vec3 colour)
{
    return dot(colour, vec3(0.299, 0.587, 0.114));
}
vec3 spatialResolve(vec2 uv, vec3 centre)
{
    vec2 pixel = inverseTextureSize.xy;
    if (min(pixel.x, pixel.y) <= 0.0) return centre;
    vec3 nw = resolvedAt(uv + vec2(-1.0, -1.0) * pixel);
    vec3 ne = resolvedAt(uv + vec2( 1.0, -1.0) * pixel);
    vec3 sw = resolvedAt(uv + vec2(-1.0,  1.0) * pixel);
    vec3 se = resolvedAt(uv + vec2( 1.0,  1.0) * pixel);
    float middle = luma(centre);
    float lnw = luma(nw), lne = luma(ne), lsw = luma(sw), lse = luma(se);
    float low = min(middle, min(min(lnw, lne), min(lsw, lse)));
    float high = max(middle, max(max(lnw, lne), max(lsw, lse)));
    if (high - low < max(0.04, high * 0.125)) return centre;
    vec2 direction = vec2(-((lnw + lne) - (lsw + lse)),
                           (lnw + lsw) - (lne + lse));
    float reduction = max((lnw + lne + lsw + lse) * (0.25 * 0.125), 1.0 / 128.0);
    direction /= min(abs(direction.x), abs(direction.y)) + reduction;
    direction = clamp(direction, vec2(-4.0), vec2(4.0)) * pixel;
    vec3 narrow = 0.5 * (resolvedAt(uv - direction / 6.0) +
                        resolvedAt(uv + direction / 6.0));
    vec3 wide = 0.5 * narrow + 0.25 *
        (resolvedAt(uv - direction * 0.5) + resolvedAt(uv + direction * 0.5));
    float wideLuma = luma(wide);
    return (wideLuma < low || wideLuma > high) ? narrow : wide;
}
void main()
{
    vec4 scene = texture(sceneTexture, postUv);
    vec3 resolved = displayEncode(toneMap(scene.rgb));
    if (spatialAaStrength > 0.5) resolved = spatialResolve(postUv, resolved);
    fragmentColour = vec4(resolved, scene.a);
}
