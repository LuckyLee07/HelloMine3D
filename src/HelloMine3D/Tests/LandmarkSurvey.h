#ifndef HELLOMINE3D_LANDMARK_SURVEY_H
#define HELLOMINE3D_LANDMARK_SURVEY_H

#include "TerrainSurvey.h"

namespace LandmarkSurvey {
// Baselines include actual blocks and container payloads in every intersecting
// chunk, not just the planner's anchor. Kept separate from graphical evidence.
inline std::size_t write(World &world, const std::filesystem::path &directory,
                         int version)
{
    if (version < 2 || version > CurrentTerrainGenerationVersion ||
        std::filesystem::exists(directory)) {
        throw std::runtime_error("Landmark survey requires supported version and new directory");
    }
    std::filesystem::create_directories(directory);
    std::ofstream plans(directory / "plans.csv"), chunks(directory / "chunks.csv");
    plans.exceptions(std::ios::failbit | std::ios::badbit);
    chunks.exceptions(std::ios::failbit | std::ios::badbit);
    plans << "seed,version,type,cell_x,cell_z,x,y,z,candidate,selection_hash,loot_hash\n";
    chunks << "seed,version,chunk_x,chunk_z,block_hash\n";
    std::size_t count = 0;
    for (int seed : TerrainSurvey::Seeds) {
        ClassicOverWorldGenerator generator(seed, version);
        std::array<int, 3> found{};
        std::set<std::pair<int, int>> locations;
        for (int radius = 0; radius <= 32; ++radius) {
            for (int cx = -radius; cx <= radius; ++cx) {
                for (int cz = -radius; cz <= radius; ++cz) {
                    if (std::max(std::abs(cx), std::abs(cz)) != radius) { continue; }
                    for (int type = 0; type < 3; ++type) {
                        if (found[type] >= 2 || (version == 2 && type != 0)) { continue; }
                        const auto plan = generator.getStructurePlanForCell(
                            static_cast<StructureType>(type), cx, cz);
                        if (!plan.valid) { continue; }
                        ++found[type]; ++count;
                        plans << seed << ',' << version << ',' << type << ',' << cx << ',' << cz
                              << ',' << plan.anchor.x << ',' << plan.anchor.y << ',' << plan.anchor.z
                              << ',' << plan.selectedCandidate << ',' << plan.selectionHash << ','
                              << structureLootForPlan(plan, ExplorationRewards::CurrentVersion).selectionHash << '\n';
                        const auto &f = plan.footprint;
                        for (int x = WorldCoordinates::floorDiv(f.minimumX, CHUNK_SIZE);
                             x <= WorldCoordinates::floorDiv(f.maximumX, CHUNK_SIZE); ++x) {
                            for (int z = WorldCoordinates::floorDiv(f.minimumZ, CHUNK_SIZE);
                                 z <= WorldCoordinates::floorDiv(f.maximumZ, CHUNK_SIZE); ++z) {
                                locations.emplace(x, z);
                            }
                        }
                    }
                }
            }
            if (found[0] == 2 && (version == 2 || (found[1] == 2 && found[2] == 2))) { break; }
        }
        for (const auto &location : locations) {
            Chunk chunk(world, {location.first, location.second});
            generator.generateTerrainFor(chunk);
            chunks << seed << ',' << version << ',' << location.first << ',' << location.second
                   << ',' << TerrainSurvey::blockHash(chunk) << '\n';
        }
    }
    return count;
}
} // namespace LandmarkSurvey
#endif
