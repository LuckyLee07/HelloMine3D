#version 150

in vec2 postUv;

uniform sampler2D sceneTexture;
uniform vec4 inverseTextureSize;
uniform float bloomThreshold;
uniform float bloomStrength;
uniform float toneStrength;
uniform float ditherStrength;
uniform float fixtureMode;

out vec4 fragmentColour;

vec3 fixtureSignal(vec2 uv)
{
    float stepIndex = floor(clamp(uv.x, 0.0, 0.9999) * 16.0);
    float normalized = stepIndex / 15.0;
    if (uv.y < 0.3333)
    {
        normalized *= 0.20;
    }
    else if (uv.y > 0.6667)
    {
        normalized = 0.72 + normalized * 0.28;
    }
    return vec3(normalized);
}

vec4 sampleSource(vec2 uv)
{
    return fixtureMode > 0.5
        ? vec4(fixtureSignal(uv), 1.0)
        : texture(sceneTexture, uv);
}

vec3 bloomSample(vec2 offset)
{
    vec3 sampleColour = sampleSource(
        postUv + offset * inverseTextureSize.xy).rgb;
    float luminance = dot(sampleColour, vec3(0.2126, 0.7152, 0.0722));
    float contribution = smoothstep(bloomThreshold, 1.0, luminance);
    return sampleColour * contribution;
}

void main()
{
    vec4 source = sampleSource(postUv);
    vec3 bloom = bloomSample(vec2(-1.5, 0.0)) +
                 bloomSample(vec2(1.5, 0.0)) +
                 bloomSample(vec2(0.0, -1.5)) +
                 bloomSample(vec2(0.0, 1.5)) +
                 bloomSample(vec2(-1.0, -1.0)) +
                 bloomSample(vec2(1.0, -1.0)) +
                 bloomSample(vec2(-1.0, 1.0)) +
                 bloomSample(vec2(1.0, 1.0));
    vec3 colour = clamp(
        source.rgb + bloom * (bloomStrength / 8.0) *
            (vec3(1.0) - source.rgb),
        0.0, 1.0);

    vec3 smoothContrast = colour * colour * (3.0 - 2.0 * colour);
    colour = mix(colour, smoothContrast, toneStrength);

    float noise = fract(52.9829189 * fract(dot(
        gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))) - 0.5;
    colour += noise * (ditherStrength / 255.0);
    fragmentColour = vec4(clamp(colour, 0.0, 1.0), source.a);
}
