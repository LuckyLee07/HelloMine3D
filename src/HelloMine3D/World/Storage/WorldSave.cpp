#include "WorldSave.h"

#include "../../Gameplay/AlphaJourney.h"

#include "WorldCatalogue.h"

#include "../../Item/ToolRegistry.h"
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
constexpr std::size_t MaxStoredObjectives = 256;
constexpr int CurrentInventoryFormat = 2;
constexpr float MaxStoredPlayerHealth = 20.f;
constexpr int MaxStoredFoodCooldownTicks = 1200;
constexpr int MaxStoredAttackCooldownTicks = 1200;
static_assert(WorldSaveFormatVersion ==
                  WorldCatalogue::CurrentSaveFormatVersion,
              "World save and catalogue versions must match.");

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

bool validObjectiveState(const WorldSaveData &data)
{
    if (data.objectiveState.definitionVersion !=
            ObjectiveSaveState::CurrentDefinitionVersion ||
        data.objectiveState.completedIds.size() > MaxStoredObjectives ||
        data.objectiveState.progress.size() > MaxStoredObjectives ||
        ObjectiveState::legacyFlagsFromCompleted(
            data.objectiveState.completedIds) != data.alphaJourneyFlags) {
        return false;
    }

    std::unordered_set<std::string> completed;
    for (const std::string &id : data.objectiveState.completedIds) {
        if (!ObjectiveState::isCanonicalId(id) ||
            !completed.emplace(id).second) {
            return false;
        }
    }
    std::unordered_set<std::string> progress;
    for (const ObjectiveProgressState &state :
         data.objectiveState.progress) {
        if (!ObjectiveState::isCanonicalId(state.id) || state.value < 0 ||
            state.value > 1000000 || completed.count(state.id) != 0 ||
            !progress.emplace(state.id).second) {
            return false;
        }
    }
    return true;
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
    loaded.terrainGenerationVersion = LegacyTerrainGenerationVersion;
    loaded.objectiveState.definitionVersion = 0;
    std::unordered_set<std::string> singletonFields;
    bool inventoryCountSeen = false;
    bool inventoryFormatSeen = false;
    int inventoryFormat = 1;
    std::size_t expectedInventoryCount = 0;
    bool actorCountSeen = false;
    std::size_t expectedActorCount = 0;
    bool alphaJourneySeen = false;
    bool playerHealthSeen = false;
    bool playerFoodCooldownSeen = false;
    bool playerAttackCooldownSeen = false;
    bool terrainGenerationVersionSeen = false;
    bool objectiveDefinitionSeen = false;
    bool objectiveCompletedCountSeen = false;
    bool objectiveProgressCountSeen = false;
    std::size_t expectedObjectiveCompletedCount = 0;
    std::size_t expectedObjectiveProgressCount = 0;
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
        else if (key == "terrain_generation_version") {
            if (!claimSingleton(key) ||
                !(input >> loaded.terrainGenerationVersion)) {
                return fail(
                    "invalid or duplicate terrain_generation_version");
            }
            terrainGenerationVersionSeen = true;
        }
        else if (key == "alpha_journey_flags") {
            if (!claimSingleton(key) ||
                !(input >> loaded.alphaJourneyFlags) ||
                !AlphaJourney::validFlags(loaded.alphaJourneyFlags)) {
                return fail("invalid or duplicate alpha_journey_flags");
            }
            alphaJourneySeen = true;
        }
        else if (key == "objective_definition_version") {
            if (!claimSingleton(key) ||
                !(input >> loaded.objectiveState.definitionVersion)) {
                return fail(
                    "invalid or duplicate objective_definition_version");
            }
            objectiveDefinitionSeen = true;
        }
        else if (key == "objective_completed_count") {
            if (!claimSingleton(key) ||
                !(input >> expectedObjectiveCompletedCount) ||
                expectedObjectiveCompletedCount > MaxStoredObjectives) {
                return fail(
                    "invalid or duplicate objective_completed_count");
            }
            loaded.objectiveState.completedIds.clear();
            loaded.objectiveState.completedIds.reserve(
                expectedObjectiveCompletedCount);
            objectiveCompletedCountSeen = true;
        }
        else if (key == "objective_completed") {
            std::string id;
            if (!objectiveCompletedCountSeen || !(input >> id) ||
                loaded.objectiveState.completedIds.size() >=
                    expectedObjectiveCompletedCount) {
                return fail("invalid objective_completed record");
            }
            loaded.objectiveState.completedIds.push_back(std::move(id));
        }
        else if (key == "objective_progress_count") {
            if (!claimSingleton(key) ||
                !(input >> expectedObjectiveProgressCount) ||
                expectedObjectiveProgressCount > MaxStoredObjectives) {
                return fail(
                    "invalid or duplicate objective_progress_count");
            }
            loaded.objectiveState.progress.clear();
            loaded.objectiveState.progress.reserve(
                expectedObjectiveProgressCount);
            objectiveProgressCountSeen = true;
        }
        else if (key == "objective_progress") {
            ObjectiveProgressState state;
            if (!objectiveProgressCountSeen ||
                !(input >> state.id >> state.value) ||
                loaded.objectiveState.progress.size() >=
                    expectedObjectiveProgressCount) {
                return fail("invalid objective_progress record");
            }
            loaded.objectiveState.progress.push_back(std::move(state));
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
        else if (key == "player_health") {
            if (!claimSingleton(key) ||
                !(input >> loaded.playerState.health)) {
                return fail("invalid or duplicate player_health");
            }
            playerHealthSeen = true;
        }
        else if (key == "player_food_cooldown") {
            if (!claimSingleton(key) ||
                !(input >> loaded.playerState.foodCooldownTicks)) {
                return fail("invalid or duplicate player_food_cooldown");
            }
            playerFoodCooldownSeen = true;
        }
        else if (key == "player_attack_cooldown") {
            if (!claimSingleton(key) ||
                !(input >> loaded.playerState.attackCooldownTicks)) {
                return fail("invalid or duplicate player_attack_cooldown");
            }
            playerAttackCooldownSeen = true;
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
        else if (key == "inventory_format") {
            if (inventoryFormatSeen ||
                !(input >> inventoryFormat) ||
                inventoryFormat < 1 ||
                inventoryFormat > CurrentInventoryFormat ||
                !loaded.playerState.inventory.empty()) {
                return fail("invalid, duplicate or misplaced inventory_format");
            }
            inventoryFormatSeen = true;
        }
        else if (key == "inventory_slot") {
            int materialId = 0;
            int amount = 0;
            int durability = 0;
            if (!(input >> materialId >> amount) ||
                (inventoryFormat >= 2 && !(input >> durability)) ||
                loaded.playerState.inventory.size() >=
                    MaxStoredInventorySlots) {
                return fail("invalid inventory_slot");
            }
            loaded.playerState.inventory.push_back(
                {static_cast<Material::ID>(materialId), amount,
                 durability});
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
    if (objectiveCompletedCountSeen &&
        loaded.objectiveState.completedIds.size() !=
            expectedObjectiveCompletedCount) {
        return fail("objective completed count does not match records");
    }
    if (objectiveProgressCountSeen &&
        loaded.objectiveState.progress.size() !=
            expectedObjectiveProgressCount) {
        return fail("objective progress count does not match records");
    }

    if (loaded.version >= 3) {
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
    if ((loaded.version >= 4 && !alphaJourneySeen) ||
        (loaded.version < 4 && alphaJourneySeen)) {
        return fail("alpha journey field does not match save version");
    }
    const bool objectiveFieldsPresent = objectiveDefinitionSeen ||
        objectiveCompletedCountSeen || objectiveProgressCountSeen ||
        !loaded.objectiveState.completedIds.empty() ||
        !loaded.objectiveState.progress.empty();
    if ((loaded.version >= 5 &&
         (!objectiveDefinitionSeen || !objectiveCompletedCountSeen ||
          !objectiveProgressCountSeen || !validObjectiveState(loaded))) ||
        (loaded.version < 5 && objectiveFieldsPresent)) {
        return fail("objective state does not match save version");
    }
    const bool playerRecoveryFieldsPresent =
        playerHealthSeen || playerFoodCooldownSeen;
    if ((loaded.version >= 6 &&
         (!playerHealthSeen || !playerFoodCooldownSeen)) ||
        (loaded.version < 6 && playerRecoveryFieldsPresent)) {
        return fail("player recovery state does not match save version");
    }
    if ((loaded.version >= 7 && !playerAttackCooldownSeen) ||
        (loaded.version < 7 && playerAttackCooldownSeen)) {
        return fail("player attack state does not match save version");
    }
    if ((loaded.version >= 8 && !terrainGenerationVersionSeen) ||
        (loaded.version < 8 && terrainGenerationVersionSeen)) {
        return fail("terrain generation state does not match save version");
    }

    if (!finiteVec3(loaded.spawnPoint) ||
        !finiteVec3(loaded.playerState.position) ||
        !finiteVec3(loaded.playerState.rotation) ||
        !std::isfinite(loaded.worldTime) ||
        !std::isfinite(loaded.playerState.health) ||
        loaded.playerState.health < 0.f ||
        loaded.playerState.health > MaxStoredPlayerHealth ||
        loaded.playerState.foodCooldownTicks < 0 ||
        loaded.playerState.foodCooldownTicks > MaxStoredFoodCooldownTicks ||
        loaded.playerState.attackCooldownTicks < 0 ||
        loaded.playerState.attackCooldownTicks >
            MaxStoredAttackCooldownTicks ||
        loaded.terrainGenerationVersion <
            LegacyTerrainGenerationVersion ||
        loaded.terrainGenerationVersion >
            CurrentTerrainGenerationVersion ||
        loaded.activeGenerator.empty()) {
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
        const Material &material = Material::toMaterial(slot.materialId);
        const ToolDefinition *tool =
            material.isTool
                ? runtimeToolRegistry().find(slot.materialId)
                : nullptr;
        if (materialId < static_cast<int>(Material::ID::Nothing) ||
            materialId >= static_cast<int>(Material::ID::Count) ||
            slot.amount < 0 ||
            (slot.materialId == Material::ID::Nothing && slot.amount != 0) ||
            (slot.materialId != Material::ID::Nothing && slot.amount == 0) ||
            slot.amount > material.maxStackSize ||
            slot.durability < 0 ||
            (material.isTool &&
             (tool == nullptr ||
              inventoryFormat < CurrentInventoryFormat ||
              slot.amount != 1 || slot.durability == 0 ||
              slot.durability > tool->maxDurability)) ||
            (!material.isTool && slot.durability != 0)) {
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
    return loadFromPath(metadataPath(), data, nullptr);
}

bool WorldSave::loadFromPath(const std::string &path, WorldSaveData &data,
                             std::string *errorMessage)
{
    return loadWorldSaveFile(path, data, errorMessage);
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
        !AlphaJourney::validFlags(data.alphaJourneyFlags) ||
        !validObjectiveState(data) ||
        !WorldCatalogue::isValidWorldId(data.worldId) ||
        !WorldCatalogue::isValidDisplayName(data.worldName) ||
        !WorldCatalogue::isValidBuildIdentity(data.lastBuildIdentity) ||
        data.terrainGenerationVersion < LegacyTerrainGenerationVersion ||
        data.terrainGenerationVersion > CurrentTerrainGenerationVersion ||
        !WorldCatalogue::isValidTimestamps(data.createdUtc,
                                           data.lastPlayedUtc)) {
        std::cerr << "Refusing to save invalid current world state: "
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
    output << "terrain_generation_version "
           << data.terrainGenerationVersion << '\n';
    output << "alpha_journey_flags " << data.alphaJourneyFlags << '\n';
    output << "objective_definition_version "
           << data.objectiveState.definitionVersion << '\n';
    output << "objective_completed_count "
           << data.objectiveState.completedIds.size() << '\n';
    for (const std::string &id : data.objectiveState.completedIds) {
        output << "objective_completed " << id << '\n';
    }
    output << "objective_progress_count "
           << data.objectiveState.progress.size() << '\n';
    for (const ObjectiveProgressState &state :
         data.objectiveState.progress) {
        output << "objective_progress " << state.id << ' ' << state.value
               << '\n';
    }
    output << "player_present " << (data.hasPlayerState ? 1 : 0) << '\n';
    output << "player_position ";
    writeVec3(output, data.playerState.position);
    output << '\n';
    output << "player_rotation ";
    writeVec3(output, data.playerState.rotation);
    output << '\n';
    output << "player_held " << data.playerState.heldItem << '\n';
    output << "player_health " << data.playerState.health << '\n';
    output << "player_food_cooldown "
           << data.playerState.foodCooldownTicks << '\n';
    output << "player_attack_cooldown "
           << data.playerState.attackCooldownTicks << '\n';
    output << "inventory_format " << CurrentInventoryFormat << '\n';
    output << "inventory_count " << data.playerState.inventory.size() << '\n';
    for (const auto &slot : data.playerState.inventory) {
        output << "inventory_slot " << static_cast<int>(slot.materialId) << ' '
               << slot.amount << ' ' << slot.durability << '\n';
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
        return WorldSave::loadFromPath(candidate, validated,
                                       &validationError);
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
