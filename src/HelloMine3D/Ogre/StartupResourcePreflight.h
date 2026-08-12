#pragma once

#include <string>
#include <vector>

struct StartupResourceRequirement
{
    std::string category;
    std::string relativePath;
};

std::vector<StartupResourceRequirement> bootstrapResourceRequirements();
void validateStartupResources(
    const std::string& root,
    const std::vector<StartupResourceRequirement>& requirements);
