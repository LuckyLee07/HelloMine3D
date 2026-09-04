#pragma once

#include <string>

#include "Material.h"

struct MachineProcessDefinition
{
    std::string id;
    Material::ID inputMaterialId = Material::ID::Nothing;
    int inputAmount = 0;
    Material::ID outputMaterialId = Material::ID::Nothing;
    int outputAmount = 0;
    int durationTicks = 0;
};

/// C2 owns one concrete, versioned process. It is intentionally not a
/// registry or an extension point for unapproved machines.
const MachineProcessDefinition &handCrusherProcessDefinition();
