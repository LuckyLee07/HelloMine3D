#include "../Diagnostics/OperationPerformanceTiming.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../World/Exploration/ExplorationMapStore.h"
#include "../World/Exploration/WorldPreviewStore.h"
#include "../World/Storage/WorldBackup.h"
#include "../World/Storage/WorldCatalogue.h"
#include "../World/Storage/WorldManagementService.h"
#include "../World/Storage/WorldSave.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    struct MetadataFixture {
        int version = WorldCatalogue::CurrentSaveFormatVersion;
        std::string id = "world-alpha";
        std::string name = "Alpha World";
        std::string seed = "42";
        std::string created = "1786838400";
        std::string lastPlayed = "1786838460";
        std::string build = "fixture-a1b2c3";
        int outcomePhase =
            static_cast<int>(WorldOutcomePhase::Unstarted);
        std::uint32_t rewardEpoch = 0;
        std::uint32_t claimedRewardEpoch = 0;
        int difficultyProfileVersion = CurrentDifficultyProfileVersion;
        int difficultyId = static_cast<int>(WorldDifficulty::Normal);
        int postVictoryEventVersion = PostVictoryEvents::CurrentVersion;
        int completedPostVictoryEvents = 0;
        int explorationRewardVersion =
            ExplorationRewards::CurrentVersion;
        std::string extra;
    };

    class TemporaryDirectory {
      public:
        explicit TemporaryDirectory(const std::string &label)
        {
            static std::uint64_t sequence = 0;
            const auto tick = std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count();
            m_path = fs::temp_directory_path() /
                     ("hellomine-catalogue-" + label + "-" +
                      std::to_string(tick) + "-" +
                      std::to_string(++sequence));
            std::error_code error;
            fs::remove_all(m_path, error);
        }

        ~TemporaryDirectory()
        {
            std::error_code error;
            fs::remove_all(m_path, error);
            fs::remove_all(m_path.string() + ".recovery", error);
        }

        const fs::path &path() const
        {
            return m_path;
        }

        void create() const
        {
            fs::create_directories(m_path);
        }

      private:
        fs::path m_path;
    };

    class TestSuite {
      public:
        void check(const std::string &id, bool passed,
                   const std::string &detail = std::string())
        {
            ++m_checks;
            if (!passed) {
                ++m_failures;
            }
            std::cout << "[WORLD_CATALOGUE_TEST] "
                      << (passed ? "PASS " : "FAIL ") << id;
            if (!detail.empty()) {
                std::cout << " :: " << detail;
            }
            std::cout << '\n';
        }

        void expectReject(const std::string &id, const std::string &expected,
                          const std::function<void()> &operation)
        {
            std::string actual = "accepted";
            try {
                operation();
            }
            catch (const WorldCatalogueError &error) {
                actual = error.what();
            }
            check(id, actual != "accepted" &&
                          actual.find(expected) != std::string::npos,
                  actual);
        }

        int finish() const
        {
            std::cout << "[WORLD_CATALOGUE_TEST] checks=" << m_checks
                      << " failures=" << m_failures << '\n';
            std::cout << "[WORLD_CATALOGUE_TEST] status="
                      << (m_failures == 0 ? "PASS" : "FAIL") << '\n';
            return m_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

      private:
        int m_checks = 0;
        int m_failures = 0;
    };

    void writeMetadata(const fs::path &root, const std::string &directory,
                       const MetadataFixture &fixture)
    {
        const fs::path world = root / directory;
        fs::create_directories(world);
        std::ofstream output(world / "world.meta", std::ios::binary);
        output << "version " << fixture.version << '\n'
               << "world_id " << fixture.id << '\n'
               << "world_name ";
        if (fixture.version >= 3) {
            output << std::quoted(fixture.name);
        }
        else {
            output << fixture.name;
        }
        output << '\n' << "seed " << fixture.seed << '\n';
        if (fixture.version >= 3) {
            output << "created_utc " << fixture.created << '\n'
                   << "last_played_utc " << fixture.lastPlayed << '\n'
                   << "last_build " << fixture.build << '\n';
        }
        if (fixture.version >= 9) {
            output << "world_outcome_phase " << fixture.outcomePhase << '\n'
                   << "world_outcome_reward_epoch "
                   << fixture.rewardEpoch << '\n'
                   << "world_outcome_claimed_epoch "
                   << fixture.claimedRewardEpoch << '\n';
        }
        if (fixture.version >= 10) {
            output << "difficulty_profile_version "
                   << fixture.difficultyProfileVersion << '\n'
                   << "difficulty_id " << fixture.difficultyId << '\n';
        }
        if (fixture.version >= 11) {
            output << "post_victory_event_version "
                   << fixture.postVictoryEventVersion << '\n'
                   << "post_victory_completed_events "
                   << fixture.completedPostVictoryEvents << '\n';
        }
        if (fixture.version >= 12) {
            output << "exploration_reward_version "
                   << fixture.explorationRewardVersion << '\n';
        }
        output << fixture.extra;
    }

    std::string snapshot(const fs::path &root)
    {
        if (!fs::exists(root)) {
            return "missing";
        }
        std::vector<std::string> records;
        for (const fs::directory_entry &entry :
             fs::recursive_directory_iterator(root)) {
            std::ostringstream record;
            record << entry.path().lexically_relative(root).generic_string()
                   << ':' << static_cast<int>(entry.symlink_status().type());
            if (entry.is_regular_file()) {
                std::ifstream input(entry.path(), std::ios::binary);
                record << ':' << std::string(
                    (std::istreambuf_iterator<char>(input)),
                    std::istreambuf_iterator<char>());
            }
            records.push_back(record.str());
        }
        std::sort(records.begin(), records.end());
        std::ostringstream result;
        for (const std::string &record : records) {
            result << record << '\n';
        }
        return result.str();
    }

    std::string readFile(const fs::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
    }

    MetadataFixture fixtureWith(const std::string &id,
                                const std::string &name,
                                const std::string &created,
                                const std::string &lastPlayed)
    {
        MetadataFixture fixture;
        fixture.id = id;
        fixture.name = name;
        fixture.created = created;
        fixture.lastPlayed = lastPlayed;
        return fixture;
    }

    const WorldCatalogueEntry *findEntry(
        const WorldManagementListResult &listed,
        const std::string &worldId)
    {
        const auto found = std::find_if(
            listed.worlds.begin(), listed.worlds.end(),
            [&](const WorldCatalogueEntry &entry) {
                return entry.id == worldId;
            });
        return found == listed.worlds.end() ? nullptr : &*found;
    }

    ExplorationMapStore::Identity mapIdentity(
        const WorldCatalogueEntry &entry)
    {
        return {entry.id, entry.seed, entry.terrainGenerationVersion};
    }

    WorldPreviewStore::Identity previewIdentity(
        const WorldCatalogueEntry &entry)
    {
        return {entry.id, entry.seed, entry.terrainGenerationVersion,
                entry.lastPlayedUtc};
    }

    bool saveExplorationMap(const WorldCatalogueEntry &entry,
                            const ExplorationAtlas &atlas)
    {
        const ExplorationMarkers markers;
        return ExplorationMapStore(entry.directoryPath)
            .save(mapIdentity(entry), atlas, markers, std::nullopt,
                  std::nullopt);
    }

    bool saveWorldPreview(const WorldCatalogueEntry &entry,
                          const ExplorationAtlas &atlas, int centerX,
                          int centerZ, float playerYaw,
                          const WorldPreviewStore::Identity *identity =
                              nullptr)
    {
        const WorldPreviewStore::Identity expected =
            identity == nullptr ? previewIdentity(entry) : *identity;
        return WorldPreviewStore(entry.directoryPath)
            .save(expected, centerX, centerZ, playerYaw, atlas);
    }
}

int main()
{
    TestSuite suite;
    RuntimeOperationTimings &operationTimings = runtimeOperationTimings();
    operationTimings.reset(true);

    {
        TemporaryDirectory missing("missing-root");
        const auto entries = WorldCatalogue::enumerate(missing.path().string());
        suite.check("K1/missing-root-is-empty-and-not-created",
                    entries.empty() && !fs::exists(missing.path()));
    }

    {
        TemporaryDirectory empty("empty");
        empty.create();
        const std::string before = snapshot(empty.path());
        const auto entries = WorldCatalogue::enumerate(empty.path().string());
        suite.check("K1/empty-catalogue", entries.empty());
        suite.check("K1/empty-enumeration-is-read-only",
                    before == snapshot(empty.path()));
    }

    {
        TemporaryDirectory multiple("multiple");
        multiple.create();
        writeMetadata(multiple.path(), "folder-z",
                      fixtureWith("world-z", "\xe4\xb8\x96\xe7\x95\x8c Z", "1786838300",
                                  "1786838500"));
        writeMetadata(multiple.path(), "folder-a",
                      fixtureWith("world-a", "Alpha World", "1786838400",
                                  "1786838500"));
        writeMetadata(multiple.path(), "folder-b",
                      fixtureWith("world-b", "Beta World", "1786838400",
                                  "1786838600"));
        const std::string before = snapshot(multiple.path());
        const auto first = WorldCatalogue::enumerate(multiple.path().string());
        const auto second = WorldCatalogue::enumerate(multiple.path().string());
        const bool order = first.size() == 3 && first[0].id == "world-b" &&
                           first[1].id == "world-a" &&
                           first[2].id == "world-z";
        suite.check("K1/multiple-worlds-stable-order", order);
        suite.check("K1/current-version-fields",
                    first.size() == 3 && first[0].displayName == "Beta World" &&
                        first[0].directoryName == "folder-b" &&
                        first[0].saveFormatVersion ==
                            WorldCatalogue::CurrentSaveFormatVersion &&
                        first[0].seed == 42 &&
                        first[0].createdUtc == 1786838400 &&
                        first[0].lastPlayedUtc == 1786838600 &&
                        first[0].lastBuildIdentity == "fixture-a1b2c3" &&
                        first[0].outcomePhase ==
                            WorldOutcomePhase::Unstarted &&
                        first[0].difficultyProfileVersion ==
                            CurrentDifficultyProfileVersion &&
                        first[0].difficulty == WorldDifficulty::Normal &&
                        first[0].postVictoryEventVersion ==
                            PostVictoryEvents::CurrentVersion &&
                        first[0].completedPostVictoryEvents == 0 &&
                        !first[0].completed &&
                        !first[0].legacyMetadata);
        suite.check("K1/repeated-enumeration-is-deterministic",
                    first.size() == second.size() &&
                        std::equal(first.begin(), first.end(), second.begin(),
                                   [](const WorldCatalogueEntry &left,
                                      const WorldCatalogueEntry &right) {
                                       return left.id == right.id &&
                                              left.directoryPath ==
                                                  right.directoryPath;
                                   }));
        suite.check("K1/enumeration-never-mutates-worlds",
                    before == snapshot(multiple.path()));
    }

    {
        TemporaryDirectory outcomes("outcomes");
        outcomes.create();
        MetadataFixture victorious;
        victorious.id = "victorious-world";
        victorious.name = "Victorious World";
        victorious.outcomePhase =
            static_cast<int>(WorldOutcomePhase::Victorious);
        victorious.rewardEpoch = 7;
        writeMetadata(outcomes.path(), "victorious", victorious);
        MetadataFixture claimed = victorious;
        claimed.id = "claimed-world";
        claimed.name = "Claimed World";
        claimed.outcomePhase =
            static_cast<int>(WorldOutcomePhase::RewardClaimed);
        claimed.claimedRewardEpoch = 7;
        writeMetadata(outcomes.path(), "claimed", claimed);
        const auto entries = WorldCatalogue::enumerate(outcomes.path().string());
        const bool bothCompleted = entries.size() == 2 &&
            std::all_of(entries.begin(), entries.end(),
                        [](const WorldCatalogueEntry &entry) {
                            return entry.completed &&
                                   (entry.outcomePhase ==
                                        WorldOutcomePhase::Victorious ||
                                    entry.outcomePhase ==
                                        WorldOutcomePhase::RewardClaimed);
                        });
        suite.check("N7A/list-marker-uses-persisted-outcome-only",
                    bothCompleted);
    }

    {
        TemporaryDirectory invalid("invalid-outcomes");
        invalid.create();
        MetadataFixture phase;
        phase.id = "invalid-phase";
        phase.outcomePhase = 99;
        writeMetadata(invalid.path(), "phase", phase);
        suite.expectReject("N7A/reject-invalid-outcome-phase",
                           "outside its range", [&]() {
                               WorldCatalogue::enumerate(
                                   invalid.path().string());
                           });
    }

    {
        TemporaryDirectory invalid("legacy-outcome");
        invalid.create();
        MetadataFixture legacy;
        legacy.version = 8;
        legacy.id = "legacy-outcome";
        legacy.extra = "world_outcome_phase 3\n"
                       "world_outcome_reward_epoch 1\n"
                       "world_outcome_claimed_epoch 0\n";
        writeMetadata(invalid.path(), "world", legacy);
        suite.expectReject("N7A/reject-outcome-fields-before-v9",
                           "require save format version 9", [&]() {
                               WorldCatalogue::enumerate(
                                   invalid.path().string());
                           });
    }

    {
        TemporaryDirectory previous("previous-v3");
        previous.create();
        MetadataFixture version3;
        version3.version = 3;
        version3.id = "previous-three";
        version3.name = "Previous Three";
        writeMetadata(previous.path(), "stable-v3-folder", version3);
        const auto entries =
            WorldCatalogue::enumerate(previous.path().string());
        suite.check("G6/version-three-catalogue-remains-readable",
                    entries.size() == 1 &&
                        entries.front().saveFormatVersion == 3 &&
                        entries.front().id == version3.id &&
                        !entries.front().legacyMetadata);
    }

    {
        TemporaryDirectory legacy("legacy");
        legacy.create();
        MetadataFixture version1;
        version1.version = 1;
        version1.id = "legacy-one";
        version1.name = "LegacyOne";
        version1.seed = "11";
        MetadataFixture version2 = version1;
        version2.version = 2;
        version2.id = "legacy-two";
        version2.name = "LegacyTwo";
        version2.seed = "22";
        writeMetadata(legacy.path(), "unmodified-v1-folder", version1);
        writeMetadata(legacy.path(), "unmodified-v2-folder", version2);
        const auto entries = WorldCatalogue::enumerate(legacy.path().string());
        suite.check("K1/version-one-discovered-without-rename",
                    entries.size() == 2 && entries[0].id == "legacy-one" &&
                        entries[0].directoryName == "unmodified-v1-folder" &&
                        entries[0].legacyMetadata &&
                        entries[0].terrainGenerationVersion == 1 &&
                        entries[0].createdUtc == LegacyWorldTimestampUtc &&
                        entries[0].lastBuildIdentity == "legacy-v1");
        suite.check("K1/version-two-discovered-without-rename",
                    entries.size() == 2 && entries[1].id == "legacy-two" &&
                        entries[1].directoryName == "unmodified-v2-folder" &&
                        entries[1].legacyMetadata &&
                        entries[1].terrainGenerationVersion == 1 &&
                        entries[1].lastPlayedUtc == LegacyWorldTimestampUtc &&
                        entries[1].lastBuildIdentity == "legacy-v2");
    }

    {
        TemporaryDirectory duplicate("duplicate");
        duplicate.create();
        writeMetadata(duplicate.path(), "first", MetadataFixture{});
        writeMetadata(duplicate.path(), "second", MetadataFixture{});
        suite.expectReject("K1/duplicate-world-id", "Duplicate world id",
                           [&]() {
                               WorldCatalogue::enumerate(
                                   duplicate.path().string());
                           });
    }

    const std::vector<std::pair<std::string, std::string>> invalidNames = {
        {"empty", ""},
        {"leading-space", " Leading"},
        {"traversal", ".."},
        {"separator", "Bad/Name"},
        {"invalid-utf8", std::string("Bad\xc0\xaf", 5)},
        {"too-long", std::string(81, 'a')}};
    for (const auto &invalid : invalidNames) {
        TemporaryDirectory root("name-" + invalid.first);
        root.create();
        MetadataFixture fixture;
        fixture.name = invalid.second;
        writeMetadata(root.path(), "world", fixture);
        suite.expectReject("K1/invalid-name-" + invalid.first,
                           "world_name", [&]() {
                               WorldCatalogue::enumerate(root.path().string());
                           });
    }

    const std::vector<std::pair<std::string, MetadataFixture>> invalidTimes = {
        {"non-integer", fixtureWith("world-a", "World", "tomorrow",
                                    "1786838500")},
        {"before-epoch", fixtureWith("world-a", "World", "1",
                                     "1786838500")},
        {"reverse", fixtureWith("world-a", "World", "1786838500",
                                "1786838400")},
        {"too-large", fixtureWith("world-a", "World", "1786838500",
                                  "253402300800")}};
    for (const auto &invalid : invalidTimes) {
        TemporaryDirectory root("time-" + invalid.first);
        root.create();
        writeMetadata(root.path(), "world", invalid.second);
        suite.expectReject("K1/invalid-timestamp-" + invalid.first,
                           invalid.first == "non-integer" ? "canonical integer"
                                                           : "timestamps",
                           [&]() {
                               WorldCatalogue::enumerate(root.path().string());
                           });
    }

    for (const int invalidVersion :
         {0, WorldCatalogue::CurrentSaveFormatVersion + 1}) {
        TemporaryDirectory root("version-" +
                                std::to_string(invalidVersion));
        root.create();
        MetadataFixture fixture;
        fixture.version = invalidVersion;
        fixture.name = "World";
        writeMetadata(root.path(), "world", fixture);
        suite.expectReject("K1/invalid-version-" +
                               std::to_string(invalidVersion),
                           "unsupported save format version", [&]() {
                               WorldCatalogue::enumerate(root.path().string());
                           });
    }

    for (const auto &invalidDifficulty :
         std::vector<std::pair<std::string, MetadataFixture>>{
             {"profile-zero", [] {
                  MetadataFixture fixture;
                  fixture.difficultyProfileVersion = 0;
                  return fixture;
              }()},
             {"profile-future", [] {
                  MetadataFixture fixture;
                  fixture.difficultyProfileVersion =
                      CurrentDifficultyProfileVersion + 1;
                  return fixture;
              }()},
             {"id-negative", [] {
                  MetadataFixture fixture;
                  fixture.difficultyId = -1;
                  return fixture;
              }()},
             {"id-out-of-range", [] {
                  MetadataFixture fixture;
                  fixture.difficultyId =
                      static_cast<int>(WorldDifficulty::Count);
                  return fixture;
              }()}}) {
        TemporaryDirectory root("difficulty-" + invalidDifficulty.first);
        root.create();
        writeMetadata(root.path(), "world", invalidDifficulty.second);
        suite.expectReject("N11A/catalogue-rejects-" +
                               invalidDifficulty.first,
                           "difficulty profile version or id", [&]() {
                               WorldCatalogue::enumerate(root.path().string());
                           });
    }

    {
        TemporaryDirectory legacyDifficulty("difficulty-on-v9");
        legacyDifficulty.create();
        MetadataFixture fixture;
        fixture.version = 9;
        fixture.extra = "difficulty_profile_version 1\n"
                        "difficulty_id 1\n";
        writeMetadata(legacyDifficulty.path(), "world", fixture);
        suite.expectReject("N11A/v9-catalogue-rejects-difficulty-fields",
                           "require save format version 10", [&]() {
                               WorldCatalogue::enumerate(
                                   legacyDifficulty.path().string());
                           });
    }

    {
        TemporaryDirectory traversal("traversal");
        traversal.create();
        MetadataFixture fixture;
        fixture.id = "../outside";
        writeMetadata(traversal.path(), "world", fixture);
        suite.expectReject("K1/world-id-path-escape", "world_id", [&]() {
            WorldCatalogue::enumerate(traversal.path().string());
        });
    }

    for (const auto &invalidReplay :
         std::vector<std::pair<std::string, MetadataFixture>>{
             {"version-zero", [] {
                  MetadataFixture fixture;
                  fixture.postVictoryEventVersion = 0;
                  return fixture;
              }()},
             {"version-future", [] {
                  MetadataFixture fixture;
                  fixture.postVictoryEventVersion =
                      PostVictoryEvents::CurrentVersion + 1;
                  return fixture;
              }()},
             {"completed-negative", [] {
                  MetadataFixture fixture;
                  fixture.completedPostVictoryEvents = -1;
                  return fixture;
              }()},
             {"completed-over-cap", [] {
                  MetadataFixture fixture;
                  fixture.completedPostVictoryEvents =
                      PostVictoryEvents::MaximumEvents + 1;
                  return fixture;
              }()},
             {"progress-before-reward", [] {
                  MetadataFixture fixture;
                  fixture.completedPostVictoryEvents = 1;
                  return fixture;
              }()}}) {
        TemporaryDirectory root("post-victory-" + invalidReplay.first);
        root.create();
        writeMetadata(root.path(), "world", invalidReplay.second);
        suite.expectReject("N11B/catalogue-rejects-" +
                               invalidReplay.first,
                           "post-victory event progress", [&]() {
                               WorldCatalogue::enumerate(
                                   root.path().string());
                           });
    }

    {
        TemporaryDirectory legacyReplay("post-victory-on-v10");
        legacyReplay.create();
        MetadataFixture fixture;
        fixture.version = 10;
        fixture.extra = "post_victory_event_version 1\n"
                        "post_victory_completed_events 0\n";
        writeMetadata(legacyReplay.path(), "world", fixture);
        suite.expectReject("N11B/v10-catalogue-rejects-replay-fields",
                           "require save format version 11", [&]() {
                               WorldCatalogue::enumerate(
                                   legacyReplay.path().string());
                           });
    }

    {
        TemporaryDirectory missing("missing-meta");
        missing.create();
        fs::create_directories(missing.path() / "world");
        suite.expectReject("K1/missing-metadata-file",
                           "Missing world metadata", [&]() {
                               WorldCatalogue::enumerate(
                                   missing.path().string());
                           });
    }

    {
        TemporaryDirectory malformed("malformed");
        malformed.create();
        MetadataFixture fixture;
        fixture.extra = "world_id duplicate\n";
        writeMetadata(malformed.path(), "world", fixture);
        suite.expectReject("K1/duplicate-metadata-field",
                           "duplicate field 'world_id'", [&]() {
                               WorldCatalogue::enumerate(
                                   malformed.path().string());
                           });
    }

    {
        TemporaryDirectory unknown("unknown");
        unknown.create();
        MetadataFixture fixture;
        fixture.extra = "mystery value\n";
        writeMetadata(unknown.path(), "world", fixture);
        suite.expectReject("K1/unknown-metadata-field", "unknown field",
                           [&]() {
                               WorldCatalogue::enumerate(
                                   unknown.path().string());
                           });
    }

    {
        TemporaryDirectory symlinkRoot("symlink");
        TemporaryDirectory outside("outside");
        symlinkRoot.create();
        outside.create();
        writeMetadata(outside.path(), "target", MetadataFixture{});
        std::error_code error;
        fs::create_directory_symlink(outside.path() / "target",
                                     symlinkRoot.path() / "linked-world",
                                     error);
        if (!error) {
            suite.expectReject("K1/symlink-world-rejected", "symlinks",
                               [&]() {
                                   WorldCatalogue::enumerate(
                                       symlinkRoot.path().string());
                               });
        }
        else {
            suite.check("K1/symlink-world-rejected", true,
                        "host cannot create test symlink: " + error.message());
        }
    }

    {
        TemporaryDirectory symlinkMeta("symlink-meta");
        TemporaryDirectory outside("outside-meta");
        symlinkMeta.create();
        outside.create();
        writeMetadata(outside.path(), "source", MetadataFixture{});
        fs::create_directories(symlinkMeta.path() / "world");
        std::error_code error;
        fs::create_symlink(outside.path() / "source" / "world.meta",
                           symlinkMeta.path() / "world" / "world.meta",
                           error);
        if (!error) {
            suite.expectReject("K1/symlink-metadata-rejected",
                               "real regular file", [&]() {
                                   WorldCatalogue::enumerate(
                                       symlinkMeta.path().string());
                               });
        }
        else {
            suite.check("K1/symlink-metadata-rejected", true,
                        "host cannot create test symlink: " + error.message());
        }
    }

    {
        TemporaryDirectory root("management");
        const WorldManagementService service(root.path().string());
        const WorldManagementListResult empty = service.listWorlds();
        suite.check("K4/missing-catalogue-is-empty-and-read-only",
                    empty.succeeded() && empty.worlds.empty() &&
                        !fs::exists(root.path()));
        suite.check("K4/invalid-create-name-is-rejected",
                    service.createWorld("../outside", 1).status ==
                            WorldManagementStatus::InvalidArgument &&
                        !fs::exists(root.path()));

        const WorldManagementResult first =
            service.createWorld("Same Name", 101);
        const WorldManagementResult second =
            service.createWorld("Same Name", 202);
        const WorldManagementListResult created = service.listWorlds();
        suite.check(
            "K4/same-display-name-keeps-distinct-stable-identities",
            first.succeeded() && second.succeeded() &&
                first.worldId != second.worldId &&
                first.directoryPath != second.directoryPath &&
                created.succeeded() && created.worlds.size() == 2);
        suite.check(
            "K4/catalogue-shows-saved-terrain-generation-version",
            created.succeeded() && created.worlds.size() == 2 &&
                std::all_of(created.worlds.begin(), created.worlds.end(),
                    [](const WorldCatalogueEntry& entry) {
                        return entry.terrainGenerationVersion ==
                            CurrentTerrainGenerationVersion;
                    }));

        const fs::path firstDirectory(first.directoryPath);
        const WorldManagementResult renamed =
            service.renameWorld(first.worldId, "Renamed World");
        const WorldManagementResult opened =
            service.prepareWorldForOpen(first.worldId);
        const WorldManagementListResult afterRename = service.listWorlds();
        const auto renamedEntry = std::find_if(
            afterRename.worlds.begin(), afterRename.worlds.end(),
            [&](const WorldCatalogueEntry &entry) {
                return entry.id == first.worldId;
            });
        suite.check(
            "K4/rename-and-open-preserve-id-and-directory",
            renamed.succeeded() && opened.succeeded() &&
                renamed.worldId == first.worldId &&
                opened.worldId == first.worldId &&
                // macOS temp roots may use /var while enumeration returns
                // canonical /private/var. Check the real directory identity.
                fs::equivalent(renamed.directoryPath, firstDirectory) &&
                fs::equivalent(opened.directoryPath, firstDirectory) &&
                renamedEntry != afterRename.worlds.end() &&
                renamedEntry->displayName == "Renamed World" &&
                fs::equivalent(renamedEntry->directoryPath, firstDirectory),
            "created=" + first.directoryPath +
                " renamed=" + renamed.directoryPath +
                " rename_status=" + worldManagementStatusName(renamed.status) +
                " open_status=" + worldManagementStatusName(opened.status) +
                " listed=" + (renamedEntry != afterRename.worlds.end()
                    ? renamedEntry->directoryPath + ":" + renamedEntry->displayName
                    : "missing"));
        suite.check(
            "K4/path-traversal-command-is-rejected",
            service.deleteWorld("../outside").status ==
                    WorldManagementStatus::InvalidArgument &&
                fs::is_directory(firstDirectory));

        const WorldManagementResult deleted =
            service.deleteWorld(first.worldId);
        const DeletedWorldListResult deletedWorlds =
            service.listDeletedWorlds();
        const WorldManagementResult restored =
            service.restoreDeletedWorld(first.worldId);
        suite.check(
            "K4/delete-is-recoverable-and-restores-original-directory",
            deleted.succeeded() && deletedWorlds.succeeded() &&
                deletedWorlds.worlds.size() == 1 &&
                deletedWorlds.worlds.front().world.id == first.worldId &&
                restored.succeeded() &&
                fs::path(restored.directoryPath) == firstDirectory &&
                fs::is_directory(firstDirectory));

        const WorldManagementResult deleteSecond =
            service.deleteWorld(second.worldId);
        const WorldManagementResult purgeSecond =
            service.permanentlyDeleteWorld(second.worldId);
        suite.check("K4/permanent-delete-only-targets-recovery-entry",
                    deleteSecond.succeeded() && purgeSecond.succeeded() &&
                        service.listDeletedWorlds().worlds.empty() &&
                        fs::is_directory(firstDirectory));
    }

    {
        TemporaryDirectory root("recovery-bound");
        WorldManagementPolicy policy;
        policy.maxRecoverableDeletes = 2;
        const WorldManagementService service(root.path().string(), policy);
        std::vector<std::string> ids;
        bool completed = true;
        for (int index = 0; index < 3; ++index) {
            const WorldManagementResult created = service.createWorld(
                "Bounded " + std::to_string(index), 500 + index);
            completed = completed && created.succeeded();
            ids.push_back(created.worldId);
        }
        for (const std::string &id : ids) {
            completed = completed && service.deleteWorld(id).succeeded();
        }
        const DeletedWorldListResult deleted = service.listDeletedWorlds();
        const bool oldestEvicted =
            std::none_of(deleted.worlds.begin(), deleted.worlds.end(),
                         [&](const DeletedWorldInfo &entry) {
                             return entry.world.id == ids.front();
                         });
        suite.check("K4/recoverable-delete-count-is-bounded",
                    completed && deleted.succeeded() &&
                        deleted.worlds.size() == 2 && oldestEvicted);
    }

    {
        TemporaryDirectory root("legacy-management");
        root.create();
        MetadataFixture legacy;
        legacy.version = 1;
        legacy.id = "legacy-managed";
        legacy.name = "LegacyManaged";
        legacy.seed = "37";
        writeMetadata(root.path(), "stable-legacy-directory", legacy);
        const WorldManagementService service(root.path().string());
        const WorldManagementResult renamed =
            service.renameWorld(legacy.id, "Legacy Renamed");
        WorldSaveData loaded;
        const bool loadedSave = WorldSave::loadFromPath(
            (root.path() / "stable-legacy-directory" / "world.meta")
                .string(),
            loaded);
        suite.check(
            "K4/legacy-rename-upgrades-metadata-without-moving-directory",
            renamed.succeeded() && loadedSave &&
                loaded.version == WorldSaveFormatVersion &&
                loaded.worldId == legacy.id &&
                loaded.worldName == "Legacy Renamed" &&
                fs::is_directory(root.path() / "stable-legacy-directory"));
    }

    {
        TemporaryDirectory root("corrupt-management");
        root.create();
        fs::create_directories(root.path() / "missing-meta");
        const WorldManagementListResult listed =
            WorldManagementService(root.path().string()).listWorlds();
        const WorldSelectionDetails selected =
            WorldManagementService(root.path().string())
                .inspectWorld("world-alpha");
        suite.check("K4/corrupt-catalogue-has-structured-failure",
                    listed.status ==
                            WorldManagementStatus::CatalogueInvalid &&
                        !listed.message.empty() && !selected.succeeded() &&
                        selected.status ==
                            WorldManagementStatus::CatalogueInvalid);
    }

    {
        TemporaryDirectory root("management-preview-absent");
        const WorldManagementService service(root.path().string());
        const WorldManagementResult created =
            service.createWorld("Preview Legacy", 606);
        const std::string beforeSelection = snapshot(root.path());
        const WorldSelectionDetails absent =
            service.inspectWorld(created.worldId);
        suite.check(
            "B6/selected-old-world-without-preview-remains-usable",
            created.succeeded() && absent.succeeded() &&
                absent.world.id == created.worldId &&
                absent.backups.empty() && !absent.preview.has_value() &&
                absent.previewStatus ==
                    WorldPreviewStore::LoadStatus::Absent &&
                beforeSelection == snapshot(root.path()));

        if (created.succeeded()) {
            std::ofstream corrupt(
                fs::path(created.directoryPath) / "world-preview.hmp",
                std::ios::binary | std::ios::trunc);
            corrupt << "not-a-preview";
        }
        const std::string beforeCatalogue = snapshot(root.path());
        const WorldManagementListResult listed = service.listWorlds();
        suite.check(
            "B6/catalogue-enumeration-never-opens-preview-payload",
            listed.succeeded() && listed.worlds.size() == 1 &&
                beforeCatalogue == snapshot(root.path()));
        const WorldSelectionDetails corrupt =
            service.inspectWorld(created.worldId);
        suite.check(
            "B6/corrupt-selected-preview-is-soft-fallback",
            corrupt.succeeded() && !corrupt.preview.has_value() &&
                corrupt.previewStatus ==
                    WorldPreviewStore::LoadStatus::Corrupt &&
                !corrupt.previewMessage.empty());

        if (created.succeeded()) {
            std::ofstream invalidBackups(
                fs::path(created.directoryPath) / "backups",
                std::ios::binary | std::ios::trunc);
            invalidBackups << "not-a-directory";
        }
        const WorldSelectionDetails backupFailure =
            service.inspectWorld(created.worldId);
        suite.check(
            "B6/selected-world-backup-failure-remains-hard",
            !backupFailure.succeeded() &&
                backupFailure.status ==
                    WorldManagementStatus::StorageFailure &&
                !backupFailure.message.empty());
    }

    {
        TemporaryDirectory root("management-preview-selected");
        const WorldManagementService service(root.path().string());
        const WorldManagementResult firstCreated =
            service.createWorld("Preview First", 701);
        const WorldManagementResult secondCreated =
            service.createWorld("Preview Second", 702);
        const WorldManagementListResult listed = service.listWorlds();
        const WorldCatalogueEntry *first =
            findEntry(listed, firstCreated.worldId);
        const WorldCatalogueEntry *second =
            findEntry(listed, secondCreated.worldId);

        ExplorationAtlas firstAtlas;
        ExplorationAtlas secondAtlas;
        bool fixturesSaved = first != nullptr && second != nullptr;
        if (fixturesSaved) {
            fixturesSaved =
                firstAtlas.observe({0, 0, 71, BlockId::ForestFloor,
                                    true}) ==
                    ExplorationAtlas::ObserveResult::Updated &&
                secondAtlas.observe({800, -400, 63, BlockId::Sand,
                                     true}) ==
                    ExplorationAtlas::ObserveResult::Updated &&
                saveExplorationMap(*first, firstAtlas) &&
                saveWorldPreview(*first, firstAtlas, 0, 0, 45.f) &&
                saveExplorationMap(*second, secondAtlas) &&
                saveWorldPreview(*second, secondAtlas, 800, -400,
                                 225.f);
        }
        const WorldSelectionDetails firstSelected =
            service.inspectWorld(firstCreated.worldId);
        const WorldSelectionDetails secondSelected =
            service.inspectWorld(secondCreated.worldId);
        const WorldSelectionDetails firstAgain =
            service.inspectWorld(firstCreated.worldId);
        const bool firstSurface = firstSelected.preview.has_value() &&
            firstSelected.preview->at(WorldPreviewStore::Width / 2,
                                      WorldPreviewStore::Height / 2)
                    .known &&
            firstSelected.preview->at(WorldPreviewStore::Width / 2,
                                      WorldPreviewStore::Height / 2)
                    .material == BlockId::ForestFloor;
        const bool secondSurface = secondSelected.preview.has_value() &&
            secondSelected.preview->at(WorldPreviewStore::Width / 2,
                                       WorldPreviewStore::Height / 2)
                    .known &&
            secondSelected.preview->at(WorldPreviewStore::Width / 2,
                                       WorldPreviewStore::Height / 2)
                    .material == BlockId::Sand;
        suite.check(
            "B6/selected-world-loads-real-bounded-preview",
            fixturesSaved && firstSelected.succeeded() && firstSurface &&
                firstSelected.previewStatus ==
                    WorldPreviewStore::LoadStatus::Loaded &&
                firstSelected.preview->centerX == 0 &&
                firstSelected.preview->centerZ == 0 &&
                firstSelected.preview->cells.size() ==
                    WorldPreviewStore::CellCount);
        suite.check(
            "B6/switching-selection-does-not-cross-contaminate-preview",
            secondSelected.succeeded() && secondSurface &&
                secondSelected.preview->centerX == 800 &&
                secondSelected.preview->centerZ == -400 &&
                firstAgain.succeeded() && firstAgain.preview.has_value() &&
                firstAgain.preview->centerX == 0 &&
                firstAgain.preview->at(WorldPreviewStore::Width / 2,
                                       WorldPreviewStore::Height / 2)
                        .material == BlockId::ForestFloor);

        bool foreignSaved = false;
        if (first != nullptr) {
            WorldPreviewStore::Identity foreign = previewIdentity(*first);
            foreign.worldId = "foreign-preview";
            foreignSaved = saveWorldPreview(*first, firstAtlas, 0, 0,
                                             45.f, &foreign);
        }
        const WorldSelectionDetails foreign =
            service.inspectWorld(firstCreated.worldId);
        suite.check(
            "B6/foreign-selected-preview-is-soft-fallback",
            foreignSaved && foreign.succeeded() &&
                !foreign.preview.has_value() &&
                foreign.previewStatus ==
                    WorldPreviewStore::LoadStatus::IdentityMismatch);

        if (first != nullptr) {
            std::ofstream corrupt(
                fs::path(first->directoryPath) / "world-preview.hmp",
                std::ios::binary | std::ios::trunc);
            corrupt << "broken";
        }
        const WorldSelectionDetails corrupt =
            service.inspectWorld(firstCreated.worldId);
        suite.check(
            "B6/malformed-selected-preview-does-not-fail-selection",
            corrupt.succeeded() && !corrupt.preview.has_value() &&
                corrupt.previewStatus ==
                    WorldPreviewStore::LoadStatus::Corrupt);

        bool sourceChanged = false;
        if (first != nullptr) {
            const bool rewritten =
                saveWorldPreview(*first, firstAtlas, 0, 0, 45.f);
            sourceChanged = rewritten &&
                firstAtlas.observe({8, 0, 74, BlockId::MossStone, true}) ==
                    ExplorationAtlas::ObserveResult::Updated &&
                saveExplorationMap(*first, firstAtlas);
        }
        const WorldSelectionDetails stale =
            service.inspectWorld(firstCreated.worldId);
        suite.check(
            "B6/stale-source-selected-preview-is-soft-fallback",
            sourceChanged && stale.succeeded() &&
                !stale.preview.has_value() &&
                stale.previewStatus ==
                    WorldPreviewStore::LoadStatus::StaleSource);

        bool unsafePrepared = false;
        if (first != nullptr) {
            std::error_code error;
            const fs::path previewPath =
                fs::path(first->directoryPath) / "world-preview.hmp";
            fs::remove(previewPath, error);
            unsafePrepared = !error && fs::create_directory(previewPath,
                                                             error) &&
                             !error;
        }
        const WorldSelectionDetails unsafe =
            service.inspectWorld(firstCreated.worldId);
        suite.check(
            "B6/unsafe-selected-preview-path-is-soft-fallback",
            unsafePrepared && unsafe.succeeded() &&
                !unsafe.preview.has_value() &&
                unsafe.previewStatus ==
                    WorldPreviewStore::LoadStatus::UnsafePath);
    }

    {
        TemporaryDirectory root("management-preview-restore");
        const WorldManagementService service(root.path().string());
        const WorldManagementResult created =
            service.createWorld("Preview Restore", 808);
        const WorldManagementListResult listed = service.listWorlds();
        const WorldCatalogueEntry *entry =
            findEntry(listed, created.worldId);
        ExplorationAtlas atlas;
        bool initialSaved = entry != nullptr;
        if (initialSaved) {
            initialSaved =
                atlas.observe({16, 24, 68, BlockId::ForestFloor, true}) ==
                    ExplorationAtlas::ObserveResult::Updated &&
                saveExplorationMap(*entry, atlas) &&
                saveWorldPreview(*entry, atlas, 16, 24, 90.f);
        }
        WorldBackupInfo backup;
        const bool backedUp = initialSaved &&
            WorldBackup(created.directoryPath).createBackup(&backup);
        const WorldManagementResult renamed =
            service.renameWorld(created.worldId, "Preview Current");
        bool alternatePreviewSaved = false;
        if (entry != nullptr) {
            // Keep world.meta and exploration.hmap byte-identical to the
            // backup. Only the disposable camera centre changes, reproducing
            // the same-second/same-source restore collision.
            alternatePreviewSaved =
                saveWorldPreview(*entry, atlas, 240, -160, 180.f);
        }
        const WorldSelectionDetails beforeRestore =
            service.inspectWorld(created.worldId);
        const std::string metadataBeforeFailure = readFile(
            fs::path(created.directoryPath) / "world.meta");
        const std::string mapBeforeFailure = readFile(
            fs::path(created.directoryPath) / "exploration.hmap");
        WorldBackupOptions interrupted;
        interrupted.faultPoint =
            WorldBackupFaultPoint::AfterFirstRestorePublish;
        const WorldManagementResult failedRestore = service.restoreBackup(
            created.worldId, backup.id, interrupted);
        const WorldSelectionDetails afterFailedRestore =
            service.inspectWorld(created.worldId);
        const std::string metadataAfterFailedRestore = readFile(
            fs::path(created.directoryPath) / "world.meta");
        const std::string mapAfterFailedRestore = readFile(
            fs::path(created.directoryPath) / "exploration.hmap");
        const WorldManagementResult restored =
            service.restoreBackup(created.worldId, backup.id);
        const WorldSelectionDetails afterRestore =
            service.inspectWorld(created.worldId);
        suite.check(
            "B6/preview-cache-is-excluded-from-authoritative-backup",
            backedUp && backup.fileCount == 2 &&
                !fs::exists(fs::path(backup.directoryPath) /
                            "world-preview.hmp"));
        suite.check(
            "B6/failed-backup-restore-drops-preview-and-rolls-back-world",
            renamed.succeeded() && alternatePreviewSaved &&
                beforeRestore.succeeded() &&
                beforeRestore.preview.has_value() &&
                beforeRestore.preview->centerX == 240 &&
                beforeRestore.preview->centerZ == -160 &&
                !failedRestore.succeeded() &&
                afterFailedRestore.succeeded() &&
                afterFailedRestore.world.displayName == "Preview Current" &&
                !afterFailedRestore.preview.has_value() &&
                afterFailedRestore.previewStatus ==
                    WorldPreviewStore::LoadStatus::Absent &&
                metadataBeforeFailure == metadataAfterFailedRestore &&
                mapBeforeFailure == mapAfterFailedRestore,
            failedRestore.message);
        suite.check(
            "B6/same-revision-backup-restore-discards-old-preview-centre",
            restored.succeeded() &&
                !fs::exists(fs::path(created.directoryPath) /
                            "world-preview.hmp") &&
                afterRestore.succeeded() &&
                afterRestore.world.displayName == "Preview Restore" &&
                !afterRestore.preview.has_value() &&
                afterRestore.previewStatus ==
                    WorldPreviewStore::LoadStatus::Absent,
            afterRestore.previewMessage);
    }

    {
        TemporaryDirectory root("management-backup");
        const WorldManagementService service(root.path().string());
        const WorldManagementResult created =
            service.createWorld("Backup Original", 909);
        WorldBackupInfo backupInfo;
        WorldBackupMetrics createMetrics;
        const bool backupCreated =
            created.succeeded() &&
            WorldBackup(created.directoryPath)
                .createBackup(&backupInfo, &createMetrics);
        const bool mutated =
            service.renameWorld(created.worldId, "Backup Modified")
                .succeeded();
        std::vector<WorldBackupInfo> backups;
        WorldManagementResult listResult;
        const bool listed = service.listBackups(
            created.worldId, backups, &listResult);
        const WorldManagementResult restored = service.restoreBackup(
            created.worldId, backupInfo.id);
        const WorldManagementListResult afterRestore = service.listWorlds();
        suite.check(
            "K4/backup-list-and-restore-use-structured-service-boundary",
            backupCreated && mutated && listed && listResult.succeeded() &&
                backups.size() == 1 && restored.succeeded() &&
                afterRestore.succeeded() && afterRestore.worlds.size() == 1 &&
                afterRestore.worlds.front().displayName == "Backup Original",
            restored.message);

        const bool changedAgain =
            service.renameWorld(created.worldId, "Rollback Primary")
                .succeeded();
        const std::string beforeFailure =
            readFile(fs::path(created.directoryPath) / "world.meta");
        WorldBackupOptions interrupted;
        interrupted.faultPoint =
            WorldBackupFaultPoint::AfterFirstRestorePublish;
        const WorldManagementResult failedRestore = service.restoreBackup(
            created.worldId, backupInfo.id, interrupted);
        suite.check(
            "K4/interrupted-backup-restore-rolls-back-active-world",
            changedAgain && !failedRestore.succeeded() &&
                failedRestore.status ==
                    WorldManagementStatus::StorageFailure &&
                readFile(fs::path(created.directoryPath) / "world.meta") ==
                    beforeFailure,
            failedRestore.message);
    }

    {
        GameApplicationFlow flow;
        const bool validPath =
            flow.state() == GameApplicationState::MainMenu &&
            flow.showWorldList() && flow.beginLoading("world-flow") &&
            flow.completeLoading(true) && flow.pause() && flow.resume() &&
            flow.returnToMainMenu() &&
            flow.state() == GameApplicationState::MainMenu &&
            flow.activeWorldId().empty();
        suite.check("K4/menu-world-loading-play-pause-state-path",
                    validPath);
        suite.check(
            "K4/invalid-state-transitions-are-rejected",
            !flow.pause() && !flow.resume() &&
                !flow.beginLoading(std::string()) && flow.showWorldList() &&
                !flow.showWorldList() && flow.beginLoading("world-failure") &&
                flow.completeLoading(false) &&
                flow.state() == GameApplicationState::WorldList &&
                flow.activeWorldId().empty());
    }

    // Keep a known successful three-entry scan inside the bounded timing
    // window after the expanded selected-world fixtures above.
    {
        TemporaryDirectory timing("timing-three-worlds");
        timing.create();
        writeMetadata(timing.path(), "one",
                      fixtureWith("timing-one", "Timing One",
                                  "1786838100", "1786838200"));
        writeMetadata(timing.path(), "two",
                      fixtureWith("timing-two", "Timing Two",
                                  "1786838200", "1786838300"));
        writeMetadata(timing.path(), "three",
                      fixtureWith("timing-three", "Timing Three",
                                  "1786838300", "1786838400"));
        (void)WorldCatalogue::enumerate(timing.path().string());
    }
    {
        TemporaryDirectory timing("timing-invalid-world");
        timing.create();
        fs::create_directories(timing.path() / "missing-meta");
        try {
            (void)WorldCatalogue::enumerate(timing.path().string());
        }
        catch (const WorldCatalogueError &) {
        }
    }

    const std::vector<RuntimeOperationRecord> timingRecords =
        operationTimings.snapshot();
    const auto successfulTiming = std::find_if(
        timingRecords.begin(), timingRecords.end(),
        [](const RuntimeOperationRecord &record) {
            return record.kind == RuntimeOperationKind::Catalogue &&
                   record.complete && record.success &&
                   record.catalogueEntries == 3 &&
                   record.totalMilliseconds >=
                       record.mainThreadMaxStallMilliseconds;
        });
    const auto failedTiming = std::find_if(
        timingRecords.begin(), timingRecords.end(),
        [](const RuntimeOperationRecord &record) {
            return record.kind == RuntimeOperationKind::Catalogue &&
                   record.complete && !record.success &&
                   record.totalMilliseconds >=
                       record.mainThreadMaxStallMilliseconds;
        });
    suite.check("Q2/catalogue-success-emits-complete-timing",
                successfulTiming != timingRecords.end());
    suite.check("Q2/catalogue-failure-emits-complete-timing",
                failedTiming != timingRecords.end());
    operationTimings.reset(false);

    return suite.finish();
}
