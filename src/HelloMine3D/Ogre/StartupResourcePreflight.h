#pragma once

#include <string>
#include <vector>

struct StartupResourceRequirement
{
    std::string category;
    std::string relativePath;
};

std::vector<StartupResourceRequirement> loadStartupResourceManifest(
    const std::string& root,
    const std::string& relativePath = "media/resource-manifest.txt");
void validateStartupResources(
    const std::string& root,
    const std::vector<StartupResourceRequirement>& requirements);
