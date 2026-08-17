#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Item/Material.h"
#include "../Maths/glm.h"

class ResourcePackResolver;

enum class ObjectiveType
{
    ObtainItem,
    CraftItem,
    PlaceBlock,
    BreakBlock,
    DefeatEnemy,
    ReachLocation,
    PickupItem,
    ReopenWorld,
    SmeltItem
};

struct ObjectiveDefinition
{
    std::string id;
    std::string prerequisite;
    ObjectiveType type = ObjectiveType::ObtainItem;
    Material::ID targetMaterial = Material::ID::Nothing;
    int required = 1;
    glm::vec3 location{0.f};
    float radius = 0.f;
    bool visible = true;
    bool optional = false;
    std::string title;
    std::string instruction;
    std::string feedback;
    std::string sourceName;
};

struct ObjectiveSource
{
    std::string name;
    std::string content;
};

class ObjectiveRegistry
{
  public:
    static constexpr std::size_t MaxSourceCount = 16;
    static constexpr std::size_t MaxSourceBytes = 256 * 1024;
    static constexpr std::size_t MaxObjectives = 128;
    static constexpr int MaxProgress = 1000000;

    void freeze(std::vector<ObjectiveSource> sources);
    void freezeFromResourceView(const ResourcePackResolver& resolver);

    bool isFrozen() const noexcept;
    int definitionVersion() const noexcept;
    const std::vector<ObjectiveDefinition>& definitions() const noexcept;
    const ObjectiveDefinition* find(const std::string& id) const noexcept;

    static const char* typeName(ObjectiveType type) noexcept;
    static bool tryParseType(const std::string& value,
                             ObjectiveType& type) noexcept;

  private:
    std::vector<ObjectiveDefinition> m_definitions;
    std::unordered_map<std::string, std::size_t> m_byId;
    int m_definitionVersion = 0;
    bool m_frozen = false;
};

ObjectiveRegistry& runtimeObjectiveRegistry();
void ensureRuntimeObjectiveRegistry();
