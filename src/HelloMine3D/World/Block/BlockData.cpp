#include "BlockData.h"

#include "../../Util/ResourcePaths.h"

#include <cctype>
#include <fstream>
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
    : BlockData(fileName, ResourcePaths::media("blocks"))
{
}

BlockData::BlockData(const std::string &fileName,
                     const std::string &blockDirectory)
{
    load(ResourcePaths::join(blockDirectory, fileName + ".block"));
}

void BlockData::load(const std::string &path)
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
            "MeshType",   "ShaderType",
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
                meshType > static_cast<int>(BlockMeshType::X)) {
                fail(path, key, "has invalid enum value " +
                                    std::to_string(meshType));
            }
            m_data.meshType = static_cast<BlockMeshType>(meshType);
        }
        else if (key == "ShaderType") {
            const int shaderType = parseInteger(path, key, value);
            if (shaderType < static_cast<int>(BlockShaderType::Chunk) ||
                shaderType > static_cast<int>(BlockShaderType::Flora)) {
                fail(path, key, "has invalid enum value " +
                                    std::to_string(shaderType));
            }
            m_data.shaderType = static_cast<BlockShaderType>(shaderType);
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
}

const BlockDataHolder &BlockData::getBlockData() const
{
    return m_data;
}

const std::string &BlockData::getSourcePath() const
{
    return m_sourcePath;
}
