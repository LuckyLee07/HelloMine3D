#include "AtmosphereShaderContract.h"

#include "../../Util/ResourcePackResolver.h"

#include <fstream>
#include <initializer_list>
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
}

void validateAtmosphereShaderContract(
    const ResourcePackResolver &resolver)
{
    requireTokens(
        resolver, "media/ogre/HelloMine3D.program",
        {"param_named fogSunwardColour float3",
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
        {"out vec3 terrainWorldPosition;", "uniform mat4 world;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DFlora.vert",
        {"out vec3 terrainWorldPosition;", "uniform mat4 world;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DTerrain.frag",
        {"in vec3 terrainWorldPosition;",
         "uniform vec3 fogSunwardColour;",
         "uniform vec3 sunDirection;",
         "uniform float fogDirectionalStrength;",
         "uniform vec3 cameraPosition;",
         "vec3 directionalFogColour"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DWater.frag",
        {"uniform vec3 fogSunwardColour;",
         "uniform float fogDirectionalStrength;",
         "vec3 directionalFogColour"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActor.vert",
        {"out vec3 actorWorldPosition;", "uniform mat4 world;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActor.frag",
        {"in vec3 actorWorldPosition;",
         "uniform vec3 fogSunwardColour;",
         "uniform vec3 sunDirection;",
         "uniform float fogDirectionalStrength;",
         "uniform vec3 cameraPosition;",
         "vec3 directionalFogColour"});
}

void validateDirectionalShadowShaderContract(
    const ResourcePackResolver &resolver)
{
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
         "vertex_program_ref HelloMine3D/DirectionalShadowCasterVertex",
         "fragment_program_ref HelloMine3D/DirectionalShadowCasterFragment"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DTerrainShadow.vert",
        {"out vec4 terrainShadowPosition;",
         "uniform mat4 shadowWorldViewProj;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DTerrainShadow.frag",
        {"in vec4 terrainShadowPosition;",
         "uniform sampler2D directionalShadowMap;",
         "uniform float directionalShadowBias;",
         "float directionalShadowVisibility()",
         "projected.z = projected.z * 0.5 + 0.5;",
         "litSamples / 4.0"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActorShadow.vert",
        {"out vec4 actorShadowPosition;",
         "uniform mat4 shadowWorldViewProj;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DActorShadow.frag",
        {"in vec4 actorShadowPosition;",
         "uniform sampler2D directionalShadowMap;",
         "float directionalShadowVisibility()",
         "projected.z = projected.z * 0.5 + 0.5;",
         "litSamples / 4.0"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DDirectionalShadowCaster.vert",
        {"uniform mat4 worldViewProj;",
         "gl_Position = worldViewProj * vertex;"});
    requireTokens(
        resolver, "media/ogre/HelloMine3DDirectionalShadowCaster.frag",
        {"gl_FragCoord.zzz", "out vec4 fragmentColour;"});
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
