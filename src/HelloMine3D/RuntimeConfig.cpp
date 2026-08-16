#include "RuntimeConfig.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
[[noreturn]] void fail(const std::string &path, const std::string &key,
                       const std::string &detail)
{
    throw std::runtime_error("Invalid runtime config '" + path +
                             "': key '" + key + "' " + detail + ".");
}

void requireEnd(const std::string &path, const std::string &key,
                std::istringstream &input)
{
    input >> std::ws;
    if (!input.eof()) {
        fail(path, key, "contains trailing data");
    }
}

int readInteger(const std::string &path, const std::string &key,
                std::istringstream &input)
{
    int value = 0;
    if (!(input >> value)) {
        fail(path, key, "must contain an integer");
    }
    return value;
}

float readFloat(const std::string &path, const std::string &key,
                std::istringstream &input)
{
    float value = 0.f;
    if (!(input >> value) || !std::isfinite(value)) {
        fail(path, key, "must contain a finite number");
    }
    return value;
}

void writeDefaultConfig(const std::string &path, const Config &config)
{
    const std::filesystem::path filePath(path);
    const std::filesystem::path parent = filePath.parent_path();
    std::error_code error;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            throw std::runtime_error("Unable to create runtime config directory '" +
                                     parent.string() + "'.");
        }
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Unable to write runtime config '" + path +
                                 "'.");
    }
    output << "renderdistance " << config.renderDistance << '\n'
           << "fullscreen " << (config.isFullscreen ? 1 : 0) << '\n'
           << "windowsize " << config.windowX << ' ' << config.windowY
           << '\n'
           << "fov " << config.fov << '\n'
           << "mousesensitivity " << config.mouseSensitivity << '\n'
           << "invertmousey " << (config.invertMouseY ? 1 : 0) << '\n'
           << "seed ";
    if (config.worldSeed.has_value()) {
        output << *config.worldSeed;
    }
    else {
        output << "random";
    }
    output << '\n';
    if (!output) {
        throw std::runtime_error("Unable to write runtime config '" + path +
                                 "'.");
    }
}
} // namespace

Config loadRuntimeConfig(const std::string &path)
{
    Config config;
    std::ifstream input(path);
    if (!input.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(path, error)) {
            throw std::runtime_error("Unable to read runtime config '" + path +
                                     "'.");
        }
        writeDefaultConfig(path, config);
        input.open(path);
    }
    if (!input.is_open()) {
        throw std::runtime_error("Unable to read runtime config '" + path +
                                 "'.");
    }

    std::set<std::string> seenKeys;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }

        std::istringstream values(line);
        std::string key;
        if (!(values >> key)) {
            continue;
        }
        if (!seenKeys.insert(key).second) {
            fail(path, key, "is duplicated at line " +
                                std::to_string(lineNumber));
        }

        if (key == "renderdistance") {
            config.renderDistance = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (config.renderDistance < 0 || config.renderDistance > 64) {
                fail(path, key, "must be between 0 and 64");
            }
        }
        else if (key == "fullscreen") {
            const int fullscreen = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (fullscreen != 0 && fullscreen != 1) {
                fail(path, key, "must be 0 or 1");
            }
            config.isFullscreen = fullscreen != 0;
        }
        else if (key == "windowsize") {
            config.windowX = readInteger(path, key, values);
            config.windowY = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (config.windowX <= 0 || config.windowY <= 0) {
                fail(path, key, "must contain positive dimensions");
            }
        }
        else if (key == "fov") {
            config.fov = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (config.fov <= 0 || config.fov >= 180) {
                fail(path, key, "must be between 1 and 179 degrees");
            }
        }
        else if (key == "mousesensitivity") {
            config.mouseSensitivity = readFloat(path, key, values);
            requireEnd(path, key, values);
            if (config.mouseSensitivity < 0.005f ||
                config.mouseSensitivity > 1.f) {
                fail(path, key, "must be between 0.005 and 1.0");
            }
        }
        else if (key == "invertmousey") {
            const int inverted = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (inverted != 0 && inverted != 1) {
                fail(path, key, "must be 0 or 1");
            }
            config.invertMouseY = inverted != 0;
        }
        else if (key == "seed") {
            std::string seedText;
            if (!(values >> seedText)) {
                fail(path, key, "must be an integer or 'random'");
            }
            requireEnd(path, key, values);
            if (seedText == "random") {
                config.worldSeed.reset();
            }
            else {
                std::istringstream seedValue(seedText);
                config.worldSeed = readInteger(path, key, seedValue);
                requireEnd(path, key, seedValue);
            }
        }
        else {
            fail(path, key, "is unknown at line " +
                                std::to_string(lineNumber));
        }
    }
    return config;
}
