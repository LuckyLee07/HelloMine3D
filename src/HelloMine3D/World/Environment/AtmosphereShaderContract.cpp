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
