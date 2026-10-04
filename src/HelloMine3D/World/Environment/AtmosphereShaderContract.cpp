#include "AtmosphereShaderContract.h"

#include "../../Util/ResourcePackResolver.h"

#include <fstream>
#include <cctype>
#include <initializer_list>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
std::string readText(const ResourcePackResolver &resolver,
                     const std::string &logicalPath)
{
    const std::string resolved = resolver.resolve(logicalPath);
    std::ifstream input(resolved, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "Unable to read V10C atmosphere shader '" + logicalPath +
            "' at '" + resolved + "'.");
    }
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

void requireTokens(const ResourcePackResolver &resolver,
                   const std::string &logicalPath,
                   std::initializer_list<const char *> tokens)
{
    const std::string source = readText(resolver, logicalPath);
    for (const char *token : tokens) {
        if (source.find(token) == std::string::npos) {
            throw std::runtime_error(
                "V10C atmosphere shader '" + logicalPath +
                "': missing interface declaration '" + token + "'.");
        }
    }
}

void requireProgramTokens(const ResourcePackResolver &resolver,
                          const char *kind, const char *program,
                          std::initializer_list<const char *> tokens)
{
    const std::string source = readText(
        resolver, "media/ogre/HelloMine3D.program");
    const std::string declaration =
        std::string(kind) + " " + program + " glsl";
    const std::size_t start = source.find(declaration);
    const std::size_t open = start == std::string::npos
        ? std::string::npos : source.find('{', start + declaration.size());
    std::size_t end = open;
    int depth = 0;
    if (open != std::string::npos) {
        do {
            if (source[end] == '{') ++depth;
            else if (source[end] == '}') --depth;
            ++end;
        } while (end < source.size() && depth > 0);
    }
    const std::string body = open != std::string::npos && depth == 0
        ? source.substr(open, end - open) : "";
    for (const char *token : tokens) {
        if (body.find(token) == std::string::npos) {
            throw std::runtime_error(
                std::string("Natural tree shader '") + program +
                "': missing interface declaration '" + token + "'.");
        }
    }
}

void requirePlayerExposureDefaults(
    const ResourcePackResolver &resolver,
    std::initializer_list<const char *> programs,
    bool requireShadowProjection = false)
{
    const std::string source = readText(
        resolver, "media/ogre/HelloMine3D.program");
    for (const char *program : programs) {
        const std::string declaration =
            std::string("fragment_program ") + program + " glsl";
        const std::size_t start = source.find(declaration);
        const std::size_t open = start == std::string::npos
            ? std::string::npos : source.find('{', start + declaration.size());
        std::size_t end = open;
        int depth = 0;
        if (open != std::string::npos) {
            do {
                if (source[end] == '{') ++depth;
                else if (source[end] == '}') --depth;
                ++end;
            } while (end < source.size() && depth > 0);
        }
        bool found = false;
        if (open != std::string::npos && depth == 0) {
            std::istringstream tokens(source.substr(open, end - open));
            std::string token;
            while (tokens >> token) {
                if (token != "param_named") continue;
                std::string name;
                std::string type;
                tokens >> name >> type;
                if (name == "playerExposure") {
                    float value = 0.f;
                    found = type == "float" && (tokens >> value) && value == -1.f;
                    break;
                }
            }
        }
        if (!found) {
            throw std::runtime_error(
                std::string("Player lighting shader '") + program +
                "': missing interface declaration 'param_named playerExposure float -1'.");
        }
        if (requireShadowProjection) {
            std::istringstream tokens(source.substr(open, end - open));
            std::string token;
            bool projectionFound = false;
            while (tokens >> token) {
                if (token != "param_named_auto") continue;
                std::string name, binding;
                tokens >> name >> binding;
                if (name == "directionalShadowViewProj") {
                    int index = -1;
                    projectionFound = binding == "texture_viewproj_matrix" &&
                        (tokens >> index) && index == 0;
                    break;
                }
            }
            if (!projectionFound) {
                throw std::runtime_error(std::string("Directional shadow shader '") + program +
                    "': missing interface declaration 'param_named_auto directionalShadowViewProj texture_viewproj_matrix 0'.");
            }
        }
    }
}
}

void validateAtmosphereShaderContract(
    const ResourcePackResolver &resolver)
{
    requirePlayerExposureDefaults(resolver,
        {"HelloMine3D/ActorFragment", "HelloMine3D/TerrainFragment",
         "HelloMine3D/TerrainArrayFragment"});
    requireTokens(resolver, "media/ogre/HelloMine3D.material",
        {"material HelloMine3D/PlayerHeld : HelloMine3D/Terrain",
         "material HelloMine3D/PlayerHeldTransparent : HelloMine3D/Transparent"});
    requireTokens(
        resolver, "media/ogre/HelloMine3D.program",
        {"param_named_auto actorPartData custom 1",
         "param_named actorSurfaceStrength float",
         "param_named surfaceLightingStrength float",
         "param_named waterDetailStrength float",
         "param_named fogSunwardColour float3",
         "param_named fogDirectionalStrength float",
         "param_named cloudLayerEnabled float",
         "param_named cloudBaseHeight float",
         "param_named cloudThickness float",
         "param_named cloudHorizontalScale float",
         "param_named cloudVelocity float2",
         "param_named cloudMaxDistance float",
         "param_named_auto cameraPosition camera_position",
         "param_named_auto globalTime time 1.0",
         "param_named_auto legacyTime time_0_x 1.0"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DSkybox.frag",
        {"uniform vec3 fogSunwardColour;",
         "uniform float fogDirectionalStrength;",
         "uniform float cloudLayerEnabled;",
         "uniform float cloudBaseHeight;",
         "uniform float cloudThickness;",
         "uniform float cloudHorizontalScale;",
         "uniform vec2 cloudVelocity;",
         "uniform float cloudMaxDistance;",
         "uniform vec3 cameraPosition;",
         "uniform float globalTime;",
         "uniform float legacyTime;",
         "void sampleLegacyClouds",
         "void sampleBoundedCloudLayer"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DTerrain.vert",
        {"out vec3 terrainWorldPosition;", "uniform mat4 world;",
         "in float uv3;", "flat out vec3 terrainNaturalTreeRoot;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DFlora.vert",
        {"out vec3 terrainWorldPosition;", "uniform mat4 world;",
         "in float uv3;", "flat out vec3 terrainNaturalTreeRoot;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DTerrain.frag",
        {"uniform vec2 viewRange;",
         "uniform vec2 viewRangeCentre;",
         "uniform float viewRangeStrength;",
         "in vec3 terrainWorldPosition;",
         "flat in vec3 terrainNaturalTreeRoot;",
         "uniform float playerExposure;",
         "uniform vec3 sunColour;",
         "uniform float sunIntensity;",
         "uniform float surfaceLightingStrength;",
         "uniform vec3 fogSunwardColour;",
         "uniform vec3 sunDirection;",
         "uniform float fogDirectionalStrength;",
         "uniform vec3 cameraPosition;",
         "vec3 directionalFogColour"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DWater.vert",
        {"out vec2 waterSurfaceData;", "out vec2 waterSurfaceDrift;",
         "uniform float waterDetailStrength;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DWater.frag",
        {"uniform vec2 viewRange;",
         "uniform vec2 viewRangeCentre;",
         "uniform float viewRangeStrength;",
         "uniform vec3 fogSunwardColour;",
         "in vec2 waterSurfaceData;",
         "in vec2 waterSurfaceDrift;",
         "uniform float waterDetailStrength;",
         "uniform float globalTime;",
         "uniform float fogDirectionalStrength;",
         "vec3 directionalFogColour"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActor.vert",
        {"out vec3 actorWorldPosition;", "out vec3 actorLocalPosition;", "uniform mat4 world;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActor.frag",
        {"uniform vec2 viewRange;",
         "uniform vec2 viewRangeCentre;",
         "uniform float viewRangeStrength;",
         "in vec3 actorWorldPosition;",
         "in vec3 actorLocalPosition;",
         "uniform float playerExposure;",
         "uniform vec4 actorPartData;",
         "uniform float actorSurfaceStrength;",
         "uniform vec3 fogSunwardColour;",
         "uniform vec3 sunDirection;",
         "uniform float fogDirectionalStrength;",
         "uniform vec3 cameraPosition;",
         "vec3 directionalFogColour"});
}

void validateDirectionalShadowShaderContract(
    const ResourcePackResolver &resolver)
{
    requirePlayerExposureDefaults(resolver,
        {"HelloMine3D/ActorShadowFragment", "HelloMine3D/TerrainShadowFragment",
         "HelloMine3D/TerrainShadowArrayFragment"}, true);
    requireTokens(
        resolver, "media/ogre/HelloMine3D.program",
        {"HelloMine3D/TerrainShadowVertex",
         "HelloMine3D/TerrainShadowFragment",
         "HelloMine3D/ActorShadowVertex",
         "HelloMine3D/ActorShadowFragment",
         "param_named_auto shadowWorldViewProj texture_worldviewproj_matrix 0",
         "param_named directionalShadowMap int 1",
         "param_named directionalShadowEnabled float",
         "param_named directionalShadowBias float",
         "param_named directionalShadowStrength float",
         "HelloMine3D/DirectionalShadowCasterVertex",
         "HelloMine3D/DirectionalShadowCasterFragment"});
    requireTokens(
        resolver, "media/ogre/HelloMine3D.material",
        {"material HelloMine3D/DirectionalShadowCaster",
         "material HelloMine3D/PlayerHeld : HelloMine3D/Terrain",
         "material HelloMine3D/PlayerHeldTransparent : HelloMine3D/Transparent",
         "vertex_program_ref HelloMine3D/DirectionalShadowCasterVertex",
         "fragment_program_ref HelloMine3D/DirectionalShadowCasterFragment"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DTerrainShadow.vert",
        {"out vec4 terrainShadowPosition;",
         "uniform mat4 shadowWorldViewProj;",
         "in float uv3;", "flat out vec3 terrainNaturalTreeRoot;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DFloraShadow.vert",
        {"in float uv3;", "flat out vec3 terrainNaturalTreeRoot;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DTerrainShadow.frag",
        {"uniform vec2 viewRange;",
         "uniform vec2 viewRangeCentre;",
         "uniform float viewRangeStrength;",
         "in vec4 terrainShadowPosition;",
         "flat in vec3 terrainNaturalTreeRoot;",
         "uniform float playerExposure;",
         "uniform vec3 sunColour;",
         "uniform float sunIntensity;",
         "uniform float surfaceLightingStrength;",
         "uniform sampler2D directionalShadowMap;",
         "uniform mat4 directionalShadowViewProj;",
         "uniform float directionalShadowBias;",
         "float directionalShadowVisibility()",
         "projected.z = projected.z * 0.5 + 0.5;",
         "vec2 base = floor(samplePosition);",
         "pcfVisibility += visible * weight.x * weight.y;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActorShadow.vert",
        {"out vec4 actorShadowPosition;", "out vec3 actorLocalPosition;",
         "uniform mat4 shadowWorldViewProj;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActorShadow.frag",
        {"uniform vec2 viewRange;",
         "uniform vec2 viewRangeCentre;",
         "uniform float viewRangeStrength;",
         "in vec4 actorShadowPosition;",
         "in vec3 actorLocalPosition;",
         "uniform float playerExposure;",
         "uniform vec4 actorPartData;",
         "uniform float actorSurfaceStrength;",
         "uniform sampler2D directionalShadowMap;",
         "uniform mat4 directionalShadowViewProj;",
         "float directionalShadowVisibility()",
         "projected.z = projected.z * 0.5 + 0.5;",
         "vec2 base = floor(samplePosition);",
         "pcfVisibility += visible * weight.x * weight.y;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DDirectionalShadowCaster.vert",
        {"uniform mat4 worldViewProj;",
         "uniform mat4 world;", "in float uv3;",
         "flat out vec3 casterNaturalTreeRoot;",
         "gl_Position = worldViewProj * vertex;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DDirectionalShadowCaster.frag",
        {"gl_FragCoord.zzz", "out vec4 fragmentColour;",
         "flat in vec3 casterNaturalTreeRoot;",
         "uniform vec2 viewRange;", "uniform vec2 viewRangeCentre;",
         "uniform float viewRangeStrength;"});
    requireProgramTokens(resolver, "vertex_program",
        "HelloMine3D/DirectionalShadowCasterVertex",
        {"param_named_auto world world_matrix"});
    requireProgramTokens(resolver, "fragment_program",
        "HelloMine3D/DirectionalShadowCasterFragment",
        {"param_named viewRange float2",
         "param_named viewRangeCentre float2",
         "param_named viewRangeStrength float"});
}

void validatePostProcessingShaderContract(
    const ResourcePackResolver &resolver)
{
    requireTokens(
        resolver, "media/ogre/HelloMine3D.compositor",
        {"compositor HelloMine3D/PostProcess",
         "texture scene target_width target_height PF_A8R8G8B8",
         "pass render_quad",
         "material HelloMine3D/PostProcess",
         "input 0 scene"});
    requireTokens(
        resolver, "media/ogre/HelloMine3D.program",
        {"HelloMine3D/PostProcessVertex",
         "source HelloMine3DPostProcess.vert",
         "HelloMine3D/PostProcessFragment",
         "source HelloMine3DPostProcess.frag",
         "param_named sceneTexture int 0",
         "param_named_auto inverseTextureSize inverse_texture_size 0",
         "param_named bloomThreshold float",
         "param_named bloomStrength float",
         "param_named toneStrength float",
         "param_named ditherStrength float",
         "param_named fixtureMode float"});
    requireTokens(
        resolver, "media/ogre/HelloMine3D.material",
        {"material HelloMine3D/PostProcess",
         "vertex_program_ref HelloMine3D/PostProcessVertex",
         "fragment_program_ref HelloMine3D/PostProcessFragment",
         "texture_unit sceneTexture",
         "filtering bilinear",
         "tex_address_mode clamp"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DPostProcess.vert",
        {"in vec4 vertex;", "in vec2 uv0;", "out vec2 postUv;",
         "gl_Position = vertex;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DPostProcess.frag",
        {"uniform sampler2D sceneTexture;",
         "uniform vec4 inverseTextureSize;",
         "uniform float bloomThreshold;",
         "uniform float bloomStrength;",
         "uniform float toneStrength;",
         "uniform float ditherStrength;",
         "uniform float fixtureMode;",
         "vec3 fixtureSignal(vec2 uv)",
         "vec4 sampleSource(vec2 uv)",
         "vec3 bloomSample(vec2 offset)",
         "smoothContrast",
         "gl_FragCoord.xy",
         "ditherStrength / 255.0"});
}

void validateCaveBoundaryShaderContract(
    const ResourcePackResolver &resolver)
{
    requireProgramTokens(resolver, "vertex_program",
        "HelloMine3D/CaveBoundaryVertex",
        {"source HelloMine3DCaveBoundary.vert", "syntax glsl150",
         "param_named_auto worldViewProj worldviewproj_matrix"});
    requireProgramTokens(resolver, "fragment_program",
        "HelloMine3D/CaveBoundaryFragment",
        {"source HelloMine3DCaveBoundary.frag", "syntax glsl150",
         "param_named caveBoundaryMask int 0"});
    requireTokens(resolver, "media/ogre/HelloMine3DCaveBoundary.vert",
        {"in vec4 vertex;", "in vec2 uv0;", "uniform mat4 worldViewProj;",
         "out vec2 boundaryUV;"});
    requireTokens(resolver, "media/ogre/HelloMine3DCaveBoundary.frag",
        {"in vec2 boundaryUV;", "uniform sampler2D caveBoundaryMask;",
         "out vec4 fragColour;"});

    const std::string material = readText(
        resolver, "media/ogre/HelloMine3D.material");
    const std::string declaration = "material HelloMine3D/CaveBoundary";
    auto start = material.find(declaration);
    while (start != std::string::npos) {
        const auto next = start + declaration.size();
        if (next == material.size() || material[next] == '{' ||
            material[next] == ':' ||
            std::isspace(static_cast<unsigned char>(material[next]))) break;
        start = material.find(declaration, next);
    }
    const auto open = start == std::string::npos ? std::string::npos :
        material.find('{', start + declaration.size());
    auto end = open;
    int depth = 0;
    if (open != std::string::npos) {
        do {
            if (material[end] == '{') ++depth;
            else if (material[end] == '}') --depth;
            ++end;
        } while (end < material.size() && depth > 0);
    }
    const std::string body = open != std::string::npos && depth == 0 ?
        material.substr(open, end - open) : "";
    const std::string code = std::regex_replace(body,
        std::regex(R"(/\*[\s\S]*?\*/|//[^\r\n]*)"), "");
    for (const char *token : {
             "receive_shadows off", "depth_check on", "depth_write off",
             "cull_hardware none", "lighting off",
             "vertex_program_ref HelloMine3D/CaveBoundaryVertex",
             "fragment_program_ref HelloMine3D/CaveBoundaryFragment",
             "texture_unit caveBoundaryMask", "filtering none"}) {
        if (code.find(token) == std::string::npos) {
            throw std::runtime_error(std::string(
                "Cave boundary material: missing interface declaration '") +
                token + "'.");
        }
    }
    // The named sampler is bound to texture unit zero. Extra techniques,
    // passes or units would make the source interface ambiguous.
    for (const char *word : {"technique", "pass", "texture_unit"}) {
        const std::regex token(std::string("\\b") + word + "\\b");
        if (std::distance(std::sregex_iterator(code.begin(), code.end(), token),
                          std::sregex_iterator()) != 1) {
            throw std::runtime_error(std::string(
                "Cave boundary material: requires exactly one ") + word + ".");
        }
    }
}
