#include "ToolRegistry.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../Util/ResourcePackResolver.h"

namespace
{
    constexpr const char *ToolHeader =
        "# HelloMine3D tool registry v1";

    struct PendingTool
    {
        ToolDefinition definition;
        bool classSeen = false;
        bool tierSeen = false;
        bool speedSeen = false;
        bool durabilitySeen = false;
        bool attackSeen = false;
        bool attackCooldownSeen = false;
        bool attackReachSeen = false;
        std::size_t startLine = 0;
    };

    std::string trim(const std::string &value)
    {
        const std::size_t begin = value.find_first_not_of(" \t\r");
        if (begin == std::string::npos) {
            return {};
        }
        const std::size_t end = value.find_last_not_of(" \t\r");
        return value.substr(begin, end - begin + 1);
    }

    std::vector<std::string> tokens(const std::string &line)
    {
        std::istringstream input(line);
        std::vector<std::string> result;
        std::string token;
        while (input >> token) {
            result.push_back(token);
        }
        return result;
    }

    [[noreturn]] void fail(const std::string &source, std::size_t line,
                           const std::string &message)
    {
        throw std::runtime_error(
            "Invalid tool source '" + source + "' at line " +
            std::to_string(line) + ": " + message);
    }

    int parseInteger(const std::string &text, int minimum, int maximum,
                     const std::string &source, std::size_t line,
                     const std::string &field)
    {
        int value = 0;
        try {
            std::size_t consumed = 0;
            value = std::stoi(text, &consumed);
            if (consumed != text.size()) {
                fail(source, line, field + " must be an integer.");
            }
        }
        catch (const std::invalid_argument &) {
            fail(source, line, field + " must be an integer.");
        }
        catch (const std::out_of_range &) {
            fail(source, line, field + " is outside the supported range.");
        }
        if (value < minimum || value > maximum) {
            fail(source, line,
                 field + " must be in [" + std::to_string(minimum) +
                     ", " + std::to_string(maximum) + "].");
        }
        return value;
    }

    float parseSpeed(const std::string &text, const std::string &source,
                     std::size_t line)
    {
        float value = 0.0f;
        try {
            std::size_t consumed = 0;
            value = std::stof(text, &consumed);
            if (consumed != text.size()) {
                fail(source, line, "speed must be a finite number.");
            }
        }
        catch (const std::invalid_argument &) {
            fail(source, line, "speed must be a finite number.");
        }
        catch (const std::out_of_range &) {
            fail(source, line, "speed is outside the supported range.");
        }
        if (!std::isfinite(value) || value < 1.0f ||
            value > ToolRegistry::MaxSpeedMultiplier) {
            fail(source, line,
                 "speed must be in [1, " +
                     std::to_string(ToolRegistry::MaxSpeedMultiplier) +
                     "].");
        }
        return value;
    }

    float parseAttack(const std::string &text, const std::string &source,
                      std::size_t line)
    {
        float value = 0.0f;
        try {
            std::size_t consumed = 0;
            value = std::stof(text, &consumed);
            if (consumed != text.size()) {
                fail(source, line, "attack must be a finite number.");
            }
        }
        catch (const std::invalid_argument &) {
            fail(source, line, "attack must be a finite number.");
        }
        catch (const std::out_of_range &) {
            fail(source, line, "attack is outside the supported range.");
        }
        if (!std::isfinite(value) || value < 1.0f ||
            value > ToolRegistry::MaxAttackDamage) {
            fail(source, line,
                 "attack must be in [1, " +
                     std::to_string(ToolRegistry::MaxAttackDamage) +
                     "].");
        }
        return value;
    }

    float parseAttackReach(const std::string &text,
                           const std::string &source, std::size_t line)
    {
        float value = 0.0f;
        try {
            std::size_t consumed = 0;
            value = std::stof(text, &consumed);
            if (consumed != text.size()) {
                fail(source, line,
                     "attack_reach must be a finite number.");
            }
        }
        catch (const std::exception &) {
            fail(source, line,
                 "attack_reach must be a finite number.");
        }
        if (!std::isfinite(value) || value < 1.0f ||
            value > ToolRegistry::MaxAttackReach) {
            fail(source, line,
                 "attack_reach must be in [1, " +
                     std::to_string(ToolRegistry::MaxAttackReach) + "].");
        }
        return value;
    }

    std::vector<ToolDefinition> parseSource(const ToolSource &source)
    {
        std::istringstream input(source.content);
        std::vector<ToolDefinition> result;
        PendingTool pending;
        bool headerSeen = false;
        bool insideTool = false;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (lineNumber == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf) {
                line.erase(0, 3);
            }
            line = trim(line);
            if (!headerSeen) {
                if (line != ToolHeader) {
                    fail(source.name, lineNumber,
                         "unsupported or missing header.");
                }
                headerSeen = true;
                continue;
            }
            if (line.empty() || line.front() == '#') {
                continue;
            }
            const std::vector<std::string> parts = tokens(line);
            if (!insideTool) {
                if (parts.size() != 2 || parts[0] != "tool") {
                    fail(source.name, lineNumber,
                         "expected 'tool <material-id>'.");
                }
                Material::ID materialId = Material::ID::Nothing;
                if (!Material::tryParseStringId(parts[1], materialId) ||
                    materialId == Material::ID::Nothing ||
                    !Material::toMaterial(materialId).isTool) {
                    fail(source.name, lineNumber,
                         "tool material must be a registered unstackable tool.");
                }
                pending = {};
                pending.definition.materialId = materialId;
                pending.definition.sourceName = source.name;
                pending.startLine = lineNumber;
                insideTool = true;
                continue;
            }

            if (parts.size() == 2 && parts[0] == "class") {
                if (pending.classSeen ||
                    !ToolRegistry::tryParseMiningClass(
                        parts[1], pending.definition.miningClass) ||
                    pending.definition.miningClass == MiningClass::None) {
                    fail(source.name, lineNumber,
                         "class must be one supported non-empty class.");
                }
                pending.classSeen = true;
            }
            else if (parts.size() == 2 && parts[0] == "tier") {
                if (pending.tierSeen) {
                    fail(source.name, lineNumber, "tier is duplicated.");
                }
                pending.definition.tier = parseInteger(
                    parts[1], 1, ToolRegistry::MaxTier,
                    source.name, lineNumber, "tier");
                pending.tierSeen = true;
            }
            else if (parts.size() == 2 && parts[0] == "speed") {
                if (pending.speedSeen) {
                    fail(source.name, lineNumber, "speed is duplicated.");
                }
                pending.definition.speedMultiplier =
                    parseSpeed(parts[1], source.name, lineNumber);
                pending.speedSeen = true;
            }
            else if (parts.size() == 2 && parts[0] == "durability") {
                if (pending.durabilitySeen) {
                    fail(source.name, lineNumber,
                         "durability is duplicated.");
                }
                pending.definition.maxDurability = parseInteger(
                    parts[1], 1, ToolRegistry::MaxDurability,
                    source.name, lineNumber, "durability");
                pending.durabilitySeen = true;
            }
            else if (parts.size() == 2 && parts[0] == "attack") {
                if (pending.attackSeen) {
                    fail(source.name, lineNumber, "attack is duplicated.");
                }
                pending.definition.attackDamage =
                    parseAttack(parts[1], source.name, lineNumber);
                pending.attackSeen = true;
            }
            else if (parts.size() == 2 && parts[0] == "attack_cooldown") {
                if (pending.attackCooldownSeen) {
                    fail(source.name, lineNumber,
                         "attack_cooldown is duplicated.");
                }
                pending.definition.attackCooldownTicks = parseInteger(
                    parts[1], 1, ToolRegistry::MaxAttackCooldownTicks,
                    source.name, lineNumber, "attack_cooldown");
                pending.attackCooldownSeen = true;
            }
            else if (parts.size() == 2 && parts[0] == "attack_reach") {
                if (pending.attackReachSeen) {
                    fail(source.name, lineNumber,
                         "attack_reach is duplicated.");
                }
                pending.definition.attackReach = parseAttackReach(
                    parts[1], source.name, lineNumber);
                pending.attackReachSeen = true;
            }
            else if (parts.size() == 1 && parts[0] == "end") {
                if (!pending.classSeen || !pending.tierSeen ||
                    !pending.speedSeen || !pending.durabilitySeen ||
                    !pending.attackSeen || !pending.attackCooldownSeen ||
                    !pending.attackReachSeen) {
                    fail(source.name, pending.startLine,
                         "tool is missing a required mining or combat field.");
                }
                result.push_back(std::move(pending.definition));
                insideTool = false;
            }
            else {
                fail(source.name, lineNumber,
                     "unknown, duplicated or misplaced directive '" +
                         parts[0] + "'.");
            }
        }
        if (!headerSeen) {
            fail(source.name, 1, "empty source.");
        }
        if (insideTool) {
            fail(source.name, pending.startLine,
                 "tool is missing its 'end' directive.");
        }
        return result;
    }
}

void ToolRegistry::freeze(std::vector<ToolSource> sources)
{
    if (m_frozen) {
        throw std::runtime_error(
            "The tool registry is already frozen for this process.");
    }
    if (sources.empty() || sources.size() > MaxSourceCount) {
        throw std::runtime_error(
            "Tool source count must be in [1, " +
            std::to_string(MaxSourceCount) + "].");
    }
    std::sort(sources.begin(), sources.end(),
              [](const ToolSource &left, const ToolSource &right)
              {
                  return left.name < right.name;
              });

    std::vector<ToolDefinition> definitions;
    std::map<Material::ID, std::string> owners;
    std::set<std::string> sourceNames;
    for (const ToolSource &source : sources) {
        if (source.name.empty() || !sourceNames.insert(source.name).second ||
            source.content.empty() || source.content.size() > MaxSourceBytes) {
            throw std::runtime_error(
                "Tool sources must be non-empty, bounded and uniquely named: '" +
                source.name + "'.");
        }
        for (ToolDefinition &definition : parseSource(source)) {
            if (definitions.size() >= MaxTools) {
                throw std::runtime_error(
                    "Tool registry exceeds " + std::to_string(MaxTools) +
                    " entries.");
            }
            const auto inserted = owners.emplace(
                definition.materialId, definition.sourceName);
            if (!inserted.second) {
                throw std::runtime_error(
                    "Duplicate tool material '" +
                    std::string(Material::toStringId(definition.materialId)) +
                    "' in '" + definition.sourceName +
                    "'; first declared in '" + inserted.first->second +
                    "'.");
            }
            definitions.push_back(std::move(definition));
        }
    }
    for (int value = static_cast<int>(Material::ID::Nothing) + 1;
         value < static_cast<int>(Material::ID::Count); ++value) {
        const Material::ID id = static_cast<Material::ID>(value);
        if (Material::toMaterial(id).isTool && owners.count(id) == 0) {
            throw std::runtime_error(
                "Missing tool definition for '" +
                std::string(Material::toStringId(id)) + "'.");
        }
    }
    if (definitions.empty()) {
        throw std::runtime_error("Tool registry contains no tools.");
    }
    std::sort(definitions.begin(), definitions.end(),
              [](const ToolDefinition &left, const ToolDefinition &right)
              {
                  return left.materialId < right.materialId;
              });
    std::unordered_map<Material::ID, std::size_t> byMaterial;
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        byMaterial.emplace(definitions[index].materialId, index);
    }
    m_tools = std::move(definitions);
    m_byMaterial = std::move(byMaterial);
    m_frozen = true;
}

void ToolRegistry::freezeFromResourceView(
    const ResourcePackResolver &resolver)
{
    if (!resolver.isFrozen()) {
        throw std::runtime_error(
            "Tool loading requires a frozen effective resource view.");
    }
    std::vector<ToolSource> sources;
    for (const EffectiveResource &resource : resolver.effectiveResources()) {
        if (resource.category != "tool") {
            continue;
        }
        if (!resource.packName.empty()) {
            throw std::runtime_error(
                "Resource-pack v1 cannot own tool resource '" +
                resource.logicalPath + "'.");
        }
        std::ifstream input(resource.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) > MaxSourceBytes) {
            throw std::runtime_error(
                "Missing, empty or oversized base tool resource '" +
                resource.logicalPath + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size)) {
            throw std::runtime_error(
                "Unable to read base tool resource '" +
                resource.logicalPath + "'.");
        }
        sources.push_back({resource.logicalPath, std::move(content)});
    }
    freeze(std::move(sources));
}

bool ToolRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

const std::vector<ToolDefinition> &ToolRegistry::tools() const noexcept
{
    return m_tools;
}

const ToolDefinition *ToolRegistry::find(
    Material::ID materialId) const noexcept
{
    const auto found = m_byMaterial.find(materialId);
    return found == m_byMaterial.end() ? nullptr : &m_tools[found->second];
}

const char *ToolRegistry::miningClassName(MiningClass value) noexcept
{
    switch (value) {
        case MiningClass::Pickaxe:
            return "pickaxe";
        case MiningClass::Axe:
            return "axe";
        case MiningClass::Shovel:
            return "shovel";
        case MiningClass::Weapon:
            return "weapon";
        default:
            return "none";
    }
}

bool ToolRegistry::tryParseMiningClass(
    const std::string &text, MiningClass &value) noexcept
{
    if (text == "none") {
        value = MiningClass::None;
        return true;
    }
    if (text == "pickaxe") {
        value = MiningClass::Pickaxe;
        return true;
    }
    if (text == "axe") {
        value = MiningClass::Axe;
        return true;
    }
    if (text == "shovel") {
        value = MiningClass::Shovel;
        return true;
    }
    if (text == "weapon") {
        value = MiningClass::Weapon;
        return true;
    }
    return false;
}

ToolRegistry &runtimeToolRegistry()
{
    static ToolRegistry registry;
    return registry;
}
