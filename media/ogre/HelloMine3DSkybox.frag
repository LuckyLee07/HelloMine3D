#version 150

in vec3 vDirection;

uniform vec3 skyZenithColour;
uniform vec3 skyHorizonColour;
uniform vec3 sunDirection;
uniform vec3 sunColour;
uniform float sunIntensity;
uniform float moonIntensity;
uniform float starIntensity;
uniform vec3 cloudLightColour;
uniform vec3 cloudShadowColour;
uniform float cloudCoverage;
uniform float globalTime;

out vec4 fragColor;

float hash31(vec3 value)
{
    value = fract(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
}

float hash21(vec2 value)
{
    vec3 projected = fract(vec3(value.xyx) * 0.1031);
    projected += dot(projected, projected.yzx + 33.33);
    return fract((projected.x + projected.y) * projected.z);
}

float valueNoise(vec2 value)
{
    vec2 cell = floor(value);
    vec2 blend = fract(value);
    blend = blend * blend * (3.0 - 2.0 * blend);
    float a = hash21(cell);
    float b = hash21(cell + vec2(1.0, 0.0));
    float c = hash21(cell + vec2(0.0, 1.0));
    float d = hash21(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

float cloudNoise(vec2 value)
{
    float result = valueNoise(value) * 0.58;
    result += valueNoise(value * 2.03 + 19.7) * 0.28;
    result += valueNoise(value * 4.07 - 7.3) * 0.14;
    return result;
}

void main()
{
    vec3 direction = normalize(vDirection);
    float upperSky = smoothstep(0.0, 0.78, max(direction.y, 0.0));
    vec3 colour = mix(skyHorizonColour, skyZenithColour, upperSky);

    // Keep the lower hemisphere on the exact fog colour so distant terrain
    // fades into the sky without a hard horizon band.
    float horizonFade = smoothstep(-0.24, 0.04, direction.y);
    colour = mix(skyHorizonColour, colour, horizonFade);

    vec3 starCell = floor(direction * 420.0);
    float starNoise = hash31(starCell);
    float stars = smoothstep(0.996, 1.0, starNoise) *
                  smoothstep(-0.02, 0.18, direction.y) *
                  starIntensity;
    colour += vec3(0.68, 0.78, 1.0) * stars;

    // Project a moving two-dimensional field onto the upper hemisphere.
    // Broad, low-octave shapes keep the result readable as stylised voxel
    // clouds instead of introducing a heavy volumetric weather pass.
    float cloudHorizonFade = smoothstep(0.025, 0.16, direction.y);
    vec2 cloudUv = direction.xz / max(0.20, direction.y + 0.24);
    cloudUv = cloudUv * 2.7 + vec2(globalTime * 0.006,
                                   globalTime * 0.0025);
    float clouds = cloudNoise(cloudUv);
    float cloudThreshold = mix(0.70, 0.46, cloudCoverage);
    float cloudMask = smoothstep(cloudThreshold,
                                 cloudThreshold + 0.13, clouds) *
                      cloudHorizonFade;
    float cloudBody = smoothstep(cloudThreshold - 0.12,
                                 cloudThreshold + 0.12, clouds);
    vec3 cloudColour = mix(cloudShadowColour, cloudLightColour,
                           0.30 + cloudBody * 0.70);
    colour = mix(colour, cloudColour, cloudMask * 0.82);

    float sunAlignment = dot(direction, normalize(sunDirection));
    float sunHalo = smoothstep(0.965, 0.9992, sunAlignment);
    float sunDisc = smoothstep(0.9988, 0.99975, sunAlignment);
    colour += sunColour * sunIntensity *
              (sunHalo * 0.22 + sunDisc * 1.35);

    vec3 moonDirection = -normalize(sunDirection);
    float moonAlignment = dot(direction, moonDirection);
    float moonHalo = smoothstep(0.982, 0.9993, moonAlignment);
    float moonDisc = smoothstep(0.9990, 0.99972, moonAlignment);
    vec3 moonColour = vec3(0.62, 0.72, 0.92);
    colour += moonColour * moonIntensity *
              (moonHalo * 0.12 + moonDisc * 0.9);

    fragColor = vec4(clamp(colour, vec3(0.0), vec3(1.35)), 1.0);
}
