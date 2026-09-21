// Refine coast candidates from the frozen v19 production survey. This calls
// the same pure column planner consumed by ClassicOverWorldGenerator v18/v19;
// actual block and visual evidence is recorded separately in the client.
#include "World/Generation/Ecology/AdventureEcologyPlanner.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <vector>

namespace {
struct Candidate {
    int seed, x, z, halfX, halfZ;
};

struct Component {
    std::vector<bool> member;
    std::size_t cells = 0;
    bool touchesBoundary = false;
};

Component dryComponent(const std::vector<TerrainFoundation::Column>& grid,
                       int width, int depth, int start)
{
    Component result;
    result.member.resize(grid.size());
    if (grid.at(start).height < 64) return result;
    std::queue<int> pending;
    pending.push(start);
    result.member[start] = true;
    while (!pending.empty()) {
        const int index = pending.front();
        pending.pop();
        ++result.cells;
        const int x = index % width, z = index / width;
        result.touchesBoundary = result.touchesBoundary ||
            x == 0 || z == 0 || x == width - 1 || z == depth - 1;
        // Include diagonal dry contacts conservatively: a narrow or diagonal
        // land bridge must not be reported as open water around an island.
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx, nz = z + dz;
                if ((dx == 0 && dz == 0) || nx < 0 || nz < 0 ||
                    nx >= width || nz >= depth) continue;
                const int next = nz * width + nx;
                if (!result.member[next] && grid[next].height >= 64) {
                    result.member[next] = true;
                    pending.push(next);
                }
            }
        }
    }
    return result;
}
} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc != 2) throw std::runtime_error("Usage: survey_adventure_islands <new-output-directory>");
        const std::filesystem::path output(argv[1]);
        if (std::filesystem::exists(output))
            throw std::runtime_error("Refusing to replace previous evidence");
        std::filesystem::create_directories(output);
        // Both candidates were selected before images, using every internal
        // dry component >= 5 samples on the original 32 m survey grid and a
        // marine boundary. The larger component is first, deterministic ties.
        const std::array<Candidate, 2> candidates{{
            {0, -32, 1824, 256, 192},
            {20260807, 1792, 1280, 128, 128}
        }};
        std::ofstream summary(output / "summary.csv");
        if (!summary) throw std::runtime_error("Cannot write summary");
        summary << "seed,queries,dry_cells,x_min,x_max,z_min,z_max,max_height,"
                   "wet_boundary_cells,marine_boundary_cells,closed,bridge_negative_rejected\n";
        for (const auto& candidate : candidates) {
            const int width = candidate.halfX * 2 + 1;
            const int depth = candidate.halfZ * 2 + 1;
            const int minX = candidate.x - candidate.halfX;
            const int minZ = candidate.z - candidate.halfZ;
            AdventureEcologyPlanner planner(candidate.seed);
            std::vector<TerrainFoundation::Column> grid(width * depth);
            for (int z = 0; z < depth; ++z)
                for (int x = 0; x < width; ++x)
                    grid[z * width + x] = planner.sample(minX + x, minZ + z).column;
            const int start = candidate.halfZ * width + candidate.halfX;
            const auto component = dryComponent(grid, width, depth, start);
            if (component.cells == 0 || component.touchesBoundary)
                throw std::runtime_error("Candidate is wet or connects to the survey boundary");
            auto injected = grid;
            for (int x = 0; x <= candidate.halfX; ++x)
                injected[candidate.halfZ * width + x].height = 64;
            if (!dryComponent(injected, width, depth, start).touchesBoundary)
                throw std::runtime_error("A sea-level land bridge was missed by the analysis");
            std::vector<bool> boundary(grid.size());
            int xMin = width, xMax = 0, zMin = depth, zMax = 0, maxHeight = 0;
            for (int z = 1; z < depth - 1; ++z) {
                for (int x = 1; x < width - 1; ++x) {
                    const int index = z * width + x;
                    if (!component.member[index]) continue;
                    xMin = std::min(xMin, x); xMax = std::max(xMax, x);
                    zMin = std::min(zMin, z); zMax = std::max(zMax, z);
                    maxHeight = std::max(maxHeight, grid[index].height);
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int next = (z + dz) * width + x + dx;
                            if (!component.member[next]) boundary[next] = true;
                        }
                }
            }
            std::size_t wet = 0, marine = 0;
            std::ofstream columns(output / ("seed-" + std::to_string(candidate.seed) + "-grid.csv"));
            if (!columns) throw std::runtime_error("Cannot write columns");
            columns << "x,z,height,biome,surface,island,boundary\n";
            for (int z = 0; z < depth; ++z) {
                for (int x = 0; x < width; ++x) {
                    const int index = z * width + x;
                    const auto& column = grid[index];
                    if (boundary[index]) {
                        if (column.height >= 64)
                            throw std::runtime_error("Dry cell escaped the connected component");
                        ++wet;
                        if (column.biome == TerrainBiome::Ocean) ++marine;
                    }
                    columns << minX + x << ',' << minZ + z << ',' << column.height << ','
                            << static_cast<int>(column.biome) << ',' << static_cast<int>(column.surface)
                            << ',' << component.member[index] << ',' << boundary[index] << '\n';
                }
            }
            if (wet == 0 || marine != wet)
                throw std::runtime_error("The refined island boundary is not entirely marine");
            summary << candidate.seed << ',' << grid.size() << ',' << component.cells << ','
                    << minX + xMin << ',' << minX + xMax << ',' << minZ + zMin << ','
                    << minZ + zMax << ',' << maxHeight << ',' << wet << ',' << marine << ",1,1\n";
            std::cout << "[ISLAND_SURVEY] seed=" << candidate.seed << " dry_cells="
                      << component.cells << " marine_boundary=" << marine
                      << " closed=1 bridge_negative_rejected=1\n";
        }
        std::cout << "[ISLAND_SURVEY] status=PASS evidence=PURE_COLUMN_TOPOLOGY\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[ISLAND_SURVEY] status=FAIL error=" << error.what() << '\n';
        return 1;
    }
}
