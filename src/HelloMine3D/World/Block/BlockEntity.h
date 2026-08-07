#ifndef BLOCKENTITY_H_INCLUDED
#define BLOCKENTITY_H_INCLUDED

#include <string>

#include "../../Maths/glm.h"

struct BlockEntityRecord {
    glm::ivec3 position{0};
    std::string type;
    std::string payload;
};

#endif // BLOCKENTITY_H_INCLUDED
