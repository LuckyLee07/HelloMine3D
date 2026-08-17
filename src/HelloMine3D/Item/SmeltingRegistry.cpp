#include "SmeltingRegistry.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../Util/ResourcePackResolver.h"

namespace
{
constexpr const char *SmeltingHeader =
    "# HelloMine3D smelting registry v1";

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
        "Invalid smelting source '" + source + "' at line " +
        std::to_string(line) + ": " + message);
}

int parsePositive(const std::string &text, int maximum,
                  const std::string &source, std::size_t line,
                  const std::string &field)
{
    int value = 0;
    try {
        std::size_t consumed = 0;
        value = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            value = 0;
        }
    }
    catch (...) {
        value = 0;
    }
    if (value <= 0 || value > maximum) {
        fail(source, line, field + " must be in [1, " +
                               std::to_string(maximum) + "].");
    }
    return value;
}

Material::ID parseStackableMaterial(const std::string &text,
                                    const std::string &source,
                                    std::size_t line)
{
    Material::ID id = Material::ID::Nothing;
    if (!Material::tryParseStringId(text, id) ||
        id == Material::ID::Nothing || Material::toMaterial(id).isTool) {
        fail(source, line,
             "material must be a registered stackable material.");
    }
    return id;
}

bool isValidId(const std::string &value)
{
    const std::size_t separator = value.find(':');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 >= value.size() || value.size() > 128) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') ||
               (ch >= '0' && ch <= '9') || ch == ':' || ch == '_' ||
               ch == '-' || ch == '.' || ch == '/';
    });
}

struct ParsedSource
{
    std::vector<SmeltingRecipeDefinition> recipes;
    std::vector<SmeltingFuelDefinition> fuels;
};

ParsedSource parseSource(const SmeltingSource &source)
{
    enum class Section { None, Recipe, Fuel };
    ParsedSource result;
    SmeltingRecipeDefinition recipe;
    SmeltingFuelDefinition fuel;
    bool inputSeen = false;
    bool outputSeen = false;
    bool ticksSeen = false;
    bool headerSeen = false;
    Section section = Section::None;
    std::size_t startLine = 0;
    std::istringstream input(source.content);
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
            if (line != SmeltingHeader) {
                fail(source.name, lineNumber,
                     "unsupported or missing header.");
            }
            headerSeen = true;
            continue;
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto parts = tokens(line);
        if (section == Section::None) {
            if (parts.size() == 2 && parts[0] == "smelt" &&
                isValidId(parts[1])) {
                recipe = {};
                recipe.id = parts[1];
                recipe.sourceName = source.name;
                inputSeen = outputSeen = ticksSeen = false;
                section = Section::Recipe;
                startLine = lineNumber;
            }
            else if (parts.size() == 2 && parts[0] == "fuel") {
                fuel = {};
                fuel.materialId = parseStackableMaterial(
                    parts[1], source.name, lineNumber);
                fuel.sourceName = source.name;
                ticksSeen = false;
                section = Section::Fuel;
                startLine = lineNumber;
            }
            else {
                fail(source.name, lineNumber,
                     "expected 'smelt <id>' or 'fuel <material-id>'.");
            }
            continue;
        }

        if (section == Section::Recipe && parts.size() == 2 &&
            parts[0] == "input" && !inputSeen) {
            recipe.inputMaterialId = parseStackableMaterial(
                parts[1], source.name, lineNumber);
            inputSeen = true;
        }
        else if (section == Section::Recipe && parts.size() == 3 &&
                 parts[0] == "output" && !outputSeen) {
            recipe.outputMaterialId = parseStackableMaterial(
                parts[1], source.name, lineNumber);
            recipe.outputAmount = parsePositive(
                parts[2], Material::toMaterial(recipe.outputMaterialId)
                              .maxStackSize,
                source.name, lineNumber, "output count");
            outputSeen = true;
        }
        else if (parts.size() == 2 && parts[0] == "ticks" &&
                 !ticksSeen) {
            const int ticks = parsePositive(parts[1],
                                            SmeltingRegistry::MaxTicks,
                                            source.name, lineNumber,
                                            "ticks");
            if (section == Section::Recipe) {
                recipe.durationTicks = ticks;
            }
            else {
                fuel.burnTicks = ticks;
            }
            ticksSeen = true;
        }
        else if (parts.size() == 1 && parts[0] == "end") {
            if (section == Section::Recipe) {
                if (!inputSeen || !outputSeen || !ticksSeen ||
                    recipe.inputMaterialId == recipe.outputMaterialId) {
                    fail(source.name, startLine,
                         "smelt requires distinct input/output and ticks.");
                }
                result.recipes.push_back(recipe);
            }
            else {
                if (!ticksSeen) {
                    fail(source.name, startLine,
                         "fuel requires ticks.");
                }
                result.fuels.push_back(fuel);
            }
            section = Section::None;
        }
        else {
            fail(source.name, lineNumber,
                 "unknown, duplicated or misplaced directive.");
        }
    }
    if (!headerSeen) {
        fail(source.name, 1, "empty source.");
    }
    if (section != Section::None) {
        fail(source.name, startLine, "entry is missing its 'end'.");
    }
    return result;
}
} // namespace

void SmeltingRegistry::freeze(std::vector<SmeltingSource> sources)
{
    if (m_frozen) {
        throw std::runtime_error("The smelting registry is already frozen.");
    }
    if (sources.empty() || sources.size() > MaxSourceCount) {
        throw std::runtime_error("Smelting source count is outside limits.");
    }
    std::sort(sources.begin(), sources.end(),
              [](const auto &left, const auto &right) {
                  return left.name < right.name;
              });
    std::set<std::string> sourceNames;
    std::map<std::string, std::string> recipeOwners;
    std::map<Material::ID, std::string> inputOwners;
    std::map<Material::ID, std::string> fuelOwners;
    std::vector<SmeltingRecipeDefinition> recipes;
    std::vector<SmeltingFuelDefinition> fuels;
    for (const SmeltingSource &source : sources) {
        if (source.name.empty() || !sourceNames.insert(source.name).second ||
            source.content.empty() || source.content.size() > MaxSourceBytes) {
            throw std::runtime_error(
                "Smelting sources must be non-empty, bounded and unique.");
        }
        ParsedSource parsed = parseSource(source);
        for (auto &recipe : parsed.recipes) {
            if (recipes.size() >= MaxRecipes ||
                !recipeOwners.emplace(recipe.id, recipe.sourceName).second ||
                !inputOwners.emplace(recipe.inputMaterialId,
                                     recipe.sourceName).second) {
                throw std::runtime_error(
                    "Duplicate or excessive smelting recipe in '" +
                    recipe.sourceName + "'.");
            }
            recipes.push_back(std::move(recipe));
        }
        for (auto &fuel : parsed.fuels) {
            if (fuels.size() >= MaxFuels ||
                !fuelOwners.emplace(fuel.materialId,
                                    fuel.sourceName).second) {
                throw std::runtime_error(
                    "Duplicate or excessive smelting fuel in '" +
                    fuel.sourceName + "'.");
            }
            fuels.push_back(std::move(fuel));
        }
    }
    if (recipes.empty() || fuels.empty()) {
        throw std::runtime_error(
            "Smelting registry requires at least one recipe and fuel.");
    }
    std::sort(recipes.begin(), recipes.end(), [](const auto &left,
                                                 const auto &right) {
        return left.inputMaterialId < right.inputMaterialId;
    });
    std::sort(fuels.begin(), fuels.end(), [](const auto &left,
                                             const auto &right) {
        return left.materialId < right.materialId;
    });
    for (std::size_t index = 0; index < recipes.size(); ++index) {
        m_recipeByInput.emplace(recipes[index].inputMaterialId, index);
    }
    for (std::size_t index = 0; index < fuels.size(); ++index) {
        m_fuelByMaterial.emplace(fuels[index].materialId, index);
    }
    m_recipes = std::move(recipes);
    m_fuels = std::move(fuels);
    m_frozen = true;
}

void SmeltingRegistry::freezeFromResourceView(
    const ResourcePackResolver &resolver)
{
    if (!resolver.isFrozen()) {
        throw std::runtime_error(
            "Smelting loading requires a frozen resource view.");
    }
    std::vector<SmeltingSource> sources;
    for (const EffectiveResource &resource : resolver.effectiveResources()) {
        if (resource.category != "smelting") {
            continue;
        }
        if (!resource.packName.empty()) {
            throw std::runtime_error(
                "Resource-pack v1 cannot own smelting resource '" +
                resource.logicalPath + "'.");
        }
        std::ifstream input(resource.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) > MaxSourceBytes) {
            throw std::runtime_error(
                "Missing, empty or oversized base smelting resource '" +
                resource.logicalPath + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size)) {
            throw std::runtime_error(
                "Unable to read base smelting resource '" +
                resource.logicalPath + "'.");
        }
        sources.push_back({resource.logicalPath, std::move(content)});
    }
    freeze(std::move(sources));
}

bool SmeltingRegistry::isFrozen() const noexcept { return m_frozen; }
const std::vector<SmeltingRecipeDefinition> &
SmeltingRegistry::recipes() const noexcept { return m_recipes; }
const std::vector<SmeltingFuelDefinition> &
SmeltingRegistry::fuels() const noexcept { return m_fuels; }

const SmeltingRecipeDefinition *SmeltingRegistry::findRecipe(
    Material::ID inputMaterialId) const noexcept
{
    const auto found = m_recipeByInput.find(inputMaterialId);
    return found == m_recipeByInput.end() ? nullptr
                                          : &m_recipes[found->second];
}

const SmeltingFuelDefinition *SmeltingRegistry::findFuel(
    Material::ID materialId) const noexcept
{
    const auto found = m_fuelByMaterial.find(materialId);
    return found == m_fuelByMaterial.end() ? nullptr
                                            : &m_fuels[found->second];
}

SmeltingRegistry &runtimeSmeltingRegistry()
{
    static SmeltingRegistry registry;
    return registry;
}
