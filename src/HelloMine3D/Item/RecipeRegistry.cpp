#include "RecipeRegistry.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../Util/ResourcePackResolver.h"

namespace
{
    constexpr const char *RecipeHeader =
        "# HelloMine3D recipe registry v1";

    struct PendingRecipe
    {
        RecipeDefinition definition;
        std::vector<std::vector<Material::ID>> rows;
        bool outputSeen = false;
        std::size_t startLine = 0;
    };

    std::string trim(const std::string &value)
    {
        std::size_t begin = 0;
        while (begin < value.size() &&
               std::isspace(static_cast<unsigned char>(value[begin]))) {
            ++begin;
        }
        std::size_t end = value.size();
        while (end > begin &&
               std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }
        return value.substr(begin, end - begin);
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
            "Invalid recipe source '" + source + "' at line " +
            std::to_string(line) + ": " + message);
    }

    bool isValidRecipeId(const std::string &value)
    {
        if (value.empty() ||
            value.size() > RecipeRegistry::MaxRecipeIdLength) {
            return false;
        }
        const std::size_t separator = value.find(':');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= value.size() ||
            value.find(':', separator + 1) != std::string::npos ||
            value[separator + 1] == '/' || value.back() == '/' ||
            value.find("//") != std::string::npos ||
            value.find("..") != std::string::npos) {
            return false;
        }
        const auto validNamespaceCharacter = [](unsigned char character)
        {
            return (character >= 'a' && character <= 'z') ||
                   (character >= '0' && character <= '9') ||
                   character == '_' || character == '-' || character == '.';
        };
        const auto validPathCharacter = [](unsigned char character)
        {
            return (character >= 'a' && character <= 'z') ||
                   (character >= '0' && character <= '9') ||
                   character == '_' || character == '-' || character == '.' ||
                   character == '/';
        };
        return std::all_of(value.begin(), value.begin() + separator,
                           validNamespaceCharacter) &&
               std::all_of(value.begin() + separator + 1, value.end(),
                           validPathCharacter);
    }

    int parseCount(const std::string &text, const std::string &source,
                   std::size_t line, const std::string &field)
    {
        int count = 0;
        try {
            std::size_t consumed = 0;
            count = std::stoi(text, &consumed);
            if (consumed != text.size()) {
                count = 0;
            }
        }
        catch (...) {
            count = 0;
        }
        if (count <= 0 || count > RecipeRegistry::MaxIngredientUnits) {
            fail(source, line,
                 field + " count must be in [1, " +
                     std::to_string(RecipeRegistry::MaxIngredientUnits) +
                     "].");
        }
        return count;
    }

    Material::ID parseMaterial(const std::string &text,
                               const std::string &source,
                               std::size_t line)
    {
        Material::ID id = Material::ID::Nothing;
        if (!Material::tryParseStringId(text, id) ||
            id == Material::ID::Nothing) {
            fail(source, line, "unknown or empty material id '" + text +
                                   "'.");
        }
        return id;
    }

    std::string shapedPatternKey(PendingRecipe &pending,
                                 const std::string &source,
                                 std::size_t line)
    {
        if (pending.rows.empty()) {
            fail(source, line, "shaped recipe has no rows.");
        }
        const int sourceHeight = static_cast<int>(pending.rows.size());
        const int sourceWidth = static_cast<int>(pending.rows.front().size());
        int minimumX = sourceWidth;
        int minimumY = sourceHeight;
        int maximumX = -1;
        int maximumY = -1;
        for (int y = 0; y < sourceHeight; ++y) {
            for (int x = 0; x < sourceWidth; ++x) {
                if (pending.rows[y][x] != Material::ID::Nothing) {
                    minimumX = std::min(minimumX, x);
                    minimumY = std::min(minimumY, y);
                    maximumX = std::max(maximumX, x);
                    maximumY = std::max(maximumY, y);
                }
            }
        }
        if (maximumX < 0 || maximumY < 0) {
            fail(source, line, "shaped recipe contains only empty cells.");
        }

        pending.definition.width = maximumX - minimumX + 1;
        pending.definition.height = maximumY - minimumY + 1;
        std::map<Material::ID, int> counts;
        std::ostringstream key;
        key << "shaped:" << pending.definition.width << 'x'
            << pending.definition.height << ':';
        for (int y = minimumY; y <= maximumY; ++y) {
            for (int x = minimumX; x <= maximumX; ++x) {
                const Material::ID material = pending.rows[y][x];
                pending.definition.shapedCells.push_back(material);
                key << static_cast<int>(material) << ',';
                if (material != Material::ID::Nothing) {
                    ++counts[material];
                }
            }
        }
        for (const auto &entry : counts) {
            pending.definition.ingredients.push_back(
                {entry.first, entry.second});
        }
        return key.str();
    }

    std::string shapelessPatternKey(PendingRecipe &pending,
                                    const std::string &source,
                                    std::size_t line)
    {
        if (pending.definition.ingredients.empty()) {
            fail(source, line, "shapeless recipe has no inputs.");
        }
        std::sort(pending.definition.ingredients.begin(),
                  pending.definition.ingredients.end(),
                  [](const RecipeIngredient &left,
                     const RecipeIngredient &right)
                  {
                      return left.materialId < right.materialId;
                  });
        int totalUnits = 0;
        for (const RecipeIngredient &ingredient :
             pending.definition.ingredients) {
            totalUnits += ingredient.count;
        }
        if (totalUnits > RecipeRegistry::MaxIngredientUnits) {
            fail(source, line,
                 "shapeless input total exceeds " +
                     std::to_string(RecipeRegistry::MaxIngredientUnits) +
                     " units.");
        }
        std::ostringstream key;
        key << "shapeless:";
        for (const RecipeIngredient &ingredient :
             pending.definition.ingredients) {
            key << static_cast<int>(ingredient.materialId) << 'x'
                << ingredient.count << ',';
        }
        return key.str();
    }

    std::pair<RecipeDefinition, std::string> finalizeRecipe(
        PendingRecipe pending, const std::string &source, std::size_t line)
    {
        if (!pending.outputSeen) {
            fail(source, line, "recipe is missing its output.");
        }
        std::string pattern;
        if (pending.definition.type == RecipeType::Shaped) {
            pattern = shapedPatternKey(pending, source, line);
        }
        else {
            pattern = shapelessPatternKey(pending, source, line);
        }
        return {std::move(pending.definition), std::move(pattern)};
    }

    std::vector<std::pair<RecipeDefinition, std::string>> parseSource(
        const RecipeSource &source)
    {
        std::istringstream input(source.content);
        std::vector<std::pair<RecipeDefinition, std::string>> result;
        PendingRecipe pending;
        bool insideRecipe = false;
        std::string line;
        std::size_t lineNumber = 0;
        bool headerSeen = false;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (lineNumber == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf) {
                line.erase(0, 3);
            }
            line = trim(line);
            if (!headerSeen) {
                if (line != RecipeHeader) {
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
            if (!insideRecipe) {
                if (parts.size() != 3 || parts[0] != "recipe" ||
                    !isValidRecipeId(parts[1]) ||
                    (parts[2] != "shaped" && parts[2] != "shapeless")) {
                    fail(source.name, lineNumber,
                         "expected 'recipe namespace:id shaped|shapeless'.");
                }
                pending = PendingRecipe();
                pending.definition.id = parts[1];
                pending.definition.type = parts[2] == "shaped"
                                              ? RecipeType::Shaped
                                              : RecipeType::Shapeless;
                pending.definition.sourceName = source.name;
                pending.startLine = lineNumber;
                insideRecipe = true;
                continue;
            }

            if (parts[0] == "row") {
                if (pending.definition.type != RecipeType::Shaped ||
                    parts.size() < 2 ||
                    parts.size() > static_cast<std::size_t>(
                                       RecipeRegistry::MaxGridDimension + 1) ||
                    pending.rows.size() >= static_cast<std::size_t>(
                                               RecipeRegistry::MaxGridDimension)) {
                    fail(source.name, lineNumber,
                         "invalid or out-of-bounds shaped row.");
                }
                if (!pending.rows.empty() &&
                    pending.rows.front().size() != parts.size() - 1) {
                    fail(source.name, lineNumber,
                         "shaped rows must have equal widths.");
                }
                std::vector<Material::ID> row;
                for (std::size_t index = 1; index < parts.size(); ++index) {
                    row.push_back(parts[index] == "_"
                                      ? Material::ID::Nothing
                                      : parseMaterial(parts[index], source.name,
                                                      lineNumber));
                }
                pending.rows.push_back(std::move(row));
                continue;
            }

            if (parts[0] == "input") {
                if (pending.definition.type != RecipeType::Shapeless ||
                    parts.size() != 3 ||
                    pending.definition.ingredients.size() >=
                        static_cast<std::size_t>(
                            RecipeRegistry::MaxIngredientEntries)) {
                    fail(source.name, lineNumber,
                         "invalid or out-of-bounds shapeless input.");
                }
                const Material::ID material =
                    parseMaterial(parts[1], source.name, lineNumber);
                if (std::any_of(
                        pending.definition.ingredients.begin(),
                        pending.definition.ingredients.end(),
                        [material](const RecipeIngredient &ingredient)
                        {
                            return ingredient.materialId == material;
                        })) {
                    fail(source.name, lineNumber,
                         "duplicate shapeless material '" + parts[1] +
                             "'.");
                }
                pending.definition.ingredients.push_back(
                    {material,
                     parseCount(parts[2], source.name, lineNumber, "input")});
                continue;
            }

            if (parts[0] == "output") {
                if (parts.size() != 3 || pending.outputSeen) {
                    fail(source.name, lineNumber,
                         "recipe must contain exactly one output.");
                }
                const Material::ID material =
                    parseMaterial(parts[1], source.name, lineNumber);
                const int count = parseCount(parts[2], source.name,
                                             lineNumber, "output");
                if (count > Material::toMaterial(material).maxStackSize) {
                    fail(source.name, lineNumber,
                         "output count exceeds the material stack limit.");
                }
                pending.definition.outputMaterialId = material;
                pending.definition.outputCount = count;
                pending.outputSeen = true;
                continue;
            }

            if (parts.size() == 1 && parts[0] == "end") {
                result.push_back(finalizeRecipe(
                    std::move(pending), source.name, lineNumber));
                insideRecipe = false;
                continue;
            }

            fail(source.name, lineNumber,
                 "unknown or misplaced recipe directive '" + parts[0] +
                     "'.");
        }
        if (!headerSeen) {
            fail(source.name, 1, "empty source.");
        }
        if (insideRecipe) {
            fail(source.name, pending.startLine,
                 "recipe is missing its 'end' directive.");
        }
        return result;
    }
}

void RecipeRegistry::freeze(std::vector<RecipeSource> sources)
{
    if (m_frozen) {
        throw std::runtime_error(
            "The recipe registry is already frozen for this process.");
    }
    if (sources.empty() || sources.size() > MaxSourceCount) {
        throw std::runtime_error(
            "Recipe source count must be in [1, " +
            std::to_string(MaxSourceCount) + "].");
    }
    std::sort(sources.begin(), sources.end(),
              [](const RecipeSource &left, const RecipeSource &right)
              {
                  return left.name < right.name;
              });

    std::vector<RecipeDefinition> recipes;
    std::map<std::string, std::string> idOwners;
    std::map<std::string, std::string> patternOwners;
    std::set<std::string> sourceNames;
    for (const RecipeSource &source : sources) {
        if (source.name.empty() || !sourceNames.insert(source.name).second) {
            throw std::runtime_error(
                "Recipe source names must be non-empty and unique: '" +
                source.name + "'.");
        }
        if (source.content.empty() || source.content.size() > MaxSourceBytes) {
            throw std::runtime_error(
                "Recipe source '" + source.name +
                "' is empty or exceeds " + std::to_string(MaxSourceBytes) +
                " bytes.");
        }
        for (auto &parsed : parseSource(source)) {
            if (recipes.size() >= MaxRecipes) {
                throw std::runtime_error(
                    "Recipe registry exceeds " +
                    std::to_string(MaxRecipes) + " entries.");
            }
            const auto idOwner = idOwners.emplace(
                parsed.first.id, parsed.first.sourceName);
            if (!idOwner.second) {
                throw std::runtime_error(
                    "Duplicate recipe id '" + parsed.first.id +
                    "' in '" + parsed.first.sourceName +
                    "'; first declared in '" + idOwner.first->second +
                    "'.");
            }
            const auto patternOwner = patternOwners.emplace(
                parsed.second, parsed.first.id);
            if (!patternOwner.second) {
                throw std::runtime_error(
                    "Duplicate recipe pattern for '" + parsed.first.id +
                    "'; first declared by '" + patternOwner.first->second +
                    "'.");
            }
            recipes.push_back(std::move(parsed.first));
        }
    }
    if (recipes.empty()) {
        throw std::runtime_error("Recipe registry contains no recipes.");
    }
    std::sort(recipes.begin(), recipes.end(),
              [](const RecipeDefinition &left,
                 const RecipeDefinition &right)
              {
                  return left.id < right.id;
              });
    std::unordered_map<std::string, std::size_t> byId;
    for (std::size_t index = 0; index < recipes.size(); ++index) {
        byId.emplace(recipes[index].id, index);
    }
    m_recipes = std::move(recipes);
    m_byId = std::move(byId);
    m_frozen = true;
}

void RecipeRegistry::freezeFromResourceView(
    const ResourcePackResolver &resolver)
{
    if (!resolver.isFrozen()) {
        throw std::runtime_error(
            "Recipe loading requires a frozen effective resource view.");
    }
    std::vector<RecipeSource> sources;
    for (const EffectiveResource &resource : resolver.effectiveResources()) {
        if (resource.category != "recipe") {
            continue;
        }
        if (!resource.packName.empty()) {
            throw std::runtime_error(
                "Resource-pack v1 cannot own recipe resource '" +
                resource.logicalPath + "'.");
        }
        if (sources.size() >= MaxSourceCount) {
            throw std::runtime_error(
                "Recipe resource view exceeds " +
                std::to_string(MaxSourceCount) + " sources.");
        }
        std::ifstream input(resource.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) > MaxSourceBytes) {
            throw std::runtime_error(
                "Missing, empty or oversized base recipe resource '" +
                resource.logicalPath + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size)) {
            throw std::runtime_error(
                "Unable to read base recipe resource '" +
                resource.logicalPath + "'.");
        }
        sources.push_back({resource.logicalPath, std::move(content)});
    }
    freeze(std::move(sources));
}

bool RecipeRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

const std::vector<RecipeDefinition> &RecipeRegistry::recipes() const noexcept
{
    return m_recipes;
}

const RecipeDefinition *RecipeRegistry::find(
    const std::string &id) const noexcept
{
    const auto found = m_byId.find(id);
    return found == m_byId.end() ? nullptr : &m_recipes[found->second];
}

RecipeRegistry &runtimeRecipeRegistry()
{
    static RecipeRegistry registry;
    return registry;
}
