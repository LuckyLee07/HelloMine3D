#ifndef ENTITY_H_INCLUDED
#define ENTITY_H_INCLUDED

#include "../Maths/glm.h"
#include "../Physics/AABB.h"

struct Entity {
    Entity()
        : position(glm::vec3(0.f))
        , rotation(glm::vec3(0.f))
        , velocity(glm::vec3(0.f))
        , box({0.f, 0.f, 0.f})
    {
    }

    Entity(const glm::vec3 &pos, const glm::vec3 &rot)
        : position(pos)
        , rotation(rot)
        , velocity(glm::vec3(0.f))
        , box({0, 0, 0})
    {
    }
    Entity(const glm::vec3 &pos, const glm::vec3 &rot, const glm::vec3 &box)
        : position(pos)
        , rotation(rot)
        , velocity(glm::vec3(0.f))
        , box(box)
    {
    }

    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 velocity;

    AABB box;
};

#endif // ENTITY_H_INCLUDED
