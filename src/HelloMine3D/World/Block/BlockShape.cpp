#include "BlockShape.h"

#include "../../Util/ResourcePaths.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
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
    throw std::runtime_error("Invalid block shape file '" + path +
                             "': key '" + key + "' " + detail + ".");
}

bool isValidName(const std::string &name)
{
    if (name.empty()) {
        return false;
    }
    for (char value : name) {
        if (!std::isalnum(static_cast<unsigned char>(value)) &&
            value != '_' && value != '-') {
            return false;
        }
    }
    return true;
}

BlockShapeFace parseFace(const std::string &path,
                         const std::string &value)
{
    std::istringstream input(value);
    BlockShapeFace face{};
    for (float &coordinate : face) {
        if (!(input >> coordinate)) {
            fail(path, "Face", "must contain 12 coordinates");
        }
        if (!std::isfinite(coordinate) || coordinate < 0.f ||
            coordinate > 1.f) {
            fail(path, "Face", "has a coordinate outside [0, 1]");
        }
    }
    input >> std::ws;
    if (!input.eof()) {
        fail(path, "Face", "contains trailing data");
    }
    return face;
}
} // namespace

BlockShape loadBlockShape(const std::string &name,
                          const std::string &shapeDirectory)
{
    return loadBlockShapeFile(
        name, ResourcePaths::join(shapeDirectory, name + ".shape"));
}

BlockShape loadBlockShapeFile(const std::string &name,
                              const std::string &path)
{
    if (!isValidName(name)) {
        throw std::runtime_error("Invalid block shape name '" + name +
                                 "'.");
    }
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open block shape file '" + path +
                                 "'.");
    }

    BlockShape shape;
    shape.name = name;
    std::string keyLine;
    std::size_t lineNumber = 0;
    while (std::getline(input, keyLine)) {
        ++lineNumber;
        const std::string key = trim(keyLine);
        if (key.empty()) {
            continue;
        }
        if (key != "Face") {
            fail(path, key, "is unknown at line " +
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
        shape.faces.push_back(parseFace(path, value));
    }

    if (shape.faces.empty()) {
        fail(path, "Face", "is missing");
    }
    return shape;
}
