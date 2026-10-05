// Actual saved-state preflight -> public World reopen -> public save -> second
// public World reopen. No Ogre Root/context, commands, ticks, mesh update or UI.
#include "Core/Camera.h"
#include "Player/Player.h"
#include "World/World.h"
#include "World/Storage/WorldSave.h"
#include "World/Storage/ChunkStorageData.h"
#include "World/Exploration/ExplorationMapStore.h"
#include "World/Block/BlockDatabase.h"
#include "World/Block/TerrainMaterialProfile.h"
#include "Util/ResourcePackResolver.h"
#include "Ogre/StartupResourcePreflight.h"
#include <FreeImage.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr std::size_t TextLimit = 4u * 1024u * 1024u;
struct Check { std::string name; bool passed; };
std::vector<Check> checks;
void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}
void check(const std::string& name, bool value)
{
    checks.push_back({name, value});
    std::cout << "[SHORE_REOPEN] " << (value ? "PASS " : "FAIL ") << name << '\n';
    require(value, name);
}
std::string readText(const fs::path& path)
{
    require(fs::is_regular_file(path) && !fs::is_symlink(path), "Missing/unsafe input: " + path.string());
    const auto bytes = fs::file_size(path);
    require(bytes <= TextLimit, "Text exceeds4MiB: " + path.string());
    std::ifstream stream(path, std::ios::binary);
    std::string result(static_cast<std::size_t>(bytes), '\0');
    if (bytes) stream.read(result.data(), static_cast<std::streamsize>(bytes));
    require(static_cast<bool>(stream), "Cannot read " + path.string());
    return result;
}
void writeText(const fs::path& path, const std::string& value)
{
    require(!fs::exists(path), "Refusing to overwrite " + path.string());
    std::ofstream out(path, std::ios::binary);
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
    require(static_cast<bool>(out), "Cannot write " + path.string());
}
std::string quote(const std::string& value)
{
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4)
            << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}

// Bounded strict JSON reader for retained capture metadata. Captured block facts
// supply expectations; production storage codecs and World independently read
// the saved truth. The save path must equal this capture directory's own /save.
struct Json {
    enum class Kind { Null, Object, Array, String, Number, Boolean } kind = Kind::Null;
    std::map<std::string, Json> object;
    std::vector<Json> array;
    std::string string;
    double number = 0;
    bool boolean = false;
    const Json& at(const std::string& key) const {
        require(kind == Kind::Object, "JSON object required for " + key);
        const auto found = object.find(key);
        require(found != object.end(), "Missing JSON field " + key);
        return found->second;
    }
    int integer() const {
        require(kind == Kind::Number && std::isfinite(number) && std::floor(number) == number &&
            number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max(), "JSON int required");
        return static_cast<int>(number);
    }
    const std::string& text() const {
        require(kind == Kind::String, "JSON string required"); return string;
    }
    bool flag() const { require(kind == Kind::Boolean, "JSON bool required"); return boolean; }
    const std::vector<Json>& values() const { require(kind == Kind::Array, "JSON array required"); return array; }
};
class JsonReader {
    const std::string& source;
    std::size_t offset = 0, nodes = 0;
    void whitespace() {
        while (offset < source.size() && (source[offset] == ' ' || source[offset] == '\n' ||
            source[offset] == '\r' || source[offset] == '\t')) ++offset;
    }
    char peek() { whitespace(); return offset < source.size() ? source[offset] : '\0'; }
    void take(char expected) { require(peek() == expected, "JSON token mismatch"); ++offset; }
    unsigned hex4() {
        require(offset + 4 <= source.size(), "Truncated JSON Unicode escape");
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = source[offset++]; unsigned digit = 0;
            if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
            else throw std::runtime_error("Invalid JSON Unicode escape");
            value = value * 16 + digit;
        }
        return value;
    }
    static void utf8(std::string& out, unsigned value) {
        if (value <= 0x7f) out.push_back(static_cast<char>(value));
        else if (value <= 0x7ff) {
            out.push_back(static_cast<char>(0xc0 | (value >> 6)));
            out.push_back(static_cast<char>(0x80 | (value & 63)));
        } else if (value <= 0xffff) {
            out.push_back(static_cast<char>(0xe0 | (value >> 12)));
            out.push_back(static_cast<char>(0x80 | ((value >> 6) & 63)));
            out.push_back(static_cast<char>(0x80 | (value & 63)));
        } else {
            out.push_back(static_cast<char>(0xf0 | (value >> 18)));
            out.push_back(static_cast<char>(0x80 | ((value >> 12) & 63)));
            out.push_back(static_cast<char>(0x80 | ((value >> 6) & 63)));
            out.push_back(static_cast<char>(0x80 | (value & 63)));
        }
    }
    std::string stringValue() {
        take('"'); std::string out;
        while (offset < source.size()) {
            const unsigned char c = static_cast<unsigned char>(source[offset++]);
            if (c == '"') return out;
            require(c >= 32, "Unescaped JSON control character");
            if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
            require(offset < source.size(), "Truncated JSON escape");
            const char escaped = source[offset++];
            switch (escaped) {
                case '"': case '\\': case '/': out.push_back(escaped); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned value = hex4();
                    if (value >= 0xd800 && value <= 0xdbff) {
                        require(offset + 2 <= source.size() && source[offset] == '\\' && source[offset + 1] == 'u', "Missing Unicode low surrogate");
                        offset += 2; const unsigned low = hex4();
                        require(low >= 0xdc00 && low <= 0xdfff, "Invalid Unicode low surrogate");
                        value = 0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
                    } else require(value < 0xdc00 || value > 0xdfff, "Unpaired Unicode low surrogate");
                    utf8(out, value); break;
                }
                default: throw std::runtime_error("Invalid JSON escape");
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }
    void literal(const std::string& value) {
        require(source.compare(offset, value.size(), value) == 0, "Invalid JSON literal"); offset += value.size();
    }
    Json value(unsigned depth) {
        require(depth <= 32 && ++nodes <= 65536, "JSON depth/node bound exceeded");
        Json result; const char token = peek();
        if (token == '{') {
            result.kind = Json::Kind::Object; take('{');
            if (peek() == '}') { take('}'); return result; }
            for (;;) {
                const auto key = stringValue(); take(':');
                require(result.object.emplace(key, value(depth + 1)).second, "Duplicate JSON field " + key);
                if (peek() == '}') { take('}'); return result; } take(',');
            }
        }
        if (token == '[') {
            result.kind = Json::Kind::Array; take('[');
            if (peek() == ']') { take(']'); return result; }
            for (;;) {
                result.array.push_back(value(depth + 1));
                if (peek() == ']') { take(']'); return result; } take(',');
            }
        }
        if (token == '"') { result.kind = Json::Kind::String; result.string = stringValue(); return result; }
        if (token == 'n') { literal("null"); return result; }
        if (token == 't' || token == 'f') {
            result.kind = Json::Kind::Boolean; result.boolean = token == 't';
            literal(result.boolean ? "true" : "false"); return result;
        }
        result.kind = Json::Kind::Number; const std::size_t begin = offset;
        if (offset < source.size() && source[offset] == '-') ++offset;
        require(offset < source.size() && source[offset] >= '0' && source[offset] <= '9', "JSON number required");
        if (source[offset] == '0') ++offset;
        else while (offset < source.size() && source[offset] >= '0' && source[offset] <= '9') ++offset;
        if (offset < source.size() && source[offset] == '.') {
            ++offset; const auto digits = offset;
            while (offset < source.size() && source[offset] >= '0' && source[offset] <= '9') ++offset;
            require(offset > digits, "Missing JSON fraction");
        }
        if (offset < source.size() && (source[offset] == 'e' || source[offset] == 'E')) {
            ++offset;
            if (offset < source.size() && (source[offset] == '+' || source[offset] == '-')) ++offset;
            const auto digits = offset;
            while (offset < source.size() && source[offset] >= '0' && source[offset] <= '9') ++offset;
            require(offset > digits, "Missing JSON exponent");
        }
        const auto number = source.substr(begin, offset - begin); char* end = nullptr;
        result.number = std::strtod(number.c_str(), &end);
        require(end == number.c_str() + number.size() && std::isfinite(result.number), "Invalid JSON number");
        return result;
    }
public:
    explicit JsonReader(const std::string& text) : source(text) {}
    Json parse() { auto result = value(0); whitespace(); require(offset == source.size(), "Trailing JSON data"); return result; }
};
Json readJson(const fs::path& path) { const auto text = readText(path); return JsonReader(text).parse(); }
glm::ivec3 targetOf(const Json& document) {
    const auto& values = document.values(); require(values.size() == 3, "Three target coordinates required");
    return {values[0].integer(), values[1].integer(), values[2].integer()};
}
ChunkBlock capturedTargetBlock(const Json& phase, glm::ivec3 target, int y) {
    std::optional<ChunkBlock> result;
    for (const auto& column : phase.at("column").values()) {
        if (column.at("x").integer() != target.x || column.at("z").integer() != target.z) continue;
        for (const auto& row : column.at("blocks").values()) {
            const auto& values = row.values(); require(values.size() == 3, "Three captured block fields required");
            if (values[0].integer() != y) continue;
            require(!result, "Duplicate captured target block");
            const int id = values[1].integer(), metadata = values[2].integer();
            require(id >= 0 && id < int(BlockId::NUM_TYPES) && metadata >= 0 && metadata <= 255, "Invalid captured block");
            result = ChunkBlock(static_cast<Block_t>(id), static_cast<BlockMetadata_t>(metadata));
        }
    }
    require(result.has_value(), "Captured target block missing"); return *result;
}
bool sameVector(const glm::vec3& a, const glm::vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool sameInventory(const PlayerSaveState& a, const PlayerSaveState& b) {
    return a.inventory == b.inventory && a.heldItem == b.heldItem;
}
void blockJson(std::ostream& out, glm::ivec3 p, ChunkBlock block) {
    out << "{\"position\":[" << p.x << ',' << p.y << ',' << p.z << "],\"id\":"
        << int(block.id) << ",\"metadata\":" << int(block.metadata) << '}';
}
struct Pass {
    int seed = 0, terrain = 0, sand = 0, heldMaterial = 0, heldAmount = 0;
    std::size_t loadedChunks = 0, exploredCells = 0;
    ChunkBlock top, lower;
    bool archivedBeforeObservation = false, observedKnown = false, saveCalled = false, saved = false;
    int archiveHeight = 0, archiveMaterial = 0, observedHeight = 0, observedMaterial = 0;
};
struct StoredInputPass {
    int pass = 0;
    std::vector<VectorXZ> persisted;
    std::vector<VectorXZ> absentProcedural;
};
std::vector<StoredInputPass> storedInputPasses;
struct FreeImageLifetime { FreeImageLifetime() { FreeImage_Initialise(FALSE); } ~FreeImageLifetime() { FreeImage_DeInitialise(); } };
void verifyStoredInputs(const fs::path& save, const WorldSaveData& expected, glm::ivec3 target,
                        ChunkBlock top, ChunkBlock lower, const fs::path& output, int pass) {
    const auto centre = World::getChunkXZ(World::toBlockCoord(expected.playerState.position.x), World::toBlockCoord(expected.playerState.position.z));
    const auto targetChunk = World::getChunkXZ(target.x, target.z);
    const auto local = World::getBlockXZ(target.x, target.z);
    ChunkStorageData storage((save / "chunks").string());
    storedInputPasses.push_back({pass, {}, {}});
    auto& inputs = storedInputPasses.back();
    bool targetFound = false;
    // Production save writes dirty chunks only (World.cpp3534;
    // ChunkManager.cpp590). Generated untouched chunks are clean
    // (Chunk.cpp539), so missing procedural neighbours are valid inputs for
    // ordinary constructor generation. The edited/restored target must exist.
    for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) {
        const int x = centre.x + dx, z = centre.z + dz;
        const fs::path path(storage.chunkPath(x, z)); StoredChunkData data; std::string error;
        if (!fs::exists(path) && !fs::is_symlink(path)) {
            if (x == targetChunk.x && z == targetChunk.z)
                check("pass" + std::to_string(pass) + "/modified-target-chunk-must-have-persisted-file", false);
            inputs.absentProcedural.push_back({x, z});
            continue;
        }
        check("pass" + std::to_string(pass) + "/stored-existing-resident-chunk-" + std::to_string(x) + "," + std::to_string(z),
            fs::is_regular_file(path) && !fs::is_symlink(path) && ChunkStorageData::loadChunkFile(path.string(), x, z, data, &error));
        inputs.persisted.push_back({x, z});
        if (x == targetChunk.x && z == targetChunk.z) {
            targetFound = true;
            const auto at = [&](int y) {
                const auto index = static_cast<std::size_t>(y) * CHUNK_AREA + static_cast<std::size_t>(local.z) * CHUNK_SIZE + static_cast<std::size_t>(local.x);
                require(index < data.blockIds.size() && index < data.metadata.size(), "Target exceeds saved chunk");
                return ChunkBlock(data.blockIds[index], data.metadata[index]);
            };
            check("pass" + std::to_string(pass) + "/stored-original-water64-metadata", at(target.y) == top);
            check("pass" + std::to_string(pass) + "/stored-original-water63-metadata", at(target.y - 1) == lower);
            if (pass == 1) check("preserved-target-chunk-before", fs::copy_file(path, output / "target-chunk-before.hmc"));
        }
    }
    check("pass" + std::to_string(pass) + "/modified-target-chunk-present-in-preload-envelope", targetFound);
    ExplorationAtlas persisted;
    std::string mapError;
    const auto loaded = ExplorationMapStore(save.string()).load(
        {expected.worldId, expected.seed, expected.terrainGenerationVersion}, persisted, &mapError);
    check("pass" + std::to_string(pass) + "/persistent-map-codec-identity", loaded == ExplorationMapStore::LoadStatus::Loaded);
    const auto archived = persisted.surfaceAt(target.x, target.z);
    check("pass" + std::to_string(pass) + "/persistent-canonical-cell-before-World", archived && archived->height == 64 && archived->material == BlockId::Water);
}
Pass runPass(const fs::path& save, const WorldSaveData& expected, glm::ivec3 target,
             ChunkBlock top, ChunkBlock lower, bool callSave, int pass) {
    Config config; config.renderDistance = 1; // No seed, position, time or inventory override.
    Camera camera(config); Player player;
    World world(camera, config, player, save.string(), false, 1);
    camera.hookEntity(player); camera.update();
    Pass result;
    result.seed = world.getChunkManager().getTerrainSeed();
    result.terrain = world.getChunkManager().getTerrainGenerationVersion();
    result.loadedChunks = world.getChunkManager().getChunks().size();
    const auto prefix = "pass" + std::to_string(pass) + '/';
    check(prefix + "actual-seed42-terrain30", result.seed == 42 && result.terrain == 30);
    check(prefix + "actual-nine-resident-chunks", result.loadedChunks == 9);
    const auto state = player.getSaveState();
    check(prefix + "actual-saved-position-rotation", sameVector(state.position, expected.playerState.position) && sameVector(state.rotation, expected.playerState.rotation));
    check(prefix + "actual-all-inventory-slots-and-held-index", sameInventory(state, expected.playerState));
    check(prefix + "actual-world-time-from-metadata", world.getWorldTime() == expected.worldTime);
    result.sand = player.getInventoryCount(Material::Sand);
    result.heldMaterial = int(player.getHeldItems().getMaterial().id);
    result.heldAmount = player.getHeldItems().getNumInStack();
    check(prefix + "actual-one-held-Sand", result.sand == 1 && result.heldMaterial == int(Material::Sand) && result.heldAmount == 1);
    result.top = world.getBlock(target.x, target.y, target.z);
    result.lower = world.getBlock(target.x, target.y - 1, target.z);
    check(prefix + "actual-original-Water64-metadata", result.top == top);
    check(prefix + "actual-original-Water63-metadata", result.lower == lower);
    const auto status = world.explorationMapStatus();
    check(prefix + "actual-map-loaded-without-reset", status.resetReason == ExplorationMapStatus::ResetReason::None && !status.quarantineFailed && !status.saveFailed);
    // Check persistence before observation: observeSurfaceMap could otherwise
    // refill an absent archive and conceal an unsaved/foreign map.
    const auto archived = world.exploredSurfaceAt(target.x, target.z);
    result.archivedBeforeObservation = archived.has_value();
    if (archived) { result.archiveHeight = archived->height; result.archiveMaterial = int(archived->material); }
    result.exploredCells = world.exploredCellCount();
    check(prefix + "actual-persistent-canonical-cell-before-observation", archived && archived->height == 64 && archived->material == BlockId::Water);
    const auto samples = world.observeSurfaceMap({{target.x, target.z}});
    if (samples.size() == 1) {
        result.observedKnown = samples[0].known; result.observedHeight = samples[0].height; result.observedMaterial = int(samples[0].material);
    }
    check(prefix + "actual-resident-surface-Water64", samples.size() == 1 && result.observedKnown && result.observedHeight == 64 && result.observedMaterial == int(BlockId::Water));
    check(prefix + "observation-does-not-add-archive-cell", world.exploredCellCount() == result.exploredCells);
    result.saveCalled = callSave;
    if (callSave) { result.saved = world.save(); check(prefix + "public-World-save-returned-true", result.saved); }
    return result; // World destruction uses its ordinary save path, still no ticks.
}
void writeReceipt(const fs::path& output, const fs::path& capture, const fs::path& save,
                  glm::ivec3 target, ChunkBlock top, ChunkBlock lower,
                  const std::string& worldId, const std::vector<Pass>& passes,
                  bool forcedEnvironmentRemoved, const std::string& error) {
    std::ostringstream out;
    out << "{\"schema\":\"hellomine3d-shore-restored-state-reopen-v1\",\"status\":" << quote(error.empty() ? "PASS" : "FAIL")
        << ",\"scope\":\"restored-state storage and production reopen only; normal input, edited-state persistence, UI reopen and GPU remain unclaimed\","
        << "\"normal_input\":false,\"capture\":" << quote(capture.string()) << ",\"existing_capture_save\":" << quote(save.string())
        << ",\"world_id\":" << quote(worldId) << ",\"target\":[" << target.x << ',' << target.y << ',' << target.z << ']' << ",\"original_top\":";
    blockJson(out, target, top); out << ",\"original_lower\":"; blockJson(out, target - glm::ivec3(0, 1, 0), lower);
    out << ",\"forced_seed_position_rotation_time_removed\":" << (forcedEnvironmentRemoved ? "true" : "false")
        << ",\"world_updates\":0,\"world_ticks\":0,\"commands\":0,\"block_mutations_requested\":0,"
        << "\"source_lifecycle\":\"only the explicit new completed shore capture save is opened; ordinary public save adds a backup and ordinary World destruction may update metadata. Before metadata/map/target-chunk retained in this fresh helper output. Old V06b apps/saves untouched\","
        << "\"storage_preflight_policy\":\"require persisted modified target and exact original Water64/63 metadata; parse every existing file in bounded initial9 envelope. Untouched generated clean chunks need not have files; ordinary constructor may regenerate them. Runtime resident9 remains required. r1 nine-file assumption failure retained\","
        << "\"stored_input_passes\":[";
    for (std::size_t i = 0; i < storedInputPasses.size(); ++i) {
        if (i) out << ','; const auto& p = storedInputPasses[i];
        out << "{\"pass\":" << p.pass << ",\"persisted_chunks\":[";
        for (std::size_t j = 0; j < p.persisted.size(); ++j) {
            if (j) out << ','; out << '[' << p.persisted[j].x << ',' << p.persisted[j].z << ']';
        }
        out << "],\"absent_untouched_procedural_neighbours\":[";
        for (std::size_t j = 0; j < p.absentProcedural.size(); ++j) {
            if (j) out << ','; out << '[' << p.absentProcedural[j].x << ',' << p.absentProcedural[j].z << ']';
        }
        out << "]}";
    }
    out << "],\"passes\":[";
    for (std::size_t i = 0; i < passes.size(); ++i) {
        if (i) out << ','; const auto& p = passes[i];
        out << "{\"pass\":" << i + 1 << ",\"seed\":" << p.seed << ",\"terrain\":" << p.terrain << ",\"loaded_chunks\":" << p.loadedChunks
            << ",\"inventory_sand\":" << p.sand << ",\"held_material\":" << p.heldMaterial << ",\"held_amount\":" << p.heldAmount
            << ",\"archived_before_observation\":" << (p.archivedBeforeObservation ? "true" : "false")
            << ",\"archive_height\":" << p.archiveHeight << ",\"archive_material\":" << p.archiveMaterial
            << ",\"explored_cells\":" << p.exploredCells << ",\"observed_known\":" << (p.observedKnown ? "true" : "false")
            << ",\"observed_height\":" << p.observedHeight << ",\"observed_material\":" << p.observedMaterial
            << ",\"public_save_called\":" << (p.saveCalled ? "true" : "false") << ",\"public_save_succeeded\":" << (p.saved ? "true" : "false") << '}';
    }
    out << "],\"checks\":[";
    for (std::size_t i = 0; i < checks.size(); ++i) {
        if (i) out << ',';
        out << "{\"name\":" << quote(checks[i].name) << ",\"passed\":" << (checks[i].passed ? "true" : "false") << '}';
    }
    out << "],\"error\":" << (error.empty() ? "null" : quote(error)) << "}\n";
    writeText(output / "reopen-validation.json", out.str());
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4) { std::cerr << "Usage: shore-world-reopen repository-root fresh-output-directory standard\n"; return 2; }
    fs::path output, capture, save; glm::ivec3 target(0); ChunkBlock top, lower;
    std::string worldId, error; std::vector<Pass> passes;
    bool outputCreated = false, forcedEnvironmentRemoved = false;
    try {
        const fs::path repository = fs::canonical(argv[1]); output = fs::absolute(argv[2]);
        require(std::string(argv[3]) == "standard", "Only standard CPU initialization is supported");
        require(!fs::exists(output) && !fs::is_symlink(output), "Helper output must be fresh");
        outputCreated = fs::create_directories(output);
        require(outputCreated, "Cannot create fresh helper output");
        const char* supplied = std::getenv("HELLOMINE3D_SHORE_REOPEN_CAPTURE");
        require(supplied && *supplied, "HELLOMINE3D_SHORE_REOPEN_CAPTURE is required");
        capture = fs::absolute(supplied); if (fs::is_directory(capture)) capture /= "capture.json";
        const auto outer = readJson(capture);
        check("completed-explicit-shore-capture", outer.at("result").text() == "CAPTURED" && !outer.at("normal_input").flag() &&
            outer.at("diagnostic_fixture").text() == "shore-edit-production-world-map");
        target = targetOf(outer.at("shore_edit_target"));
        check("canonical4-Water64-target", target.y == 64 && target.x % 4 == 0 && target.z % 4 == 0);
        const auto root = fs::canonical(capture.parent_path());
        const fs::path requestedSave(outer.at("environment").at("HELLOMINE3D_SAVE_DIR").text());
        require(!fs::is_symlink(requestedSave), "Capture save must not be a symlink");
        save = fs::canonical(requestedSave);
        check("capture-owned-save-path", save == fs::canonical(root / "save"));
        const auto index = readJson(root / "shore-edit/index.json");
        check("six-phase-capture-index", index.at("schema").text() == "hellomine3d-shore-edit-capture-v1" &&
            index.at("status").text() == "CAPTURED" && index.at("restored_save_succeeded").flag() && index.at("frames").values().size() == 6);
        std::array<Json, 6> phases;
        for (int i = 0; i < 6; ++i) {
            phases[static_cast<std::size_t>(i)] = readJson(root / "shore-edit" / ("phase-00" + std::to_string(i) + "-world-map.json"));
            const auto& phase = phases[static_cast<std::size_t>(i)];
            check("phase" + std::to_string(i) + "/schema-target", phase.at("schema").text() == "hellomine3d-shore-edit-world-map-v1" &&
                phase.at("phase").integer() == i && !phase.at("normal_input").flag() && targetOf(phase.at("target")) == target);
        }
        top = capturedTargetBlock(phases[0], target, 64); lower = capturedTargetBlock(phases[0], target, 63);
        check("baseline-original-Water64-Water63", top.id == Block_t(BlockId::Water) && lower.id == Block_t(BlockId::Water));
        check("phase5-restores-baseline-exact-id-metadata", capturedTargetBlock(phases[5], target, 64) == top && capturedTargetBlock(phases[5], target, 63) == lower);
        check("phase5-one-held-Sand", phases[5].at("inventory_sand").integer() == 1 &&
            phases[5].at("held_material").integer() == int(Material::Sand) && phases[5].at("held_amount").integer() == 1);
        const auto metadata = readText(save / "world.meta");
        check("capture-record-matches-existing-metadata-before-reopen", outer.at("world_metadata").text() == metadata);
        writeText(output / "metadata-before.txt", metadata);
        const fs::path mapPath(save / "exploration.hmap");
        check("preserve-existing-map-before", fs::is_regular_file(mapPath) && !fs::is_symlink(mapPath) &&
            fs::file_size(mapPath) <= ExplorationMapStore::MaxFileBytes && fs::copy_file(mapPath, output / "exploration-before.hmap"));
        WorldSaveData expected; std::string metadataError;
        check("existing-metadata-production-codec", WorldSave::loadFromPath((save / "world.meta").string(), expected, &metadataError));
        worldId = expected.worldId;
        check("existing-metadata-save12-seed42-terrain30-player", expected.version == WorldSaveFormatVersion &&
            expected.seed == 42 && expected.terrainGenerationVersion == 30 && expected.hasPlayerState);
        const auto held = expected.playerState.heldItem;
        int sand = 0; for (const auto& slot : expected.playerState.inventory) if (slot.materialId == Material::Sand) sand += slot.amount;
        check("existing-metadata-one-held-Sand", sand == 1 && held >= 0 && held < int(expected.playerState.inventory.size()) &&
            expected.playerState.inventory[static_cast<std::size_t>(held)].materialId == Material::Sand &&
            expected.playerState.inventory[static_cast<std::size_t>(held)].amount == 1);
        // No inherited diagnostic override may replace the saved truth.
        for (const char* name : {"HELLOMINE3D_SEED", "HELLOMINE3D_PLAYER_POSITION", "HELLOMINE3D_PLAYER_ROTATION", "HELLOMINE3D_WORLD_TIME"}) unsetenv(name);
        forcedEnvironmentRemoved = true;
        setenv("HELLOMINE3D_ROOT", repository.c_str(), 1);
        FreeImageLifetime images;
        const auto resources = loadStartupResourceManifest(repository.string());
        std::vector<ResourcePackRequirement> requirements;
        for (const auto& resource : resources) requirements.push_back({resource.category, resource.relativePath});
        runtimeResourcePackResolver().freeze(repository.string(), requirements, {});
        validateStartupResources(repository.string(), resources);
        runtimeTerrainMaterialProfile().freezeFromResourceView(runtimeResourcePackResolver());
        runtimeTerrainMaterialProfile().freezeRenderingMode(true, true); BlockDatabase::get();
        verifyStoredInputs(save, expected, target, top, lower, output, 1);
        passes.push_back(runPass(save, expected, target, top, lower, true, 1));
        writeText(output / "metadata-after-public-save.txt", readText(save / "world.meta"));
        WorldSaveData second; metadataError.clear();
        check("second-existing-metadata-production-codec", WorldSave::loadFromPath((save / "world.meta").string(), second, &metadataError));
        check("second-world-identity-and-inventory-unchanged", second.worldId == expected.worldId && second.seed == expected.seed &&
            second.terrainGenerationVersion == expected.terrainGenerationVersion && sameInventory(second.playerState, expected.playerState));
        verifyStoredInputs(save, second, target, top, lower, output, 2);
        passes.push_back(runPass(save, second, target, top, lower, false, 2));
        writeText(output / "metadata-after-second-reopen.txt", readText(save / "world.meta"));
    } catch (const std::exception& exception) { error = exception.what(); std::cerr << "[SHORE_REOPEN] FAIL " << error << '\n'; }
    if (outputCreated) {
        try { writeReceipt(output, capture, save, target, top, lower, worldId, passes, forcedEnvironmentRemoved, error); }
        catch (const std::exception& exception) { std::cerr << "Cannot preserve reopen receipt: " << exception.what() << '\n'; return 2; }
    }
    return error.empty() ? 0 : 1;
}
