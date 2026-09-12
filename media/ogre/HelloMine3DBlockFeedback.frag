#version 150

in vec2 terrainTileUv;
in vec2 terrainRepeat;
out vec4 fragmentColour;

#ifdef TERRAIN_ARRAY
uniform sampler2DArray terrainArray;
#else
uniform sampler2D terrainAtlas;
uniform float atlasPixels;
uniform float tilePixels;
#endif
uniform float tilesPerRow;
uniform float alphaCutoff;
uniform float highlightStrength;
uniform float crackStage;
uniform vec2 crackSeed;
uniform vec2 crackStretch;

vec2 fracturePoint(vec2 cell)
{
    vec2 value = vec2(dot(cell, vec2(127.1, 311.7)),
                      dot(cell, vec2(269.5, 183.3)));
    return 0.18 + 0.64 * fract(sin(value + crackSeed) * 43758.5453);
}

float fractureDistance(vec2 uv)
{
    // Stationary irregular cells form connected branches. Small spatial
    // distortion roughens the edges without animated noise or crawling.
    vec2 p = uv * crackStretch * 4.1;
    p += 0.045 * (abs(fract(vec2(p.y * 8.0 + p.x * 2.1,
                                 p.x * 9.0 - p.y * 1.7)) - 0.5) - 0.25);
    vec2 cell = floor(p);
    vec2 local = fract(p);
    float nearest = 100.0;
    float second = 100.0;
    for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x)
    {
        vec2 neighbour = vec2(float(x), float(y));
        vec2 delta = neighbour + fracturePoint(cell + neighbour) - local;
        float distance = dot(delta, delta);
        if (distance < nearest)
        {
            second = nearest;
            nearest = distance;
        }
        else second = min(second, distance);
    }
    return (sqrt(second) - sqrt(nearest)) / 4.1;
}

void main()
{
    vec2 tile = floor(terrainTileUv * tilesPerRow);
    vec2 uv = clamp(terrainRepeat, vec2(0.0), vec2(1.0));
#ifdef TERRAIN_ARRAY
    vec4 surface = textureGrad(terrainArray,
        vec3(fract(terrainRepeat), tile.y * tilesPerRow + tile.x),
        dFdx(terrainRepeat), dFdy(terrainRepeat));
#else
    vec2 pixel = vec2(0.5) + fract(terrainRepeat) * (tilePixels - 1.0);
    vec4 surface = texture(terrainAtlas, (tile * tilePixels + pixel) / atlasPixels);
#endif
    float distance = fractureDistance(uv);
    float antialias = max(fwidth(distance), 0.0007);
    float progress = clamp((crackStage + 1.0) / 10.0, 0.0, 1.0);
    float radius = 0.04 + progress * 0.91;
    float growth = 1.0 - smoothstep(radius - 0.07, radius,
                                    length((uv - vec2(0.46, 0.54)) * vec2(1.0, 0.92)));
    float width = mix(0.0012, 0.0065, progress * progress);
    float crack = (1.0 - smoothstep(width, width + antialias, distance)) * growth;
    float bevel = (1.0 - smoothstep(width + antialias, width + 0.0035 + antialias,
                                    distance)) * growth * (1.0 - crack);
    if (crackStage < 0.0) { crack = 0.0; bevel = 0.0; }
    if (surface.a <= alphaCutoff) discard;

    // A pale surface lift preserves the material and alpha silhouette. Dark
    // grooves plus a narrow chipped rim replace the former yellow wire rays.
    vec3 colour = mix(vec3(0.83, 0.91, 1.0), vec3(0.57, 0.53, 0.46), bevel * 0.65);
    colour = mix(colour, vec3(0.035, 0.028, 0.022), crack);
    float alpha = mix(highlightStrength + bevel * 0.18, 0.93, crack);
    fragmentColour = vec4(colour, alpha * surface.a);
}
