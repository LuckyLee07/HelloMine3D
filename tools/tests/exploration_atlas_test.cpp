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

        ExplorationAtlas overview;
        overview.observe({-1, -1, 82, BlockId::ForestFloor, true});
        overview.observe({0, 0, 70, BlockId::Grass, true});
        overview.observe({28, 28, 65, BlockId::Sand, true});
        overview.observe({60, 60, 64, BlockId::Water, true});
        const auto near = overview.overviewAt(0, 0, 3, 4);
        require(near.size() == 9 && near[0].known &&
                    near[0].surface.material == BlockId::ForestFloor &&
                    near[4].known &&
                    near[4].surface.material == BlockId::Grass &&
                    !near[8].known,
                "native-scale overview invented or missed observed cells");
        const auto far = overview.overviewAt(32, 32, 3, 64);
        require(far.size() == 9 && far[0].known && far[4].known &&
                    far[4].surface.material == BlockId::Sand &&
                    far[4].worldX == 28 && far[4].worldZ == 28 &&
                    !far[8].known,
                "zoomed-out overview dropped an off-centre visited route");
        overview.observe({124, 0, 75, BlockId::Snow, true});
        overview.observe({128, 0, 76, BlockId::Gravel, true});
        const auto seam = overview.overviewAt(160, 0, 3, 64);
        require(seam.size() == 9 && seam[3].known && seam[4].known &&
                    seam[3].surface.material == BlockId::Snow &&
                    seam[4].surface.material == BlockId::Gravel,
                "tile seam merged two distinct overview pixels");
        const auto detailed = overview.overviewAt(0, 0, 129, 4);
        require(detailed.size() == 129*129 && detailed[64*129+64].known &&
                    detailed[64*129+96].surface.height == 76,
                "detail overview must retain separate real 4m records across tile seams");
        require(overview.overviewAt(0, 0, 130, 4).empty() &&
                    overview.overviewAt(0, 0, 65, 128).empty(),
                "unbounded overview request was accepted");

        atlas.clear();
        require(atlas.observe({std::numeric_limits<int>::min(),
                               std::numeric_limits<int>::max(),
                               255, BlockId::Snow, true}) ==
                    Result::Updated &&
                    atlas.surfaceAt(std::numeric_limits<int>::min(),
                                    std::numeric_limits<int>::max())
                            ->material == BlockId::Snow,
                "extreme world coordinates cannot overflow page addressing");
        const auto extremeOverview = atlas.overviewAt(
            std::numeric_limits<int>::min(),
            static_cast<std::int64_t>(std::numeric_limits<int>::max()) - 3,
            3, 4);
        require(extremeOverview.size() == 9 && extremeOverview[4].known &&
                    extremeOverview[4].surface.material == BlockId::Snow,
                "extreme observed surface disappeared from overview");
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
        std::cout << "[EXPLORATION_ATLAS] checks=14 status=PASS\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "[EXPLORATION_ATLAS] FAIL " << error.what() << '\n';
        return 1;
    }
}
