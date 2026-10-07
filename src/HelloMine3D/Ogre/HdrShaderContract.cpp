#include "HdrShaderContract.h"
#include "../Util/ResourcePackResolver.h"
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
namespace {
void require(const ResourcePackResolver& resolver, const std::string& path,
             std::initializer_list<const char*> tokens)
{
    std::ifstream input(resolver.resolve(path), std::ios::binary);
    if (!input) throw std::runtime_error("Missing HDR resource: " + path);
    std::ostringstream buffer; buffer << input.rdbuf();
    const std::string source = buffer.str();
    for (const auto* token : tokens)
        if (source.find(token) == std::string::npos)
            throw std::runtime_error("Invalid HDR resource '" + path + "': missing " + token);
}
}
void validateHdrShaderContract(const ResourcePackResolver& resolver)
{
    require(resolver, "media/ogre/HelloMine3DHdr.compositor",
        {"compositor HelloMine3D/LinearHdr", "texture scene 1 1 PF_FLOAT16_RGBA",
         "material HelloMine3D/HdrResolve", "input 0 scene"});
    require(resolver, "media/ogre/HelloMine3DHdr.program",
        {"fragment_program HelloMine3D/HdrResolveFragment glsl", "source HelloMine3DHdrResolve.frag",
         "param_named sceneTexture int 0", "param_named exposure float 1.0"});
    require(resolver, "media/ogre/HelloMine3DHdr.material",
        {"material HelloMine3D/HdrResolve", "vertex_program_ref HelloMine3D/PostProcessVertex",
         "fragment_program_ref HelloMine3D/HdrResolveFragment", "texture_unit sceneTexture"});
    require(resolver, "media/ogre/HelloMine3DHdrResolve.frag",
        {"uniform sampler2D sceneTexture;", "uniform float exposure;", "vec3 displayEncode", "vec3 toneMap"});
}
void validateHdrSceneShaderContract(const ResourcePackResolver& resolver)
{
    for (const auto* file : {"HelloMine3DTerrain.frag", "HelloMine3DTerrainShadow.frag",
            "HelloMine3DActor.frag", "HelloMine3DActorShadow.frag", "HelloMine3DSkybox.frag",
            "HelloMine3DWater.frag", "HelloMine3DBlockFeedback.frag", "HelloMine3DBlockParticle.frag",
            "HelloMine3DCaveBoundary.frag"})
        require(resolver, std::string("media/ogre/") + file,
            {"uniform float linearHdrMode;", "vec3 sceneColour(vec3 authored)"});
}
