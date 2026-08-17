#include "StartupResourcePreflight.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "../Util/ResourcePaths.h"
#include "../Util/ResourcePackResolver.h"

std::vector<StartupResourceRequirement> loadStartupResourceManifest(
    const std::string& root, const std::string& relativePath)
{
    const std::string resolved = ResourcePaths::join(root, relativePath);
    std::ifstream input(resolved);
    if (!input)
    {
        throw std::runtime_error(
            "Missing startup manifest resource '" + relativePath +
            "': expected a readable file at '" + resolved + "'.");
    }

    std::vector<StartupResourceRequirement> requirements;
    std::string previous;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (lineNumber == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xef &&
            static_cast<unsigned char>(line[1]) == 0xbb &&
            static_cast<unsigned char>(line[2]) == 0xbf)
        {
            line.erase(0, 3);
        }
        if (lineNumber == 1)
        {
            if (line != "# HelloMine3D resource manifest v1")
            {
                throw std::runtime_error(
                    "Invalid startup resource manifest '" + relativePath +
                    "': unsupported or missing header.");
            }
            continue;
        }
        if (line.empty() || line.front() == '#')
        {
            continue;
        }

        const std::size_t separator = line.find('|');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= line.size() ||
            line.find('|', separator + 1) != std::string::npos)
        {
            throw std::runtime_error(
                "Invalid startup resource manifest '" + relativePath +
                "': line " + std::to_string(lineNumber) +
                " must be category|relative/path.");
        }
        if (!previous.empty() && line <= previous)
        {
            throw std::runtime_error(
                "Invalid startup resource manifest '" + relativePath +
                "': entries must be unique and sorted (line " +
                std::to_string(lineNumber) + ").");
        }
        previous = line;
        const std::string resourcePath = line.substr(separator + 1);
        if (resourcePath.front() == '/' ||
            resourcePath.find("..") != std::string::npos)
        {
            throw std::runtime_error(
                "Invalid startup resource manifest '" + relativePath +
                "': line " + std::to_string(lineNumber) +
                " must use a repository-relative path.");
        }
        requirements.push_back(
            {line.substr(0, separator), resourcePath});
    }
    if (requirements.empty())
    {
        throw std::runtime_error(
            "Invalid startup resource manifest '" + relativePath +
            "': no resource entries were found.");
    }
    return requirements;
}

void validateStartupResources(
    const std::string& root,
    const std::vector<StartupResourceRequirement>& requirements)
{
    for (const StartupResourceRequirement& requirement : requirements)
    {
        const std::string resolved = runtimeResourcePackResolver().isFrozen()
            ? runtimeResourcePackResolver().resolve(requirement.relativePath)
            : ResourcePaths::join(root, requirement.relativePath);
        std::ifstream input(resolved, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0)
        {
            if (requirement.category == "audio")
            {
                continue;
            }
            std::string owner = "base resources";
            if (runtimeResourcePackResolver().isFrozen())
            {
                const auto& effective =
                    runtimeResourcePackResolver().effectiveResources();
                const auto found = std::find_if(
                    effective.begin(), effective.end(),
                    [&requirement](const EffectiveResource& resource)
                    {
                        return resource.logicalPath ==
                               requirement.relativePath;
                    });
                if (found != effective.end() && !found->packName.empty())
                {
                    owner = "resource pack '" + found->packName + "'";
                }
            }
            throw std::runtime_error(
                "Missing startup " + requirement.category +
                " resource '" + requirement.relativePath +
                "' from " + owner +
                ": expected a non-empty file at '" + resolved + "'.");
        }
    }
}
