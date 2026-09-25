#include "../../src/HelloMine3D/World/Exploration/WorldPreviewStore.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

int checks = 0;

void require(bool condition, const char* label)
{
    ++checks;
    if (!condition) {
        throw std::runtime_error(label);
    }
}

std::vector<char> readBytes(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

void writeBytes(const fs::path& path, const std::vector<char>& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(output.good(), "test fixture write failed");
}

struct TemporaryWorld {
    fs::path path;

    TemporaryWorld()
    {
        const auto ticks = std::chrono::steady_clock::now()
                               .time_since_epoch().count();
        path = fs::temp_directory_path() /
               ("hm3d-world-preview-" + std::to_string(ticks));
        fs::create_directories(path);
    }

    ~TemporaryWorld()
    {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
}

int main()
{
    try {
        using PreviewStatus = WorldPreviewStore::LoadStatus;
        using RevisionStatus = ExplorationMapStore::SourceRevisionStatus;
        TemporaryWorld world;
        const ExplorationMapStore mapStore(world.path.string());
        const WorldPreviewStore previewStore(world.path.string());
        const ExplorationMapStore::Identity mapIdentity{
            "world-preview-test", 4207, 22};
        const WorldPreviewStore::Identity identity{
            "world-preview-test", 4207, 22, 1770000000};
        std::string error;
        WorldPreviewStore::Preview preview;
        ExplorationMapStore::SourceRevision revision;

        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Absent &&
                    preview.cells.empty() && error.empty(),
                "missing preview did not report absent");
        require(mapStore.sourceRevision(revision, &error) ==
                        RevisionStatus::Missing &&
                    revision.fileBytes == 0 && error.empty(),
                "missing exploration map did not report missing revision");

        ExplorationAtlas atlas;
        using ObserveResult = ExplorationAtlas::ObserveResult;
        require(atlas.observe({80, -40, 90, BlockId::Grass, true}) ==
                        ObserveResult::Updated &&
                    atlas.observe({-112, -136, 63, BlockId::Sand, true}) ==
                        ObserveResult::Updated &&
                    atlas.observe({272, 56, 62, BlockId::Water, true}) ==
                        ObserveResult::Updated &&
                    atlas.observe({80, -144, 80, BlockId::Stone, true}) ==
                        ObserveResult::Updated,
                "preview exploration fixtures were not accepted");
        ExplorationMarkers markers;
        StorageTransactionMetrics metrics;
        require(mapStore.save(mapIdentity, atlas, markers, std::nullopt,
                              std::nullopt, {}, &metrics) &&
                    metrics.published && metrics.candidateValidated,
                "exploration source was not published");
        require(mapStore.sourceRevision(revision, &error) ==
                        RevisionStatus::Available &&
                    revision.fileBytes ==
                        readBytes(mapStore.filePath()).size() &&
                    revision.trailingChecksum != 0 && error.empty(),
                "source revision did not read bounded metadata and checksum");

        constexpr float yaw = 1.25f;
        require(previewStore.save(identity, 80, -40, yaw, atlas, {},
                                  &metrics) &&
                    metrics.published && metrics.candidateValidated &&
                    metrics.bytesWritten <= WorldPreviewStore::MaxFileBytes &&
                    fs::file_size(previewStore.filePath()) <=
                        WorldPreviewStore::MaxFileBytes &&
                    WorldPreviewStore::validateFile(
                        previewStore.filePath(), &error),
                "bounded preview candidate was not atomically published");
        const std::vector<char> originalPreview =
            readBytes(previewStore.filePath());
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Loaded &&
                    preview.width == WorldPreviewStore::Width &&
                    preview.height == WorldPreviewStore::Height &&
                    preview.metresPerCell ==
                        WorldPreviewStore::MetresPerPixel &&
                    preview.cells.size() == WorldPreviewStore::CellCount &&
                    preview.centerX == 80 && preview.centerZ == -40 &&
                    std::fabs(preview.playerYaw - yaw) < 0.0001f &&
                    error.empty(),
                "preview identity, dimensions or pose did not round trip");
        require(preview.knownCellCount() == 3 &&
                    preview.at(24, 12).known &&
                    preview.at(24, 12).material == BlockId::Grass &&
                    preview.at(0, 0).material == BlockId::Sand &&
                    preview.at(48, 24).material == BlockId::Water &&
                    !preview.at(24, 0).known &&
                    preview.worldXAt(0) == -112 &&
                    preview.worldXAt(48) == 272 &&
                    preview.worldZAt(0) == -136 &&
                    preview.worldZAt(24) == 56,
                "preview did not crop real explored surfaces to 49x25");
        bool boundsRejected = false;
        try {
            (void)preview.at(49, 0);
        }
        catch (const std::out_of_range&) {
            boundsRejected = true;
        }
        require(boundsRejected,
                "preview allowed a cell outside its fixed dimensions");

        for (const WorldPreviewStore::Identity& foreign : {
                 WorldPreviewStore::Identity{
                     "world-preview-other", 4207, 22, 1770000000},
                 WorldPreviewStore::Identity{
                     "world-preview-test", 4208, 22, 1770000000},
                 WorldPreviewStore::Identity{
                     "world-preview-test", 4207, 21, 1770000000}}) {
            require(previewStore.load(foreign, preview, &error) ==
                            PreviewStatus::IdentityMismatch &&
                        preview.cells.empty() && !error.empty() &&
                        readBytes(previewStore.filePath()) == originalPreview,
                    "foreign preview identity leaked or rewrote cache data");
        }
        const WorldPreviewStore::Identity later{
            identity.worldId, identity.seed,
            identity.terrainGenerationVersion,
            identity.lastPlayedUtc + 1};
        require(previewStore.load(later, preview, &error) ==
                        PreviewStatus::StaleSource &&
                    preview.cells.empty() && !error.empty(),
                "changed last-played identity did not stale the preview");

        require(atlas.observe({88, -40, 91, BlockId::ForestFloor, true}) ==
                        ObserveResult::Updated &&
                    mapStore.save(mapIdentity, atlas, markers, std::nullopt,
                                  std::nullopt, {}, &metrics),
                "updated exploration source was not published");
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::StaleSource &&
                    preview.cells.empty() && !error.empty(),
                "changed exploration revision did not stale the preview");
        require(previewStore.save(identity, 80, -40, yaw, atlas, {},
                                  &metrics) &&
                    previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Loaded &&
                    preview.knownCellCount() == 4,
                "stale preview was not regenerated from current exploration");
        const std::vector<char> currentPreview =
            readBytes(previewStore.filePath());

        StorageTransactionOptions fault;
        fault.faultPoint = StorageFaultPoint::BeforeReplace;
        require(!previewStore.save(identity, -400, -800, -0.5f, atlas,
                                   fault, &metrics) &&
                    !metrics.published &&
                    readBytes(previewStore.filePath()) == currentPreview &&
                    previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Loaded &&
                    preview.centerX == 80 && preview.centerZ == -40,
                "failed preview transaction replaced the valid primary");

        std::vector<char> damaged = currentPreview;
        damaged[damaged.size() / 2] ^= 1;
        writeBytes(previewStore.filePath(), damaged);
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Corrupt &&
                    preview.cells.empty() && !error.empty() &&
                    !WorldPreviewStore::validateFile(
                        previewStore.filePath()),
                "damaged preview did not fail closed as disposable cache");
        require(previewStore.save(identity, 80, -40, yaw, atlas, {},
                                  &metrics) &&
                    previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Loaded,
                "damaged regular preview could not be softly regenerated");

        const std::vector<char> currentMap = readBytes(mapStore.filePath());
        ExplorationMapStore::SourceRevision beforeMiddleDamage;
        require(mapStore.sourceRevision(beforeMiddleDamage, &error) ==
                    RevisionStatus::Available,
                "current map revision was unavailable");
        std::vector<char> middleDamage = currentMap;
        middleDamage[middleDamage.size() / 2] ^= 1;
        writeBytes(mapStore.filePath(), middleDamage);
        ExplorationMapStore::SourceRevision afterMiddleDamage;
        require(mapStore.sourceRevision(afterMiddleDamage, &error) ==
                        RevisionStatus::Available &&
                    afterMiddleDamage == beforeMiddleDamage,
                "source revision loaded map pages instead of its stored tail");
        writeBytes(mapStore.filePath(), currentMap);

        std::vector<char> changedTail = currentMap;
        changedTail.back() ^= 1;
        writeBytes(mapStore.filePath(), changedTail);
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::StaleSource &&
                    preview.cells.empty(),
                "changed source checksum did not stale the preview");
        writeBytes(mapStore.filePath(), currentMap);

        const fs::path savedMap = world.path / "saved-exploration.hmap";
        fs::rename(mapStore.filePath(), savedMap);
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::StaleSource &&
                    preview.cells.empty() && !error.empty(),
                "missing preview source did not report stale source");
        fs::create_directory(mapStore.filePath());
        require(mapStore.sourceRevision(revision, &error) ==
                        RevisionStatus::UnsafePath &&
                    previewStore.load(identity, preview, &error) ==
                        PreviewStatus::UnsafePath &&
                    !previewStore.save(identity, 80, -40, yaw, atlas, {},
                                       &metrics),
                "non-regular exploration source was not rejected explicitly");
        fs::remove(mapStore.filePath());
        fs::rename(savedMap, mapStore.filePath());

        const std::vector<char> safePreview =
            readBytes(previewStore.filePath());
        const fs::path outside = world.path / "outside-preview";
        writeBytes(outside, safePreview);
        fs::remove(previewStore.filePath());
        std::error_code symlinkError;
        fs::create_symlink(outside, previewStore.filePath(), symlinkError);
        if (!symlinkError) {
            require(previewStore.load(identity, preview, &error) ==
                            PreviewStatus::UnsafePath &&
                        !previewStore.save(identity, 80, -40, yaw, atlas, {},
                                           &metrics) &&
                        readBytes(outside) == safePreview,
                    "preview symlink was followed or replaced");
            fs::remove(previewStore.filePath());
        }
        fs::create_directory(previewStore.filePath());
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::UnsafePath &&
                    !previewStore.save(identity, 80, -40, yaw, atlas, {},
                                       &metrics),
                "non-regular preview target was not rejected explicitly");
        fs::remove(previewStore.filePath());
        writeBytes(previewStore.filePath(), safePreview);

        std::vector<char> oversized(WorldPreviewStore::MaxFileBytes + 1, 0);
        writeBytes(previewStore.filePath(), oversized);
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Corrupt &&
                    !WorldPreviewStore::validateFile(
                        previewStore.filePath()) &&
                    previewStore.save(identity, -80, -40, -1.f, atlas, {},
                                      &metrics),
                "oversized disposable preview was accepted or not replaceable");
        require(previewStore.load(identity, preview, &error) ==
                        PreviewStatus::Loaded &&
                    preview.centerX == -80 && preview.centerZ == -40 &&
                    preview.cells.size() == WorldPreviewStore::CellCount,
                "negative-coordinate preview did not round trip");

        std::cout << "[WORLD_PREVIEW_STORE] checks=" << checks
                  << " status=PASS\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "[WORLD_PREVIEW_STORE] FAIL "
                  << exception.what() << '\n';
        return 1;
    }
}
