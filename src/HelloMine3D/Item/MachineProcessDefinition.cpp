#include "MachineProcessDefinition.h"

const MachineProcessDefinition &handCrusherProcessDefinition()
{
    static const MachineProcessDefinition definition{
        "hellomine:crush_cobblestone",
        Material::ID::Cobblestone,
        1,
        Material::ID::Sand,
        1,
        40};
    return definition;
}
