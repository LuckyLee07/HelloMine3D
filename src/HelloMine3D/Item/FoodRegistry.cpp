#include "FoodRegistry.h"

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
    constexpr const char *FoodHeader =
        "# HelloMine3D food registry v1";

    struct PendingFood
    {
        FoodDefinition definition;
        bool restoreSeen = false;
        bool cooldownSeen = false;
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
            "Invalid food source '" + source + "' at line " +
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

    float parseRestore(const std::string &text, const std::string &source,
                       std::size_t line)
    {
        float value = 0.f;
        try {
            std::size_t consumed = 0;
            value = std::stof(text, &consumed);
            if (consumed != text.size()) {
                fail(source, line, "restore must be a finite number.");
            }
        }
        catch (const std::invalid_argument &) {
            fail(source, line, "restore must be a finite number.");
        }
        catch (const std::out_of_range &) {
            fail(source, line, "restore is outside the supported range.");
        }
        if (!std::isfinite(value) || value <= 0.f ||
            value > FoodRegistry::MaxHealthRestored) {
            fail(source, line,
                 "restore must be in (0, " +
                     std::to_string(FoodRegistry::MaxHealthRestored) + "].");
        }
        return value;
    }

    std::vector<FoodDefinition> parseSource(const FoodSource &source)
    {
        std::istringstream input(source.content);
        std::vector<FoodDefinition> result;
        PendingFood pending;
        bool headerSeen = false;
        bool insideFood = false;
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
                if (line != FoodHeader) {
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
            if (!insideFood) {
                if (parts.size() != 2 || parts[0] != "food") {
                    fail(source.name, lineNumber,
                         "expected 'food <material-id>'.");
                }
                Material::ID materialId = Material::ID::Nothing;
                if (!Material::tryParseStringId(parts[1], materialId) ||
                    materialId == Material::ID::Nothing ||
                    !Material::toMaterial(materialId).isFood) {
                    fail(source.name, lineNumber,
                         "food material must be registered as food.");
                }
                pending = {};
                pending.definition.materialId = materialId;
                pending.definition.sourceName = source.name;
                pending.startLine = lineNumber;
                insideFood = true;
                continue;
            }

            if (parts.size() == 2 && parts[0] == "restore") {
                if (pending.restoreSeen) {
                    fail(source.name, lineNumber, "restore is duplicated.");
                }
                pending.definition.healthRestored =
                    parseRestore(parts[1], source.name, lineNumber);
                pending.restoreSeen = true;
            }
            else if (parts.size() == 2 && parts[0] == "cooldown_ticks") {
                if (pending.cooldownSeen) {
                    fail(source.name, lineNumber,
                         "cooldown_ticks is duplicated.");
                }
                pending.definition.cooldownTicks = parseInteger(
                    parts[1], 1, FoodRegistry::MaxCooldownTicks,
                    source.name, lineNumber, "cooldown_ticks");
                pending.cooldownSeen = true;
            }
            else if (parts.size() == 1 && parts[0] == "end") {
                if (!pending.restoreSeen || !pending.cooldownSeen) {
                    fail(source.name, pending.startLine,
                         "food is missing restore or cooldown_ticks.");
                }
                result.push_back(std::move(pending.definition));
                insideFood = false;
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
        if (insideFood) {
            fail(source.name, pending.startLine,
                 "food is missing its 'end' directive.");
        }
        return result;
    }
}

void FoodRegistry::freeze(std::vector<FoodSource> sources)
{
    if (m_frozen) {
        throw std::runtime_error(
            "The food registry is already frozen for this process.");
    }
    if (sources.empty() || sources.size() > MaxSourceCount) {
        throw std::runtime_error(
            "Food source count must be in [1, " +
            std::to_string(MaxSourceCount) + "].");
    }
    std::sort(sources.begin(), sources.end(),
              [](const FoodSource &left, const FoodSource &right)
              {
                  return left.name < right.name;
              });

    std::vector<FoodDefinition> definitions;
    std::map<Material::ID, std::string> owners;
    std::set<std::string> sourceNames;
    for (const FoodSource &source : sources) {
        if (source.name.empty() || !sourceNames.insert(source.name).second ||
            source.content.empty() || source.content.size() > MaxSourceBytes) {
            throw std::runtime_error(
                "Food sources must be non-empty, bounded and uniquely named: '" +
                source.name + "'.");
        }
        for (FoodDefinition &definition : parseSource(source)) {
            if (definitions.size() >= MaxFoods) {
                throw std::runtime_error(
                    "Food registry exceeds " + std::to_string(MaxFoods) +
                    " entries.");
            }
            const auto inserted = owners.emplace(
                definition.materialId, definition.sourceName);
            if (!inserted.second) {
                throw std::runtime_error(
                    "Duplicate food material '" +
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
        if (Material::toMaterial(id).isFood && owners.count(id) == 0) {
            throw std::runtime_error(
                "Missing food definition for '" +
                std::string(Material::toStringId(id)) + "'.");
        }
    }
    if (definitions.empty()) {
        throw std::runtime_error("Food registry contains no foods.");
    }
    std::sort(definitions.begin(), definitions.end(),
              [](const FoodDefinition &left, const FoodDefinition &right)
              {
                  return left.materialId < right.materialId;
              });
    std::unordered_map<Material::ID, std::size_t> byMaterial;
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        byMaterial.emplace(definitions[index].materialId, index);
    }
    m_foods = std::move(definitions);
    m_byMaterial = std::move(byMaterial);
    m_frozen = true;
}

void FoodRegistry::freezeFromResourceView(
    const ResourcePackResolver &resolver)
{
    if (!resolver.isFrozen()) {
        throw std::runtime_error(
            "Food loading requires a frozen effective resource view.");
    }
    std::vector<FoodSource> sources;
    for (const EffectiveResource &resource : resolver.effectiveResources()) {
        if (resource.category != "food") {
            continue;
        }
        if (!resource.packName.empty()) {
            throw std::runtime_error(
                "Resource-pack v1 cannot own food resource '" +
                resource.logicalPath + "'.");
        }
        std::ifstream input(resource.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) > MaxSourceBytes) {
            throw std::runtime_error(
                "Missing, empty or oversized base food resource '" +
                resource.logicalPath + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size)) {
            throw std::runtime_error(
                "Unable to read base food resource '" +
                resource.logicalPath + "'.");
        }
        sources.push_back({resource.logicalPath, std::move(content)});
    }
    freeze(std::move(sources));
}

bool FoodRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

const std::vector<FoodDefinition> &FoodRegistry::foods() const noexcept
{
    return m_foods;
}

const FoodDefinition *FoodRegistry::find(
    Material::ID materialId) const noexcept
{
    const auto found = m_byMaterial.find(materialId);
    return found == m_byMaterial.end() ? nullptr : &m_foods[found->second];
}

FoodRegistry &runtimeFoodRegistry()
{
    static FoodRegistry registry;
    return registry;
}
