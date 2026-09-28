#ifndef HELLOMINE3D_ADVENTURE_WATER_PLANNER_H
#define HELLOMINE3D_ADVENTURE_WATER_PLANNER_H

#include "AdventureTerrainPlanner.h"
#include <array>

class AdventureWaterPlanner {
  public:
    struct Sample {
        TerrainFoundation::Column column;
        double riverInfluence = 0;
        double lakeInfluence = 0;
        AdventureTerrainPlanner::Sample base;
        double bankDeposit = 0; // v26 inner-bend bar, derived from actual channel geometry.
    };
    struct FlowNode {
        double x = 0, z = 0, height = 0, potential = 0;
        double downstreamX = 0, downstreamZ = 0, downstreamHeight = 0, downstreamPotential = 0;
        bool drains = false;
    };
    struct ChannelPath {
        std::array<std::array<double,2>,9> points{};
        bool valid=false;
    };
    static constexpr int CellSize = 512;
    explicit AdventureWaterPlanner(int seed, int version = AdventureWaterTerrainGenerationVersion) noexcept
        : m_seed(seed), m_base(seed), m_polished(version >= WaterbankPolishTerrainGenerationVersion) {}
    Sample sample(int worldX, int worldZ) const noexcept;
    FlowNode planNode(int cellX, int cellZ) const noexcept;
    ChannelPath channelPath(int cellX,int cellZ) const noexcept;

  private:
    struct Node { double x=0,z=0,height=0,potential=0,width=0; bool lakeSite=false,basin=false; };
    struct Point { double x=0,z=0; };
    struct Edge {
        std::array<Point,9> points{};
        double heightA=0,heightB=0,widthA=0,widthB=0;
        double minimumX=0,minimumZ=0,maximumX=0,maximumZ=0;
        double bend=0, phase=0;
        bool valid=false;
    };
    struct Lake { double x=0,z=0,radius=0; bool valid=false; };
    struct Tile { std::array<Edge,9> edges{}; std::array<Lake,9> lakes{}; };
    Node node(std::int64_t cellX,std::int64_t cellZ) const noexcept;
    Tile buildTile(std::int64_t cellX,std::int64_t cellZ) const noexcept;
    const Tile &tile(std::int64_t cellX,std::int64_t cellZ) const noexcept;
    double random(std::int64_t x,std::int64_t z,std::uint64_t salt) const noexcept;
    int m_seed=0;
    AdventureTerrainPlanner m_base;
    bool m_polished=false;
};

#endif
