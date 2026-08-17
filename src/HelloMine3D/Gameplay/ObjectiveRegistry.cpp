#include "ObjectiveRegistry.h"

#include "ObjectiveState.h"
#include "../Util/ResourcePackResolver.h"
#include "../Util/ResourcePaths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr const char* Header =
        "# HelloMine3D objective registry v1";

    std::string trim(const std::string& value)
    {
        std::size_t begin = 0;
        while (begin < value.size() &&
               std::isspace(static_cast<unsigned char>(value[begin])))
        {
            ++begin;
        }
        std::size_t end = value.size();
        while (end > begin &&
               std::isspace(static_cast<unsigned char>(value[end - 1])))
        {
            --end;
        }
        return value.substr(begin, end - begin);
    }

    [[noreturn]] void reject(const ObjectiveSource& source,
                             std::size_t line,
                             const std::string& reason)
    {
        throw std::runtime_error(
            "Invalid objective resource '" + source.name + "' at line " +
            std::to_string(line) + ": " + reason);
    }

    std::string singleValue(const ObjectiveSource& source,
                            std::size_t line,
                            const std::string& text)
    {
        std::istringstream input(text);
        std::string value;
        if (!(input >> value))
        {
            reject(source, line, "missing value");
        }
        input >> std::ws;
        if (!input.eof())
        {
            reject(source, line, "unexpected trailing value");
        }
        return value;
    }

    std::string quotedValue(const ObjectiveSource& source,
                            std::size_t line,
                            const std::string& text)
    {
        std::istringstream input(text);
        std::string value;
        if (!(input >> std::quoted(value)))
        {
            reject(source, line, "expected a quoted string");
        }
        input >> std::ws;
        if (!input.eof() || value.empty() || value.size() > 240)
        {
            reject(source, line, "quoted string is empty, oversized or trailing");
        }
        return value;
    }

    int positiveInt(const ObjectiveSource& source, std::size_t line,
                    const std::string& text, int maximum)
    {
        const std::string value = singleValue(source, line, text);
        int parsed = 0;
        std::size_t consumed = 0;
        try
        {
            parsed = std::stoi(value, &consumed);
        }
        catch (const std::invalid_argument&)
        {
            reject(source, line, "integer is invalid");
        }
        catch (const std::out_of_range&)
        {
            reject(source, line, "integer is outside the allowed range");
        }
        if (consumed != value.size() || parsed <= 0 || parsed > maximum)
        {
            reject(source, line, "integer is outside the allowed range");
        }
        return parsed;
    }

    bool booleanValue(const ObjectiveSource& source, std::size_t line,
                      const std::string& text)
    {
        const std::string value = singleValue(source, line, text);
        if (value == "0")
        {
            return false;
        }
        if (value == "1")
        {
            return true;
        }
        reject(source, line, "boolean must be 0 or 1");
    }

    Material::ID materialValue(const ObjectiveSource& source,
                               std::size_t line,
                               const std::string& text)
    {
        Material::ID material = Material::ID::Nothing;
        const std::string value = singleValue(source, line, text);
        if (!Material::tryParseStringId(value, material) ||
            material == Material::ID::Nothing)
        {
            reject(source, line, "target material is not registered");
        }
        return material;
    }

    bool typeNeedsTarget(ObjectiveType type)
    {
        return type == ObjectiveType::ObtainItem ||
               type == ObjectiveType::CraftItem ||
               type == ObjectiveType::PlaceBlock ||
               type == ObjectiveType::BreakBlock ||
               type == ObjectiveType::PickupItem ||
               type == ObjectiveType::SmeltItem;
    }

    void validateDefinition(const ObjectiveSource& source,
                            std::size_t line,
                            const ObjectiveDefinition& definition,
                            const std::set<std::string>& fields)
    {
        static const std::set<std::string> required = {
            "type", "required", "prerequisite", "visible", "optional",
            "title", "instruction", "feedback"};
        for (const std::string& field : required)
        {
            if (fields.find(field) == fields.end())
            {
                reject(source, line, "objective '" + definition.id +
                                         "' is missing field '" + field + "'");
            }
        }
        const bool hasTarget = fields.find("target") != fields.end();
        const bool hasLocation = fields.find("location") != fields.end();
        if (typeNeedsTarget(definition.type) != hasTarget)
        {
            reject(source, line,
                   "target presence does not match the objective type");
        }
        if ((definition.type == ObjectiveType::ReachLocation) != hasLocation)
        {
            reject(source, line,
                   "location presence does not match the objective type");
        }
        if ((definition.type == ObjectiveType::PlaceBlock ||
             definition.type == ObjectiveType::BreakBlock) &&
            !Material::toMaterial(definition.targetMaterial).isBlock)
        {
            reject(source, line, "block objective target is not a block");
        }
        if (!definition.visible && !definition.optional)
        {
            reject(source, line,
                   "a hidden objective must be optional");
        }
        if ((definition.type == ObjectiveType::ReachLocation ||
             definition.type == ObjectiveType::ReopenWorld) &&
            definition.required != 1)
        {
            reject(source, line,
                   "reach and reopen objectives must require exactly one");
        }
    }

    std::vector<ObjectiveDefinition>
    parseSource(const ObjectiveSource& source, int& version)
    {
        if (source.content.empty() ||
            source.content.size() > ObjectiveRegistry::MaxSourceBytes)
        {
            throw std::runtime_error("Objective source '" + source.name +
                                     "' is empty or oversized.");
        }

        std::istringstream input(source.content);
        std::string line;
        std::size_t lineNumber = 0;
        bool headerSeen = false;
        bool versionSeen = false;
        bool inObjective = false;
        ObjectiveDefinition current;
        std::set<std::string> fields;
        std::vector<ObjectiveDefinition> result;

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
            line = trim(line);
            if (line.empty())
            {
                continue;
            }
            if (!headerSeen)
            {
                if (line != Header)
                {
                    reject(source, lineNumber,
                           "unsupported or missing header");
                }
                headerSeen = true;
                continue;
            }
            if (line.front() == '#')
            {
                continue;
            }

            const std::size_t separator = line.find_first_of(" \t");
            const std::string key = line.substr(0, separator);
            const std::string value = separator == std::string::npos
                                          ? std::string()
                                          : trim(line.substr(separator + 1));

            if (!inObjective)
            {
                if (key == "version")
                {
                    if (versionSeen)
                    {
                        reject(source, lineNumber, "duplicate version");
                    }
                    const int parsed = positiveInt(source, lineNumber,
                                                   value, 1);
                    if (version != 0 && version != parsed)
                    {
                        reject(source, lineNumber,
                               "definition versions disagree across sources");
                    }
                    version = parsed;
                    versionSeen = true;
                }
                else if (key == "objective")
                {
                    if (!versionSeen)
                    {
                        reject(source, lineNumber,
                               "version must precede objectives");
                    }
                    current = {};
                    current.id = singleValue(source, lineNumber, value);
                    current.sourceName = source.name;
                    if (!ObjectiveState::isCanonicalId(current.id))
                    {
                        reject(source, lineNumber,
                               "objective id is not canonical");
                    }
                    fields.clear();
                    inObjective = true;
                }
                else
                {
                    reject(source, lineNumber,
                           "expected version or objective");
                }
                continue;
            }

            if (key == "end")
            {
                if (!value.empty())
                {
                    reject(source, lineNumber, "end has trailing data");
                }
                validateDefinition(source, lineNumber, current, fields);
                result.push_back(std::move(current));
                fields.clear();
                inObjective = false;
                continue;
            }
            if (!fields.emplace(key).second)
            {
                reject(source, lineNumber, "duplicate field '" + key + "'");
            }

            if (key == "type")
            {
                const std::string type = singleValue(source, lineNumber, value);
                if (!ObjectiveRegistry::tryParseType(type, current.type))
                {
                    reject(source, lineNumber, "unknown objective type");
                }
            }
            else if (key == "target")
            {
                current.targetMaterial =
                    materialValue(source, lineNumber, value);
            }
            else if (key == "required")
            {
                current.required = positiveInt(
                    source, lineNumber, value, ObjectiveRegistry::MaxProgress);
            }
            else if (key == "prerequisite")
            {
                current.prerequisite = singleValue(source, lineNumber, value);
                if (current.prerequisite == "none")
                {
                    current.prerequisite.clear();
                }
                else if (!ObjectiveState::isCanonicalId(
                             current.prerequisite))
                {
                    reject(source, lineNumber,
                           "prerequisite id is not canonical");
                }
            }
            else if (key == "visible")
            {
                current.visible = booleanValue(source, lineNumber, value);
            }
            else if (key == "optional")
            {
                current.optional = booleanValue(source, lineNumber, value);
            }
            else if (key == "title")
            {
                current.title = quotedValue(source, lineNumber, value);
            }
            else if (key == "instruction")
            {
                current.instruction = quotedValue(source, lineNumber, value);
            }
            else if (key == "feedback")
            {
                current.feedback = quotedValue(source, lineNumber, value);
            }
            else if (key == "location")
            {
                std::istringstream location(value);
                if (!(location >> current.location.x >> current.location.y >>
                      current.location.z >> current.radius))
                {
                    reject(source, lineNumber, "location is invalid");
                }
                location >> std::ws;
                if (!location.eof() || !std::isfinite(current.location.x) ||
                    !std::isfinite(current.location.y) ||
                    !std::isfinite(current.location.z) ||
                    !std::isfinite(current.radius) || current.radius <= 0.f ||
                    current.radius > 100000.f)
                {
                    reject(source, lineNumber,
                           "location is non-finite, trailing or out of range");
                }
            }
            else
            {
                reject(source, lineNumber, "unknown field '" + key + "'");
            }
        }

        if (!headerSeen || !versionSeen || inObjective)
        {
            reject(source, lineNumber,
                   inObjective ? "unterminated objective"
                               : "header or version is missing");
        }
        return result;
    }
}

void ObjectiveRegistry::freeze(std::vector<ObjectiveSource> sources)
{
    if (m_frozen)
    {
        throw std::runtime_error("Objective registry is already frozen.");
    }
    if (sources.empty() || sources.size() > MaxSourceCount)
    {
        throw std::runtime_error("Objective source count is invalid.");
    }

    std::vector<ObjectiveDefinition> definitions;
    int version = 0;
    for (const ObjectiveSource& source : sources)
    {
        std::vector<ObjectiveDefinition> parsed = parseSource(source, version);
        definitions.insert(definitions.end(),
                           std::make_move_iterator(parsed.begin()),
                           std::make_move_iterator(parsed.end()));
    }
    if (version != ObjectiveSaveState::CurrentDefinitionVersion ||
        definitions.empty() || definitions.size() > MaxObjectives)
    {
        throw std::runtime_error(
            "Objective definition version or count is unsupported.");
    }

    std::unordered_map<std::string, std::size_t> byId;
    for (std::size_t index = 0; index < definitions.size(); ++index)
    {
        const ObjectiveDefinition& definition = definitions[index];
        if (!byId.emplace(definition.id, index).second)
        {
            throw std::runtime_error("Duplicate objective id '" +
                                     definition.id + "'.");
        }
        if (!definition.prerequisite.empty())
        {
            const auto prerequisite = byId.find(definition.prerequisite);
            if (prerequisite == byId.end() || prerequisite->second >= index)
            {
                throw std::runtime_error(
                    "Objective '" + definition.id +
                    "' has a missing or forward prerequisite '" +
                    definition.prerequisite + "'.");
            }
        }
    }

    m_definitions = std::move(definitions);
    m_byId = std::move(byId);
    m_definitionVersion = version;
    m_frozen = true;
}

void ObjectiveRegistry::freezeFromResourceView(
    const ResourcePackResolver& resolver)
{
    if (!resolver.isFrozen())
    {
        throw std::runtime_error(
            "Objective loading requires a frozen effective resource view.");
    }
    std::vector<ObjectiveSource> sources;
    for (const EffectiveResource& resource : resolver.effectiveResources())
    {
        if (resource.category != "objective")
        {
            continue;
        }
        if (!resource.packName.empty())
        {
            throw std::runtime_error(
                "Resource-pack v1 cannot own objective resource '" +
                resource.logicalPath + "'.");
        }
        std::ifstream input(resource.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) > MaxSourceBytes)
        {
            throw std::runtime_error(
                "Missing, empty or oversized base objective resource '" +
                resource.logicalPath + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size))
        {
            throw std::runtime_error("Unable to read base objective resource '" +
                                     resource.logicalPath + "'.");
        }
        sources.push_back({resource.logicalPath, std::move(content)});
    }
    freeze(std::move(sources));
}

bool ObjectiveRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

int ObjectiveRegistry::definitionVersion() const noexcept
{
    return m_definitionVersion;
}

const std::vector<ObjectiveDefinition>&
ObjectiveRegistry::definitions() const noexcept
{
    return m_definitions;
}

const ObjectiveDefinition* ObjectiveRegistry::find(
    const std::string& id) const noexcept
{
    const auto found = m_byId.find(id);
    return found == m_byId.end() ? nullptr : &m_definitions[found->second];
}

const char* ObjectiveRegistry::typeName(ObjectiveType type) noexcept
{
    switch (type)
    {
    case ObjectiveType::ObtainItem: return "obtain_item";
    case ObjectiveType::CraftItem: return "craft_item";
    case ObjectiveType::PlaceBlock: return "place_block";
    case ObjectiveType::BreakBlock: return "break_block";
    case ObjectiveType::DefeatEnemy: return "defeat_enemy";
    case ObjectiveType::ReachLocation: return "reach_location";
    case ObjectiveType::PickupItem: return "pickup_item";
    case ObjectiveType::ReopenWorld: return "reopen_world";
    case ObjectiveType::SmeltItem: return "smelt_item";
    }
    return "unknown";
}

bool ObjectiveRegistry::tryParseType(const std::string& value,
                                     ObjectiveType& type) noexcept
{
    static const std::pair<const char*, ObjectiveType> values[] = {
        {"obtain_item", ObjectiveType::ObtainItem},
        {"craft_item", ObjectiveType::CraftItem},
        {"place_block", ObjectiveType::PlaceBlock},
        {"break_block", ObjectiveType::BreakBlock},
        {"defeat_enemy", ObjectiveType::DefeatEnemy},
        {"reach_location", ObjectiveType::ReachLocation},
        {"pickup_item", ObjectiveType::PickupItem},
        {"reopen_world", ObjectiveType::ReopenWorld},
        {"smelt_item", ObjectiveType::SmeltItem}};
    for (const auto& entry : values)
    {
        if (value == entry.first)
        {
            type = entry.second;
            return true;
        }
    }
    return false;
}

ObjectiveRegistry& runtimeObjectiveRegistry()
{
    static ObjectiveRegistry registry;
    return registry;
}

void ensureRuntimeObjectiveRegistry()
{
    ObjectiveRegistry& registry = runtimeObjectiveRegistry();
    if (registry.isFrozen())
    {
        return;
    }
    const std::string path = ResourcePaths::media("objectives/Base.objective");
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() <= 0 ||
        static_cast<std::size_t>(input.tellg()) >
            ObjectiveRegistry::MaxSourceBytes)
    {
        throw std::runtime_error(
            "Missing, empty or oversized base objective resource '" + path +
            "'.");
    }
    const std::streamsize size = input.tellg();
    input.seekg(0, std::ios::beg);
    std::string content(static_cast<std::size_t>(size), '\0');
    if (!input.read(content.data(), size))
    {
        throw std::runtime_error("Unable to read base objective resource '" +
                                 path + "'.");
    }
    registry.freeze({{"media/objectives/Base.objective",
                      std::move(content)}});
}
