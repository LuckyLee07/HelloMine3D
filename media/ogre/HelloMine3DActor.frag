#version 150

in float actorDistance;
in vec3 actorWorldPosition;
in vec3 actorLocalPosition;

out vec4 fragmentColour;

uniform vec4 actorTint;
uniform vec4 actorPartData;
uniform float actorSurfaceStrength;
uniform float environmentLight;
uniform vec3 fogColour;
uniform vec3 fogSunwardColour;
uniform vec3 sunDirection;
uniform float fogDirectionalStrength;
uniform float fogDensity;
uniform vec3 cameraPosition;

vec3 directionalFogColour(vec3 viewDirection)
{
    float directionLength = length(viewDirection);
    if (directionLength < 0.00001)
    {
        return fogColour;
    }
    vec3 normalisedView = viewDirection / directionLength;
    vec2 viewHorizontal = normalisedView.xz;
    vec2 sunHorizontal = sunDirection.xz;
    float viewLength = length(viewHorizontal);
    float sunLength = length(sunHorizontal);
    if (viewLength < 0.00001 || sunLength < 0.00001)
    {
        return fogColour;
    }
    float horizonAmount = 1.0 - smoothstep(
        0.12, 0.65, abs(normalisedView.y));
    float alignment = max(dot(viewHorizontal / viewLength,
                              sunHorizontal / sunLength), 0.0);
    float amount = clamp(fogDirectionalStrength * horizonAmount *
                         alignment * alignment * alignment, 0.0, 1.0);
    return mix(fogColour, fogSunwardColour, amount);
}

// Derived solely from the copied pose/profile: role, guardian, windup, archetype.
// Local -Z is the front. Broad material zones survive distance; sub-face texels
// fade with their screen footprint instead of crawling across moving limbs.
float actorPatch(vec2 point, vec2 centre, vec2 halfSize)
{
    vec2 edge = max(fwidth(point), vec2(0.002));
    vec2 coverage = 1.0 - smoothstep(halfSize - edge, halfSize + edge, abs(point - centre));
    return coverage.x * coverage.y;
}

float actorEyeMask()
{
    return step(actorLocalPosition.z, -0.499) * actorPatch(
        vec2(abs(actorLocalPosition.x), actorLocalPosition.y), vec2(0.23, 0.05), vec2(0.10, 0.055));
}

float actorCrestMark()
{
    vec2 p = actorLocalPosition.xy;
    return max(actorPatch(p, vec2(0.0), vec2(0.10, 0.34)),
               actorPatch(p, vec2(0.0, 0.14), vec2(0.29, 0.075)));
}

vec3 readableActorSurface()
{
    if (actorSurfaceStrength < 0.5) return actorTint.rgb;
    vec3 faceNormal = normalize(cross(dFdx(actorWorldPosition), dFdy(actorWorldPosition)));
    float shade = 0.68 + 0.22 * max(faceNormal.y, 0.0) +
                  0.10 * max(dot(faceNormal, normalize(vec3(-0.4, 0.6, -0.5))), 0.0);
    float role = actorPartData.x;
    if (actorPartData.w > 9.5) {
        // Wildlife uses the same matte voxel lighting as other actors. Broad
        // part colours and a few fixed pixel marks distinguish the silhouettes.
        vec3 p = actorLocalPosition;
        float front = step(p.z, -0.499);
        vec3 base = actorTint.rgb;
        float species = actorPartData.w;
        if (species < 10.5) {
            if (role > 3.5 && role < 4.5) base *= 0.60; // hooves
            if (role > 2.5 && role < 3.5) base = vec3(0.49, 0.43, 0.36);
            if (role > 1.5 && role < 2.5) base *= 0.88;
            if (role > 0.5 && role < 1.5) {
                vec3 cell = floor((p + 0.5) * 5.0);
                float patch = step(0.56, fract(dot(cell, vec3(0.31, 0.17, 0.47))));
                base *= mix(0.96, 1.035, patch);
            }
        }
        else if (species < 11.5) {
            if (role > 3.5 && role < 4.5) base *= 0.79;
            if (role > 4.5 && role < 5.5) {
                float inner = front * actorPatch(p.xy, vec2(0.0, 0.02),
                    vec2(0.24, 0.38));
                base = mix(base * 0.91, vec3(0.69, 0.51, 0.49), inner);
            }
            if (role > 5.5 && role < 6.5) base = vec3(0.75, 0.69, 0.58);
        }
        else {
            if (role > 3.5 && role < 4.5) base = vec3(0.46, 0.37, 0.27);
            if (role > 6.5 && role < 7.5) base = vec3(0.77, 0.61, 0.33);
            if (role > 7.5 && role < 8.5) base *= 0.72;
            if (role > 0.5 && role < 1.5) {
                float side = step(0.42, abs(p.x));
                base = mix(base, base * 0.87, side);
            }
        }
        if (role > 1.5 && role < 2.5) {
            float eye = front * actorPatch(
                vec2(abs(p.x), p.y), vec2(0.28, 0.08),
                vec2(0.075, 0.075));
            base = mix(base, vec3(0.15, 0.18, 0.17), eye);
        }
        return base * shade;
    }
    if (role > 9.5 && role < 10.5) {
        // Compact, faceted spit: a pale leading end and a dark tapered tail.
        // Attached to local geometry; no pulsing, transparency or extra glow.
        float leading = 1.0 - smoothstep(-0.32, 0.20, actorLocalPosition.z);
        vec3 tip = mix(actorTint.rgb, vec3(0.97, 0.87, 0.80), 0.55);
        return mix(actorTint.rgb * 0.52, tip, leading) * shade;
    }
    float archetype = actorPartData.w;
    float front = step(actorLocalPosition.z, -0.499);
    vec3 p = actorLocalPosition;
    vec3 base = actorTint.rgb;
    if (role > 4.5 && role < 6.5) base *= 0.60;
    if (role > 2.5 && role < 4.5) base *= 0.84;
    if (role > 0.5 && role < 1.5) {
        float belt = 1.0 - step(0.075, abs(p.y + 0.28));
        base = mix(base, base * 0.48, belt);
        if (archetype > 0.5 && archetype < 1.5) {
            float vest = front * actorPatch(p.xy, vec2(0.0, 0.10), vec2(0.31, 0.32));
            base = mix(base, actorTint.rgb * 1.18, vest);
            float straps = front * actorPatch(vec2(abs(p.x), p.y), vec2(0.30, 0.13), vec2(0.045, 0.35));
            base = mix(base, vec3(0.22, 0.25, 0.24), straps);
            float fastening = front * actorPatch(p.xy, vec2(0.0, -0.27), vec2(0.10, 0.06));
            base = mix(base, vec3(0.50, 0.46, 0.32), fastening);
        }
        else if (archetype > 1.5 && archetype < 2.5) {
            float plate = front * actorPatch(p.xy, vec2(0.0, 0.14), vec2(0.37, 0.27));
            base = mix(base, vec3(0.46, 0.37, 0.28), plate);
            float seam = front * actorPatch(p.xy, vec2(0.0, 0.14), vec2(0.025, 0.27));
            base = mix(base, actorTint.rgb * 0.53, seam);
            float studs = front * actorPatch(vec2(abs(p.x), abs(p.y - 0.14)), vec2(0.29, 0.18), vec2(0.035));
            base = mix(base, vec3(0.65, 0.56, 0.39), studs);
        }
        else if (archetype > 2.5) {
            float belly = 1.0 - smoothstep(-0.16, 0.05, p.y);
            base = mix(base, vec3(0.47, 0.43, 0.47), belly * 0.75);
            float back = smoothstep(0.20, 0.30, p.y);
            float ridges = 0.78 + 0.16 * step(0.45, fract((p.z + 0.5) * 4.0));
            base = mix(base, actorTint.rgb * ridges * 0.68, back);
        }
        if (actorPartData.y > 0.5) {
            float sigil = front * actorPatch(p.xy, vec2(0.0, 0.16), vec2(0.045, 0.16));
            base = mix(base, vec3(0.27, 0.54, 0.53), sigil);
        }
    }
    if (role > 1.5 && role < 2.5) {
        base = mix(base, vec3(0.62, 0.65, 0.58), 0.34);
        if (archetype > 0.5 && archetype < 1.5) {
            float cheek = front * actorPatch(p.xy, vec2(0.0, -0.20), vec2(0.34, 0.18));
            base = mix(base, vec3(0.49, 0.54, 0.49), cheek);
            float mouth = front * actorPatch(p.xy, vec2(0.0, -0.22), vec2(0.17, 0.026));
            base = mix(base, vec3(0.18, 0.23, 0.23), mouth);
        }
        else if (archetype > 1.5 && archetype < 2.5) {
            float jaw = front * actorPatch(p.xy, vec2(0.0, -0.23), vec2(0.40, 0.20));
            base = mix(base, vec3(0.60, 0.50, 0.37), jaw);
            float nose = front * actorPatch(p.xy, vec2(0.0, -0.02), vec2(0.085, 0.12));
            base = mix(base, vec3(0.35, 0.28, 0.22), nose);
            float mouth = front * actorPatch(p.xy, vec2(0.0, -0.24), vec2(0.25, 0.04));
            base = mix(base, vec3(0.23, 0.18, 0.15), mouth);
        }
        else if (archetype > 2.5) {
            float temple = front * actorPatch(vec2(abs(p.x), p.y), vec2(0.38, -0.09), vec2(0.08, 0.32));
            base = mix(base, vec3(0.47, 0.43, 0.49), temple);
        }
        float brow = front * actorPatch(p.xy, vec2(0.0, 0.18), vec2(0.43, 0.08));
        base = mix(base, base * 0.43, brow);
        vec3 eyeColour = mix(vec3(0.90, 0.79, 0.47), vec3(1.0, 0.31, 0.12), actorPartData.z);
        return mix(base * shade, eyeColour, actorEyeMask());
    }
    if (archetype > 0.5 && role > 2.5 && role < 6.5) {
        float terminal = 1.0 - smoothstep(-0.27, -0.22, p.y);
        vec3 endColour = archetype > 2.5 ? vec3(0.24, 0.22, 0.29) :
            (archetype > 1.5 ? vec3(0.34, 0.25, 0.19) : vec3(0.19, 0.25, 0.26));
        base = mix(base, endColour, terminal);
        float cuff = 1.0 - smoothstep(0.035, 0.065, abs(p.y + 0.19));
        base = mix(base, mix(actorTint.rgb, vec3(0.57, 0.54, 0.43), 0.35), cuff);
    }
    if (role > 6.5 && role < 7.5 && archetype > 2.5) {
        base = mix(actorTint.rgb, vec3(0.49, 0.44, 0.46), 0.45);
        float nose = front * actorPatch(p.xy, vec2(0.0, 0.12), vec2(0.38, 0.15));
        float mouth = front * actorPatch(p.xy, vec2(0.0, -0.16), vec2(0.41, 0.045));
        base = mix(base, vec3(0.18, 0.17, 0.22), max(nose, mouth));
    }
    if (role > 7.5 && actorPartData.y > 0.5) {
        vec3 core = mix(vec3(0.18, 0.58, 0.64), vec3(0.61, 0.90, 0.88), actorPartData.z);
        return mix(core * 0.40 * shade, core, actorCrestMark());
    }
    if (archetype > 0.5) {
        vec3 cell = floor((p + 0.5) * 12.0);
        float grain = fract(dot(cell, vec3(0.37, 0.61, 0.23)));
        float footprint = max(max(fwidth(p.x), fwidth(p.y)), fwidth(p.z));
        float detail = 1.0 - smoothstep(0.025, 0.10, footprint);
        base *= 1.0 + (grain - 0.5) * 0.07 * detail;
    }
    return base * shade;
}

// Only eyes and the guardian rune have a bounded light floor. Other material
// detail follows exposure and shadow; all parts keep fog and depth occlusion.
vec3 actorCueEmission()
{
    if (actorSurfaceStrength < 0.5) return vec3(0.0);
    if (actorPartData.x > 1.5 && actorPartData.x < 2.5)
        return mix(vec3(0.90, 0.79, 0.47), vec3(1.0, 0.31, 0.12), actorPartData.z) * actorEyeMask() * 0.64;
    if (actorPartData.x > 7.5 && actorPartData.y > 0.5)
        return vec3(0.18, 0.58, 0.64) * actorCrestMark() * 0.38;
    return vec3(0.0);
}

void main()
{
    float environmentExposure = mix(
        0.34, 1.0, clamp(environmentLight, 0.0, 1.0));
    float fogVisibility = clamp(
        exp(-actorDistance * actorDistance * fogDensity * fogDensity),
        0.0, 1.0);
    vec3 localFogColour = directionalFogColour(
        actorWorldPosition - cameraPosition);
    fragmentColour = vec4(
        mix(localFogColour, max(readableActorSurface() * environmentExposure, actorCueEmission()),
            fogVisibility),
        actorTint.a);
}
