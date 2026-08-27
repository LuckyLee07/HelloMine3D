#include "TerrainMaterialProfile.h"

#include "../../Util/ResourcePackResolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr const char *ProfileHeader =
        "# HelloMine3D terrain material parameters v1";

    std::string trim(const std::string &value)
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

    [[noreturn]] void fail(const std::string &path,
                           const std::string &detail)
    {
        throw std::runtime_error(
            "Invalid terrain material parameter resource '" + path +
            "': " + detail + ".");
    }

    int parseInteger(const std::string &path, const std::string &key,
                     const std::string &value)
    {
        std::istringstream input(value);
        int parsed = 0;
        if (!(input >> parsed))
        {
            fail(path, "key '" + key + "' must be an integer");
        }
        input >> std::ws;
        if (!input.eof())
        {
            fail(path, "key '" + key + "' contains trailing data");
        }
        return parsed;
    }

    float parseFloat(const std::string &path, const std::string &key,
                     const std::string &value)
    {
        std::istringstream input(value);
        float parsed = 0.f;
        if (!(input >> parsed))
        {
            fail(path, "key '" + key + "' must be a number");
        }
        input >> std::ws;
        if (!input.eof())
        {
            fail(path, "key '" + key + "' contains trailing data");
        }
        if (!std::isfinite(parsed))
        {
            fail(path, "key '" + key + "' must be finite");
        }
        return parsed;
    }

    std::uint32_t readBigEndian(const unsigned char *bytes)
    {
        return (static_cast<std::uint32_t>(bytes[0]) << 24u) |
               (static_cast<std::uint32_t>(bytes[1]) << 16u) |
               (static_cast<std::uint32_t>(bytes[2]) << 8u) |
               static_cast<std::uint32_t>(bytes[3]);
    }

    std::pair<int, int> readPngDimensions(
        const std::string &logicalPath, const std::string &resolvedPath)
    {
        std::ifstream input(resolvedPath, std::ios::binary);
        std::array<unsigned char, 24> header{};
        if (!input || !input.read(
                reinterpret_cast<char *>(header.data()), header.size()))
        {
            throw std::runtime_error(
                "Missing terrain atlas resource '" + logicalPath +
                "': expected a readable PNG at '" + resolvedPath + "'.");
        }
        constexpr std::array<unsigned char, 8> Signature = {
            0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au};
        if (!std::equal(Signature.begin(), Signature.end(), header.begin()) ||
            header[12] != 'I' || header[13] != 'H' ||
            header[14] != 'D' || header[15] != 'R')
        {
            throw std::runtime_error(
                "Invalid terrain atlas resource '" + logicalPath +
                "' at '" + resolvedPath +
                "': expected a PNG IHDR header.");
        }
        const std::uint32_t width = readBigEndian(header.data() + 16);
        const std::uint32_t height = readBigEndian(header.data() + 20);
        if (width == 0 || height == 0 || width > 8192u || height > 8192u)
        {
            throw std::runtime_error(
                "Invalid terrain atlas resource '" + logicalPath +
                "' at '" + resolvedPath +
                "': dimensions must be within [1, 8192].");
        }
        return {static_cast<int>(width), static_cast<int>(height)};
    }

    void validateShaderInterface(const std::string &path)
    {
        std::ifstream input(path);
        if (!input)
        {
            throw std::runtime_error(
                "Missing terrain material shader resource '" + path +
                "'.");
        }
        static const std::set<std::string> required = {
            "uniform float atlasPixels;",
            "uniform float tilePixels;",
            "uniform float tilesPerRow;",
            "uniform float colourSaturation;",
            "uniform float greenSuppression;",
            "uniform float greenRedShift;",
            "uniform float toneGamma;",
        };
        std::set<std::string> found;
        std::string line;
        while (std::getline(input, line))
        {
            line = trim(line);
            if (required.find(line) != required.end())
            {
                found.insert(line);
            }
        }
        for (const std::string &declaration : required)
        {
            if (found.find(declaration) == found.end())
            {
                throw std::runtime_error(
                    "Invalid terrain material shader resource '" + path +
                    "': missing interface declaration '" + declaration +
                    "'.");
            }
        }
    }
}

bool TerrainMaterialParameters::containsTile(int x, int y) const noexcept
{
    return x >= 0 && y >= 0 && x < tilesPerRow && y < tilesPerRow;
}

TerrainMaterialParameters loadTerrainMaterialParameters(
    const std::string &parameterPath,
    const TerrainResourceResolver &resolveResource)
{
    if (!resolveResource)
    {
        throw std::invalid_argument(
            "Terrain material resource resolver must be callable.");
    }
    std::ifstream input(parameterPath);
    if (!input)
    {
        throw std::runtime_error(
            "Missing terrain material parameter resource: expected '" +
            parameterPath + "'.");
    }

    std::string header;
    std::getline(input, header);
    if (!header.empty() && header.back() == '\r')
    {
        header.pop_back();
    }
    if (header != ProfileHeader)
    {
        fail(parameterPath, "unsupported or missing header");
    }

    static const std::set<std::string> keys = {
        "atlas_texture", "atlas_pixels", "tile_pixels", "tiles_per_row",
        "colour_saturation", "green_suppression", "green_red_shift",
        "tone_gamma"};
    std::map<std::string, std::string> values;
    std::string line;
    std::size_t lineNumber = 1;
    while (std::getline(input, line))
    {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line.front() == '#')
        {
            continue;
        }
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= line.size() ||
            line.find('=', separator + 1) != std::string::npos)
        {
            fail(parameterPath, "line " + std::to_string(lineNumber) +
                                    " must be key=value");
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (keys.find(key) == keys.end())
        {
            fail(parameterPath, "key '" + key + "' is unknown at line " +
                                    std::to_string(lineNumber));
        }
        if (value.empty())
        {
            fail(parameterPath, "key '" + key + "' is empty at line " +
                                    std::to_string(lineNumber));
        }
        if (!values.emplace(key, value).second)
        {
            fail(parameterPath, "key '" + key +
                                    "' is duplicated at line " +
                                    std::to_string(lineNumber));
        }
    }
    for (const std::string &key : keys)
    {
        if (values.find(key) == values.end())
        {
            fail(parameterPath, "key '" + key + "' is missing");
        }
    }

    TerrainMaterialParameters result;
    result.atlasTexture = values["atlas_texture"];
    if (result.atlasTexture !=
        TerrainMaterialParameters::DefaultAtlasLogicalPath)
    {
        fail(parameterPath,
             "key 'atlas_texture' must remain '" +
                 std::string(TerrainMaterialParameters::
                                 DefaultAtlasLogicalPath) +
                 "' for contract version 1");
    }
    result.atlasPixels = parseInteger(
        parameterPath, "atlas_pixels", values["atlas_pixels"]);
    result.tilePixels = parseInteger(
        parameterPath, "tile_pixels", values["tile_pixels"]);
    result.tilesPerRow = parseInteger(
        parameterPath, "tiles_per_row", values["tiles_per_row"]);
    result.colourSaturation = parseFloat(
        parameterPath, "colour_saturation",
        values["colour_saturation"]);
    result.greenSuppression = parseFloat(
        parameterPath, "green_suppression",
        values["green_suppression"]);
    result.greenRedShift = parseFloat(
        parameterPath, "green_red_shift", values["green_red_shift"]);
    result.toneGamma = parseFloat(
        parameterPath, "tone_gamma", values["tone_gamma"]);

    if (result.atlasPixels < 16 || result.atlasPixels > 8192)
    {
        fail(parameterPath, "key 'atlas_pixels' is outside [16, 8192]");
    }
    if (result.tilePixels < 2 ||
        result.tilePixels > result.atlasPixels)
    {
        fail(parameterPath,
             "key 'tile_pixels' is outside [2, atlas_pixels]");
    }
    if (result.tilesPerRow < 1 || result.tilesPerRow > 256)
    {
        fail(parameterPath, "key 'tiles_per_row' is outside [1, 256]");
    }
    if (result.atlasPixels % result.tilePixels != 0)
    {
        fail(parameterPath,
             "atlas_pixels must be evenly divisible by tile_pixels");
    }
    if (result.atlasPixels / result.tilePixels != result.tilesPerRow)
    {
        fail(parameterPath,
             "tiles_per_row must equal atlas_pixels / tile_pixels");
    }
    const auto requireUnitRange = [&](const char *key, float value)
    {
        if (value < 0.f || value > 1.f)
        {
            fail(parameterPath, "key '" + std::string(key) +
                                    "' is outside [0, 1]");
        }
    };
    requireUnitRange("colour_saturation", result.colourSaturation);
    requireUnitRange("green_suppression", result.greenSuppression);
    requireUnitRange("green_red_shift", result.greenRedShift);
    if (result.toneGamma < 0.5f || result.toneGamma > 2.f)
    {
        fail(parameterPath, "key 'tone_gamma' is outside [0.5, 2]");
    }

    const std::string atlasPath = resolveResource(result.atlasTexture);
    const std::pair<int, int> dimensions =
        readPngDimensions(result.atlasTexture, atlasPath);
    if (dimensions.first != result.atlasPixels ||
        dimensions.second != result.atlasPixels)
    {
        throw std::runtime_error(
            "Invalid terrain atlas resource '" + result.atlasTexture +
            "' at '" + atlasPath + "': expected " +
            std::to_string(result.atlasPixels) + "x" +
            std::to_string(result.atlasPixels) + " pixels, got " +
            std::to_string(dimensions.first) + "x" +
            std::to_string(dimensions.second) + ".");
    }
    validateShaderInterface(resolveResource(
        TerrainMaterialParameters::TerrainShaderLogicalPath));
    return result;
}

void RuntimeTerrainMaterialProfile::freezeFromResourceView(
    const ResourcePackResolver &resolver)
{
    if (m_frozen)
    {
        throw std::runtime_error(
            "The runtime terrain material profile is already frozen.");
    }
    const std::string parameterPath =
        resolver.resolve(TerrainMaterialParameters::LogicalPath);
    TerrainMaterialParameters parsed = loadTerrainMaterialParameters(
        parameterPath,
        [&resolver](const std::string &logicalPath)
        {
            return resolver.resolve(logicalPath);
        });
    m_parameters = std::move(parsed);
    m_frozen = true;
}

bool RuntimeTerrainMaterialProfile::isFrozen() const noexcept
{
    return m_frozen;
}

const TerrainMaterialParameters &
RuntimeTerrainMaterialProfile::parameters() const noexcept
{
    return m_parameters;
}

RuntimeTerrainMaterialProfile &runtimeTerrainMaterialProfile()
{
    static RuntimeTerrainMaterialProfile profile;
    return profile;
}
