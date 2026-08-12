#include "StartupResourcePreflight.h"

#include <fstream>
#include <stdexcept>

#include "../Util/ResourcePaths.h"

std::vector<StartupResourceRequirement> bootstrapResourceRequirements()
{
    return {
        {"shader", "media/ogre/HelloMine3DTerrain.vert"},
        {"texture", "media/textures/DefaultPack.png"},
        {"block", "media/blocks/Stone.block"},
    };
}

void validateStartupResources(
    const std::string& root,
    const std::vector<StartupResourceRequirement>& requirements)
{
    for (const StartupResourceRequirement& requirement : requirements)
    {
        const std::string resolved =
            ResourcePaths::join(root, requirement.relativePath);
        std::ifstream input(resolved, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0)
        {
            throw std::runtime_error(
                "Missing startup " + requirement.category +
                " resource '" + requirement.relativePath +
                "': expected a non-empty file at '" + resolved + "'.");
        }
    }
}
