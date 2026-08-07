#include "WorldSave.h"

#include "../../Util/ResourcePaths.h"

#include <cstddef>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <utility>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {
bool createDirectory(const std::string &path)
{
    if (path.empty() || ResourcePaths::exists(path)) {
        return true;
    }

    const auto parent = ResourcePaths::parentPath(path);
    if (parent != path && !createDirectory(parent)) {
        return false;
    }

#if defined(_WIN32)
    if (_mkdir(path.c_str()) == 0) {
#else
    if (mkdir(path.c_str(), 0755) == 0) {
#endif
        return true;
    }

    return errno == EEXIST;
}

void writeVec3(std::ostream &stream, const glm::vec3 &value)
{
    stream << value.x << ' ' << value.y << ' ' << value.z;
}

bool readVec3(std::istream &stream, glm::vec3 &value)
{
    return static_cast<bool>(stream >> value.x >> value.y >> value.z);
}
} // namespace

WorldSave::WorldSave()
    : WorldSave(ResourcePaths::bin("saves/default"))
{
}

WorldSave::WorldSave(std::string rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

bool WorldSave::load(WorldSaveData &data) const
{
    std::ifstream input(metadataPath());
    if (!input.is_open()) {
        return false;
    }

    WorldSaveData loaded;
    std::string key;
    while (input >> key) {
        if (key == "version") {
            input >> loaded.version;
        }
        else if (key == "world_id") {
            input >> loaded.worldId;
        }
        else if (key == "world_name") {
            input >> loaded.worldName;
        }
        else if (key == "seed") {
            input >> loaded.seed;
        }
        else if (key == "spawn") {
            if (!readVec3(input, loaded.spawnPoint)) {
                return false;
            }
        }
        else if (key == "world_time") {
            input >> loaded.worldTime;
        }
        else if (key == "generator") {
            input >> loaded.activeGenerator;
        }
        else if (key == "player_present") {
            int present = 0;
            input >> present;
            loaded.hasPlayerState = present != 0;
        }
        else if (key == "player_position") {
            if (!readVec3(input, loaded.playerState.position)) {
                return false;
            }
        }
        else if (key == "player_rotation") {
            if (!readVec3(input, loaded.playerState.rotation)) {
                return false;
            }
        }
        else if (key == "player_held") {
            input >> loaded.playerState.heldItem;
        }
        else if (key == "inventory_count") {
            std::size_t count = 0;
            input >> count;
            loaded.playerState.inventory.clear();
            loaded.playerState.inventory.reserve(count);
        }
        else if (key == "inventory_slot") {
            int materialId = 0;
            int amount = 0;
            input >> materialId >> amount;
            loaded.playerState.inventory.push_back(
                {static_cast<Material::ID>(materialId), amount});
        }
        else {
            std::string restOfLine;
            std::getline(input, restOfLine);
        }

        if (!input) {
            return false;
        }
    }

    data = loaded;
    return true;
}

bool WorldSave::save(const WorldSaveData &data) const
{
    if (!ensureRootDirectory()) {
        std::cerr << "Unable to create world save directory: " << m_rootDirectory
                  << '\n';
        return false;
    }

    std::ofstream output(metadataPath(), std::ios::trunc);
    if (!output.is_open()) {
        std::cerr << "Unable to open world save file: " << metadataPath()
                  << '\n';
        return false;
    }

    output << "version " << data.version << '\n';
    output << "world_id " << data.worldId << '\n';
    output << "world_name " << data.worldName << '\n';
    output << "seed " << data.seed << '\n';
    output << "spawn ";
    writeVec3(output, data.spawnPoint);
    output << '\n';
    output << "world_time " << data.worldTime << '\n';
    output << "generator " << data.activeGenerator << '\n';
    output << "player_present " << (data.hasPlayerState ? 1 : 0) << '\n';
    output << "player_position ";
    writeVec3(output, data.playerState.position);
    output << '\n';
    output << "player_rotation ";
    writeVec3(output, data.playerState.rotation);
    output << '\n';
    output << "player_held " << data.playerState.heldItem << '\n';
    output << "inventory_count " << data.playerState.inventory.size() << '\n';
    for (const auto &slot : data.playerState.inventory) {
        output << "inventory_slot " << static_cast<int>(slot.materialId) << ' '
               << slot.amount << '\n';
    }

    return static_cast<bool>(output);
}

std::string WorldSave::metadataPath() const
{
    return ResourcePaths::join(m_rootDirectory, "world.meta");
}

bool WorldSave::ensureRootDirectory() const
{
    return createDirectory(m_rootDirectory);
}
