#include "../../src/HelloMine3D/World/Exploration/ExplorationAtlas.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* label)
{
    if (!condition) {
        throw std::runtime_error(label);
    }
}
}

int main()
{
    try {
        using Result = ExplorationAtlas::ObserveResult;
        ExplorationAtlas atlas;
        require(!atlas.surfaceAt(0, 0).has_value(), "unknown stays unknown");
        require(atlas.observe({0, 0, 70, BlockId::Grass, false}) ==
                    Result::Unchanged && atlas.tileCount() == 0,
                "unobserved sample cannot reveal terrain");
        require(atlas.observe({-1, -1, 83, BlockId::ForestFloor, true}) ==
                    Result::Updated &&
                    atlas.surfaceAt(-4, -4)->height == 83 &&
                    !atlas.surfaceAt(-5, -5).has_value() &&
                    !atlas.surfaceAt(0, 0).has_value(),
                "negative coordinates keep floor-aligned cells");
        require(atlas.observe({0, 0, 0, BlockId::Air, true}) ==
                    Result::Updated &&
                    atlas.surfaceAt(3, 3)->material == BlockId::Air &&
                    !atlas.surfaceAt(4, 4).has_value(),
                "a genuinely observed empty column differs from unknown");
        require(atlas.observe({-4, -4, 83, BlockId::ForestFloor, true}) ==
                    Result::Unchanged && atlas.knownCellCount() == 2,
                "repeat observation cannot duplicate a cell");
        require(atlas.observe({-1, -1, 84, BlockId::MossStone, true}) ==
                    Result::Updated && atlas.knownCellCount() == 2 &&
                    atlas.surfaceAt(-4, -4)->material == BlockId::MossStone,
                "a changed resident surface refreshes an explored cell");
        require(atlas.observe({8, 8, -1, BlockId::Grass, true}) ==
                    Result::Invalid &&
                    atlas.observe({8, 8, 256, BlockId::Grass, true}) ==
                    Result::Invalid &&
                    atlas.observe({8, 8, 10, BlockId::NUM_TYPES, true}) ==
                    Result::Invalid && atlas.knownCellCount() == 2,
                "invalid heights and materials cannot enter the archive");

        atlas.clear();
        require(atlas.observe({std::numeric_limits<int>::min(),
                               std::numeric_limits<int>::max(),
                               255, BlockId::Snow, true}) ==
                    Result::Updated &&
                    atlas.surfaceAt(std::numeric_limits<int>::min(),
                                    std::numeric_limits<int>::max())
                            ->material == BlockId::Snow,
                "extreme world coordinates cannot overflow page addressing");
        atlas.clear();
        for (std::size_t tile = 0; tile < ExplorationAtlas::MaxTiles; ++tile) {
            require(atlas.observe({static_cast<int>(tile * 128), 0,
                                   80, BlockId::Grass, true}) == Result::Updated,
                    "every admitted tile is retained");
        }
        require(atlas.tileCount() == ExplorationAtlas::MaxTiles &&
                    atlas.residentBytesUpperBound() <
                        ExplorationAtlas::MaxResidentBytes &&
                    atlas.knownCellCount() == ExplorationAtlas::MaxTiles &&
                    atlas.observe({static_cast<int>(
                                       ExplorationAtlas::MaxTiles * 128),
                                   0, 80, BlockId::Grass, true}) ==
                        Result::Full &&
                    !atlas.surfaceAt(static_cast<int>(
                                         ExplorationAtlas::MaxTiles * 128),
                                     0).has_value() &&
                    atlas.observe({0, 0, 81, BlockId::Stone, true}) ==
                        Result::Updated &&
                    atlas.surfaceAt(0, 0)->height == 81,
                "full archive refuses new pages without evicting history");
        atlas.clear();
        require(atlas.tileCount() == 0 && atlas.knownCellCount() == 0 &&
                    !atlas.surfaceAt(0, 0).has_value(),
                "world detach clears all exploration memory");
        std::cout << "[EXPLORATION_ATLAS] checks=8 status=PASS\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "[EXPLORATION_ATLAS] FAIL " << error.what() << '\n';
        return 1;
    }
}
