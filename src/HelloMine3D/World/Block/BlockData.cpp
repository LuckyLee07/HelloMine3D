#include "BlockData.h"

#include "../../Util/ResourcePaths.h"
#include "../../Util/ResourcePackResolver.h"

#include <cctype>
#include <fstream>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
constexpr int AtlasTilesPerAxis = 16;

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

[[noreturn]] void fail(const std::string &path, const std::string &key,
                       const std::string &detail)
{
    throw std::runtime_error("Invalid block file '" + path +
                             "': key '" + key + "' " + detail + ".");
}

int parseInteger(const std::string &path, const std::string &key,
                 const std::string &value)
{
    std::istringstream input(value);
    int parsed = 0;
    if (!(input >> parsed)) {
        fail(path, key, "must be an integer");
    }
    input >> std::ws;
    if (!input.eof()) {
        fail(path, key, "contains trailing data");
    }
    return parsed;
}

bool parseBoolean(const std::string &path, const std::string &key,
                  const std::string &value)
{
    const int parsed = parseInteger(path, key, value);
    if (parsed != 0 && parsed != 1) {
        fail(path, key, "must be 0 or 1");
    }
    return parsed != 0;
}

glm::ivec2 parseAtlasCoordinate(const std::string &path,
                                const std::string &key,
                                const std::string &value)
{
    std::istringstream input(value);
    glm::ivec2 coordinate{0};
    if (!(input >> coordinate.x >> coordinate.y)) {
        fail(path, key, "must contain two integer atlas coordinates");
    }
    input >> std::ws;
    if (!input.eof()) {
        fail(path, key, "contains trailing data");
    }
    if (coordinate.x < 0 || coordinate.x >= AtlasTilesPerAxis ||
        coordinate.y < 0 || coordinate.y >= AtlasTilesPerAxis) {
        fail(path, key, "has an atlas coordinate outside [0, 15]");
    }
    return coordinate;
}
} // namespace

BlockData::BlockData(const std::string &fileName)
{
    load(runtimeResourcePackResolver().resolve(
             "media/blocks/" + fileName + ".block"),
         "");
}

float parseFloat(const std::string &path, const std::string &key,
                 const std::string &value)
{
    std::istringstream input(value);
    float parsed = 0.0f;
    if (!(input >> parsed)) {
        fail(path, key, "must be a number");
    }
    input >> std::ws;
    if (!input.eof()) {
        fail(path, key, "contains trailing data");
    }
    if (!std::isfinite(parsed) || parsed < 0.05f || parsed > 60.0f) {
        fail(path, key, "is outside [0.05, 60]");
    }
    return parsed;
}

BlockData::BlockData(const std::string &fileName,
                     const std::string &blockDirectory)
    : BlockData(fileName, blockDirectory,
                ResourcePaths::media("shapes"))
{
}

BlockData::BlockData(const std::string &fileName,
                     const std::string &blockDirectory,
                     const std::string &shapeDirectory)
{
    load(ResourcePaths::join(blockDirectory, fileName + ".block"),
         shapeDirectory);
}

void BlockData::load(const std::string &path,
                     const std::string &shapeDirectory)
{
    m_sourcePath = path;
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open block file '" + path + "'.");
    }

    std::set<std::string> seenKeys;
    bool hasTopTexture = false;
    bool hasSideTexture = false;
    bool hasBottomTexture = false;
    std::string shapeName;

    std::string keyLine;
    std::size_t lineNumber = 0;
    while (std::getline(input, keyLine)) {
        ++lineNumber;
        const std::string key = trim(keyLine);
        if (key.empty()) {
            continue;
        }

        static const std::set<std::string> validKeys = {
            "Name",       "Id",         "TexTop",   "TexSide",
            "TexBottom",  "TexAll",     "Opaque",   "Collidable",
            "MeshType",   "ShaderType", "Shape",     "Light",
            "Hardness",   "MiningClass", "RequiredToolTier",
            "WrongToolDrops",
        };
        if (validKeys.find(key) == validKeys.end()) {
            fail(path, key, "is unknown at line " +
                                std::to_string(lineNumber));
        }
        if (!seenKeys.insert(key).second) {
            fail(path, key, "is duplicated at line " +
                                std::to_string(lineNumber));
        }

        std::string valueLine;
        if (!std::getline(input, valueLine)) {
            fail(path, key, "is missing a value");
        }
        ++lineNumber;
        const std::string value = trim(valueLine);
        if (value.empty()) {
            fail(path, key, "is missing a value at line " +
                                std::to_string(lineNumber));
        }

        if (key == "Name") {
            m_data.name = value;
        }
        else if (key == "Id") {
            const int id = parseInteger(path, key, value);
            if (id < 0 || id >= static_cast<int>(BlockId::NUM_TYPES)) {
                fail(path, key, "is outside the registered block id range");
            }
            m_data.id = static_cast<BlockId>(id);
        }
        else if (key == "TexTop") {
            m_data.texTopCoord = parseAtlasCoordinate(path, key, value);
            hasTopTexture = true;
        }
        else if (key == "TexSide") {
            m_data.texSideCoord = parseAtlasCoordinate(path, key, value);
            hasSideTexture = true;
        }
        else if (key == "TexBottom") {
            m_data.texBottomCoord = parseAtlasCoordinate(path, key, value);
            hasBottomTexture = true;
        }
        else if (key == "TexAll") {
            const glm::ivec2 coordinate =
                parseAtlasCoordinate(path, key, value);
            m_data.texTopCoord = coordinate;
            m_data.texSideCoord = coordinate;
            m_data.texBottomCoord = coordinate;
            hasTopTexture = true;
            hasSideTexture = true;
            hasBottomTexture = true;
        }
        else if (key == "Opaque") {
            m_data.isOpaque = parseBoolean(path, key, value);
        }
        else if (key == "Collidable") {
            m_data.isCollidable = parseBoolean(path, key, value);
        }
        else if (key == "MeshType") {
            const int meshType = parseInteger(path, key, value);
            if (meshType < static_cast<int>(BlockMeshType::Cube) ||
                meshType > static_cast<int>(BlockMeshType::Resource)) {
                fail(path, key, "has invalid enum value " +
                                    std::to_string(meshType));
            }
            m_data.meshType = static_cast<BlockMeshType>(meshType);
        }
        else if (key == "Shape") {
            shapeName = value;
        }
        else if (key == "ShaderType") {
            const int shaderType = parseInteger(path, key, value);
            if (shaderType < static_cast<int>(BlockShaderType::Chunk) ||
                shaderType >
                    static_cast<int>(BlockShaderType::Transparent)) {
                fail(path, key, "has invalid enum value " +
                                    std::to_string(shaderType));
            }
            m_data.shaderType = static_cast<BlockShaderType>(shaderType);
        }
        else if (key == "Light") {
            const int light = parseInteger(path, key, value);
            if (light < 0 || light > 15) {
                fail(path, key, "is outside [0, 15]");
            }
            m_data.light = light;
        }
        else if (key == "Hardness") {
            m_data.hardnessSeconds = parseFloat(path, key, value);
        }
        else if (key == "MiningClass") {
            if (!ToolRegistry::tryParseMiningClass(
                    value, m_data.miningClass)) {
                fail(path, key, "has invalid value '" + value + "'");
            }
        }
        else if (key == "RequiredToolTier") {
            const int tier = parseInteger(path, key, value);
            if (tier < 0 || tier > ToolRegistry::MaxTier) {
                fail(path, key, "is outside [0, " +
                                    std::to_string(ToolRegistry::MaxTier) +
                                    "]");
            }
            m_data.requiredToolTier = tier;
        }
        else if (key == "WrongToolDrops") {
            m_data.wrongToolDrops = parseBoolean(path, key, value);
        }
    }

    const auto requireKey = [&](const std::string &key, bool present) {
        if (!present) {
            fail(path, key, "is missing");
        }
    };
    requireKey("Name", seenKeys.find("Name") != seenKeys.end());
    requireKey("Id", seenKeys.find("Id") != seenKeys.end());
    requireKey("TexTop or TexAll", hasTopTexture);
    requireKey("TexSide or TexAll", hasSideTexture);
    requireKey("TexBottom or TexAll", hasBottomTexture);
    requireKey("Opaque", seenKeys.find("Opaque") != seenKeys.end());
    requireKey("Collidable",
               seenKeys.find("Collidable") != seenKeys.end());
    requireKey("MeshType", seenKeys.find("MeshType") != seenKeys.end());
    requireKey("ShaderType",
               seenKeys.find("ShaderType") != seenKeys.end());
    requireKey("Light", seenKeys.find("Light") != seenKeys.end());

    const bool hasShape = seenKeys.find("Shape") != seenKeys.end();
    if (m_data.meshType == BlockMeshType::Resource) {
        requireKey("Shape", hasShape);
        if (shapeDirectory.empty()) {
            m_data.shape = loadBlockShapeFile(
                shapeName, runtimeResourcePackResolver().resolve(
                               "media/shapes/" + shapeName + ".shape"));
        }
        else {
            m_data.shape = loadBlockShape(shapeName, shapeDirectory);
        }
    }
    else if (hasShape) {
        fail(path, "Shape", "is only valid for resource meshes");
    }

    if (m_data.miningClass == MiningClass::None &&
        m_data.requiredToolTier != 0) {
        fail(path, "RequiredToolTier",
             "must be 0 when MiningClass is none");
    }
}

const BlockDataHolder &BlockData::getBlockData() const
{
    return m_data;
}

const std::string &BlockData::getSourcePath() const
{
    return m_sourcePath;
}
