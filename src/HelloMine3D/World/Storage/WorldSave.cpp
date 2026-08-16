#include "WorldSave.h"

#include "WorldCatalogue.h"

#include "../../Util/ResourcePaths.h"

#include <cstddef>
#include <cerrno>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {
namespace fs = std::filesystem;

constexpr std::size_t MaxStoredInventorySlots = 4096;
constexpr std::size_t MaxStoredActors = 65536;

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
        kind > static_cast<int>(ActorSaveKind::Item) ||
        (alive != 0 && alive != 1)) {
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

bool finiteVec3(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool loadWorldSaveFile(const std::string &path, WorldSaveData &data,
                       std::string *errorMessage)
{
    const auto fail = [&](const std::string &message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    };

    std::error_code fileError;
    const std::uintmax_t fileBytes = fs::file_size(path, fileError);
    if (fileError || fileBytes == 0 ||
        fileBytes > WorldCatalogue::MaxMetadataBytes) {
        return fail("metadata file size is invalid");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return fail("metadata file cannot be opened");
    }

    WorldSaveData loaded;
    std::unordered_set<std::string> singletonFields;
    bool inventoryCountSeen = false;
    std::size_t expectedInventoryCount = 0;
    bool actorCountSeen = false;
    std::size_t expectedActorCount = 0;
    const auto claimSingleton = [&](const std::string &key) {
        return singletonFields.emplace(key).second;
    };

    std::string key;
    while (input >> key) {
        if (key == "version") {
            if (!claimSingleton(key) || !(input >> loaded.version)) {
                return fail("invalid or duplicate version");
            }
        }
        else if (key == "world_id") {
            if (!claimSingleton(key) || !(input >> loaded.worldId)) {
                return fail("invalid or duplicate world_id");
            }
        }
        else if (key == "world_name") {
            if (!claimSingleton(key) ||
                !readWorldName(input, loaded.worldName)) {
                return fail("invalid or duplicate world_name");
            }
        }
        else if (key == "seed") {
            if (!claimSingleton(key) || !(input >> loaded.seed)) {
                return fail("invalid or duplicate seed");
            }
        }
        else if (key == "created_utc") {
            if (!claimSingleton(key) || !(input >> loaded.createdUtc)) {
                return fail("invalid or duplicate created_utc");
            }
        }
        else if (key == "last_played_utc") {
            if (!claimSingleton(key) || !(input >> loaded.lastPlayedUtc)) {
                return fail("invalid or duplicate last_played_utc");
            }
        }
        else if (key == "last_build") {
            if (!claimSingleton(key) ||
                !(input >> loaded.lastBuildIdentity)) {
                return fail("invalid or duplicate last_build");
            }
        }
        else if (key == "spawn") {
            if (!claimSingleton(key) || !readVec3(input, loaded.spawnPoint)) {
                return fail("invalid or duplicate spawn");
            }
        }
        else if (key == "world_time") {
            if (!claimSingleton(key) || !(input >> loaded.worldTime)) {
                return fail("invalid or duplicate world_time");
            }
        }
        else if (key == "generator") {
            if (!claimSingleton(key) ||
                !(input >> loaded.activeGenerator)) {
                return fail("invalid or duplicate generator");
            }
        }
        else if (key == "player_present") {
            int present = 0;
            if (!claimSingleton(key) || !(input >> present) ||
                (present != 0 && present != 1)) {
                return fail("invalid or duplicate player_present");
            }
            loaded.hasPlayerState = present != 0;
        }
        else if (key == "player_position") {
            if (!claimSingleton(key) ||
                !readVec3(input, loaded.playerState.position)) {
                return fail("invalid or duplicate player_position");
            }
        }
        else if (key == "player_rotation") {
            if (!claimSingleton(key) ||
                !readVec3(input, loaded.playerState.rotation)) {
                return fail("invalid or duplicate player_rotation");
            }
        }
        else if (key == "player_held") {
            if (!claimSingleton(key) ||
                !(input >> loaded.playerState.heldItem)) {
                return fail("invalid or duplicate player_held");
            }
        }
        else if (key == "inventory_count") {
            if (!claimSingleton(key) || !(input >> expectedInventoryCount) ||
                expectedInventoryCount > MaxStoredInventorySlots) {
                return fail("invalid or duplicate inventory_count");
            }
            loaded.playerState.inventory.clear();
            loaded.playerState.inventory.reserve(expectedInventoryCount);
            inventoryCountSeen = true;
        }
        else if (key == "inventory_slot") {
            int materialId = 0;
            int amount = 0;
            if (!(input >> materialId >> amount) ||
                loaded.playerState.inventory.size() >=
                    MaxStoredInventorySlots) {
                return fail("invalid inventory_slot");
            }
            loaded.playerState.inventory.push_back(
                {static_cast<Material::ID>(materialId), amount});
        }
        else if (key == "actor_count") {
            if (!claimSingleton(key) || !(input >> expectedActorCount) ||
                expectedActorCount > MaxStoredActors) {
                return fail("invalid or duplicate actor_count");
            }
            loaded.actors.clear();
            loaded.actors.reserve(expectedActorCount);
            actorCountSeen = true;
        }
        else if (key == "actor") {
            if (loaded.actors.size() >= MaxStoredActors) {
                return fail("actor count exceeds limit");
            }
            ActorSaveState actor;
            if (!readActor(input, actor)) {
                return fail("invalid actor record");
            }
            loaded.actors.push_back(std::move(actor));
        }
        else {
            std::string restOfLine;
            std::getline(input, restOfLine);
        }
    }

    if (!input.eof()) {
        return fail("metadata parse did not reach end of file");
    }
    if (singletonFields.count("version") == 0 || loaded.version < 1 ||
        loaded.version > WorldSaveFormatVersion) {
        return fail("unsupported or missing version");
    }
    if (inventoryCountSeen &&
        loaded.playerState.inventory.size() != expectedInventoryCount) {
        return fail("inventory count does not match records");
    }
    if (actorCountSeen && loaded.actors.size() != expectedActorCount) {
        return fail("actor count does not match records");
    }

    if (loaded.version == WorldSaveFormatVersion) {
        static const char *const requiredFields[] = {
            "world_id",       "world_name",       "seed",
            "created_utc",    "last_played_utc",  "last_build",
            "spawn",          "world_time",       "generator",
            "player_present", "player_position",  "player_rotation",
            "player_held",    "inventory_count",  "actor_count"};
        for (const char *required : requiredFields) {
            if (singletonFields.count(required) == 0) {
                return fail(std::string("missing required field ") +
                            required);
            }
        }
        if (!WorldCatalogue::isValidWorldId(loaded.worldId) ||
            !WorldCatalogue::isValidDisplayName(loaded.worldName) ||
            !WorldCatalogue::isValidBuildIdentity(
                loaded.lastBuildIdentity) ||
            !WorldCatalogue::isValidTimestamps(loaded.createdUtc,
                                               loaded.lastPlayedUtc)) {
            return fail("version-3 identity is invalid");
        }
    }

    if (!finiteVec3(loaded.spawnPoint) ||
        !finiteVec3(loaded.playerState.position) ||
        !finiteVec3(loaded.playerState.rotation) ||
        !std::isfinite(loaded.worldTime) || loaded.activeGenerator.empty()) {
        return fail("world state contains a non-finite or empty field");
    }
    if (loaded.playerState.heldItem < 0 ||
        (!loaded.playerState.inventory.empty() &&
         loaded.playerState.heldItem >=
             static_cast<int>(loaded.playerState.inventory.size()))) {
        return fail("selected inventory slot is out of range");
    }
    for (const PlayerInventorySlot &slot : loaded.playerState.inventory) {
        const int materialId = static_cast<int>(slot.materialId);
        if (materialId < static_cast<int>(Material::ID::Nothing) ||
            materialId >= static_cast<int>(Material::ID::Count) ||
            slot.amount < 0 ||
            (slot.materialId == Material::ID::Nothing && slot.amount != 0) ||
            (slot.materialId != Material::ID::Nothing && slot.amount == 0)) {
            return fail("inventory slot contains invalid material state");
        }
    }
    for (const ActorSaveState &actor : loaded.actors) {
        if (actor.type.empty() || !finiteVec3(actor.position) ||
            !finiteVec3(actor.rotation) || !finiteVec3(actor.velocity) ||
            !std::isfinite(actor.health) ||
            !std::isfinite(actor.pickupDelay) ||
            !std::isfinite(actor.wanderTime) ||
            !std::isfinite(actor.wanderSpeed)) {
            return fail("actor state contains a non-finite or empty field");
        }
    }

    data = std::move(loaded);
    return true;
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
    return loadWorldSaveFile(metadataPath(), data, nullptr);
}

bool WorldSave::save(const WorldSaveData &data) const
{
    return save(data, {}, nullptr);
}

bool WorldSave::save(const WorldSaveData &data,
                     const StorageTransactionOptions &options,
                     StorageTransactionMetrics *metrics) const
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

    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<float>::max_digits10);

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

    if (!output) {
        std::cerr << "Unable to serialize world save file: "
                  << metadataPath() << '\n';
        return false;
    }

    const std::string serialized = output.str();
    const std::vector<char> payload(serialized.begin(), serialized.end());
    const auto validator = [](const std::string &candidate,
                              std::string &validationError) {
        WorldSaveData validated;
        return loadWorldSaveFile(candidate, validated, &validationError);
    };
    StorageTransactionMetrics localMetrics;
    StorageTransactionMetrics *resultMetrics =
        metrics != nullptr ? metrics : &localMetrics;
    if (!StorageTransaction::publish(metadataPath(), payload, validator,
                                     options, resultMetrics)) {
        std::cerr << "Unable to publish world save file: " << metadataPath();
        if (!resultMetrics->error.empty()) {
            std::cerr << " (" << resultMetrics->error << ')';
        }
        std::cerr << '\n';
        return false;
    }
    return true;
}

std::string WorldSave::metadataPath() const
{
    return ResourcePaths::join(m_rootDirectory, "world.meta");
}

bool WorldSave::ensureRootDirectory() const
{
    return createDirectory(m_rootDirectory);
}
