#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <utility>

// Recent, actual observations in this world session. Evicting a fine tile does
// not delete the world's persistent 4m archive. Unknown is never extrapolated.
// Capacity also covers an entire maximum 257x257 fine view (at most 33x33
// tiles) before retaining nearby history; the live view must not evict itself.
template<class Sample, std::size_t Capacity = 2048>
class SurfaceMapHistory
{
    using Key = std::pair<int,int>;
    struct Tile {
        std::array<Sample,64> cells{};
        typename std::list<Key>::iterator recent;
    };
    std::map<Key,Tile> tiles;
    std::list<Key> recent;
    static int tileCoord(int v) { return v/16 - (v%16<0); }
public:
    struct Observation { int x, z; Sample surface; };
    SurfaceMapHistory() = default;
    SurfaceMapHistory(const SurfaceMapHistory&) = delete;
    SurfaceMapHistory& operator=(const SurfaceMapHistory&) = delete;
    SurfaceMapHistory(SurfaceMapHistory&&) = default;
    SurfaceMapHistory& operator=(SurfaceMapHistory&&) = default;

    std::size_t tileCount() const { return tiles.size(); }
    void observe(int x, int z, const Sample& sample)
    {
        if (!sample.known || x%2 || z%2 || Capacity==0) return;
        const Key key{tileCoord(x),tileCoord(z)};
        auto found = tiles.find(key);
        if (found == tiles.end()) {
            if (tiles.size() == Capacity) { tiles.erase(recent.back()); recent.pop_back(); }
            recent.push_front(key);
            found = tiles.emplace(key,Tile{}).first;
            found->second.recent = recent.begin();
        } else recent.splice(recent.begin(),recent,found->second.recent);
        const int ix = (std::int64_t(x)-std::int64_t(key.first)*16)/2;
        const int iz = (std::int64_t(z)-std::int64_t(key.second)*16)/2;
        found->second.cells[iz*8+ix] = sample;
    }

    std::optional<Observation> at(double x, double z) const
    {
        if (!std::isfinite(x) || !std::isfinite(z)) return {};
        x = std::floor((x+1)/2)*2; z = std::floor((z+1)/2)*2;
        if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max() ||
            z < std::numeric_limits<int>::min() || z > std::numeric_limits<int>::max()) return {};
        const Key key{tileCoord(int(x)),tileCoord(int(z))};
        const auto tile = tiles.find(key);
        if (tile == tiles.end()) return {};
        const int ix = (std::int64_t(x)-std::int64_t(key.first)*16)/2;
        const int iz = (std::int64_t(z)-std::int64_t(key.second)*16)/2;
        const auto& sample = tile->second.cells[iz*8+ix];
        return sample.known ? std::optional<Observation>({int(x),int(z),sample}) : std::nullopt;
    }

    template<class Visitor>
    void visit(double left, double top, double right, double bottom, Visitor visitor) const
    {
        const Sample unknown{};
        for (const auto& entry : tiles) {
            const auto x0=std::int64_t(entry.first.first)*16, z0=std::int64_t(entry.first.second)*16;
            if (x0+15<left || x0-1>right || z0+15<top || z0-1>bottom) continue;
            const auto west=tiles.find({entry.first.first-1,entry.first.second});
            const auto north=tiles.find({entry.first.first,entry.first.second-1});
            const auto& cells=entry.second.cells;
            for (int z=0;z<8;++z) for (int x=0;x<8;++x) {
                const auto& cell=cells[z*8+x];
                if (!cell.known) continue;
                const auto wx=x0+x*2, wz=z0+z*2;
                if (wx+1<left || wx-1>right || wz+1<top || wz-1>bottom) continue;
                const auto& w=x ? cells[z*8+x-1] : west!=tiles.end() ? west->second.cells[z*8+7] : unknown;
                const auto& n=z ? cells[(z-1)*8+x] : north!=tiles.end() ? north->second.cells[56+x] : unknown;
                visitor(int(wx),int(wz),cell,w,n);
            }
        }
    }
};
