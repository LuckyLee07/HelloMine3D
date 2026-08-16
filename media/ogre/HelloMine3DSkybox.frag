#version 150

in vec3 vDirection;

uniform vec3 skyZenithColour;
uniform vec3 skyHorizonColour;
uniform vec3 sunDirection;
uniform vec3 sunColour;
uniform float sunIntensity;
uniform float moonIntensity;
uniform float starIntensity;

out vec4 fragColor;

float hash31(vec3 value)
{
    value = fract(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
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

    vec3 starCell = floor(direction * 420.0);
    float starNoise = hash31(starCell);
    float stars = smoothstep(0.996, 1.0, starNoise) *
                  smoothstep(-0.02, 0.18, direction.y) *
                  starIntensity;
    colour += vec3(0.68, 0.78, 1.0) * stars;

    fragColor = vec4(max(colour, vec3(0.0)), 1.0);
}
