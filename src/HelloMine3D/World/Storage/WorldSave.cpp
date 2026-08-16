#include "WorldSave.h"

#include "WorldCatalogue.h"

#include "../../Util/ResourcePaths.h"

#include <cstddef>
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
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

bool readWorldName(std::istream &stream, std::string &value)
{
    std::string line;
    std::getline(stream, line);
    const std::size_t begin = line.find_first_not_of(" \t\r");
    if (begin == std::string::npos) {
        return false;
    }
    line.erase(0, begin);
    const std::size_t end = line.find_last_not_of(" \t\r");
    line.erase(end + 1);

    std::istringstream input(line);
    if (line.front() == '"') {
        if (!(input >> std::quoted(value))) {
            return false;
        }
    }
    else {
        if (!(input >> value)) {
            return false;
        }
    }
    input >> std::ws;
    return input.peek() == std::char_traits<char>::eof();
}

bool readActor(std::istream &stream, ActorSaveState &state)
{
    int kind = 0;
    int alive = 0;
    if (!(stream >> kind >> state.id >> state.type) ||
        !readVec3(stream, state.position) ||
        !readVec3(stream, state.rotation) ||
        !readVec3(stream, state.velocity) ||
        !(stream >> alive >> state.health >> state.materialId >>
          state.amount >> state.pickupDelay >> state.wanderTime >>
          state.wanderSpeed >> state.dropMaterialId >> state.dropAmount)) {
        return false;
    }
    if (kind < static_cast<int>(ActorSaveKind::Generic) ||
        kind > static_cast<int>(ActorSaveKind::Item)) {
        return false;
    }
    state.kind = static_cast<ActorSaveKind>(kind);
    state.alive = alive != 0;
    return true;
}

void writeActor(std::ostream &stream, const ActorSaveState &state)
{
    stream << "actor " << static_cast<int>(state.kind) << ' '
           << state.id << ' ' << state.type << ' ';
    writeVec3(stream, state.position);
    stream << ' ';
    writeVec3(stream, state.rotation);
    stream << ' ';
    writeVec3(stream, state.velocity);
    stream << ' ' << (state.alive ? 1 : 0) << ' ' << state.health << ' '
           << state.materialId << ' ' << state.amount << ' '
           << state.pickupDelay << ' ' << state.wanderTime << ' '
           << state.wanderSpeed << ' ' << state.dropMaterialId << ' '
           << state.dropAmount << '\n';
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
    bool actorCountSeen = false;
    std::size_t expectedActorCount = 0;
    std::string key;
    while (input >> key) {
        if (key == "version") {
            input >> loaded.version;
        }
        else if (key == "world_id") {
            input >> loaded.worldId;
        }
        else if (key == "world_name") {
            if (!readWorldName(input, loaded.worldName)) {
                return false;
            }
        }
        else if (key == "seed") {
            input >> loaded.seed;
        }
        else if (key == "created_utc") {
            input >> loaded.createdUtc;
        }
        else if (key == "last_played_utc") {
            input >> loaded.lastPlayedUtc;
        }
        else if (key == "last_build") {
            input >> loaded.lastBuildIdentity;
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
        else if (key == "actor_count") {
            input >> expectedActorCount;
            loaded.actors.clear();
            loaded.actors.reserve(expectedActorCount);
            actorCountSeen = true;
        }
        else if (key == "actor") {
            ActorSaveState actor;
            if (!readActor(input, actor)) {
                return false;
            }
            loaded.actors.push_back(std::move(actor));
        }
        else {
            std::string restOfLine;
            std::getline(input, restOfLine);
        }

        if (!input) {
            return false;
        }
    }

    if (actorCountSeen && loaded.actors.size() != expectedActorCount) {
        return false;
    }

    data = loaded;
    return true;
}

bool WorldSave::save(const WorldSaveData &data) const
{
    if (data.version != WorldSaveFormatVersion ||
        !WorldCatalogue::isValidWorldId(data.worldId) ||
        !WorldCatalogue::isValidDisplayName(data.worldName) ||
        !WorldCatalogue::isValidBuildIdentity(data.lastBuildIdentity) ||
        !WorldCatalogue::isValidTimestamps(data.createdUtc,
                                           data.lastPlayedUtc)) {
        std::cerr << "Refusing to save invalid version-3 world identity: "
                  << metadataPath() << '\n';
        return false;
    }

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
    output << "world_name " << std::quoted(data.worldName) << '\n';
    output << "seed " << data.seed << '\n';
    output << "created_utc " << data.createdUtc << '\n';
    output << "last_played_utc " << data.lastPlayedUtc << '\n';
    output << "last_build " << data.lastBuildIdentity << '\n';
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
    output << "actor_count " << data.actors.size() << '\n';
    for (const ActorSaveState &actor : data.actors) {
        writeActor(output, actor);
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
