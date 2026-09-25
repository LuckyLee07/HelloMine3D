#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

// A bounded, centre-first sweep of the current render region. This class owns
// only copied observations: it cannot generate terrain or turn unknown into land.
template<class Sample, int MaximumSide = 129, int MinimumStep = 4>
struct MapSurfaceRegion
{
    static constexpr int MaxSide = MaximumSide;
    static constexpr int BatchSize = 195;
    struct Query { int x, z, cell; };
    std::vector<Sample> cells;
    int centerX = 0, centerZ = 0, count = 0, step = 4, cursor = 0;
    std::uint64_t revision = 0;
    std::vector<unsigned char> observed;
    std::vector<int> pending;
    std::size_t pendingCursor = 0;

    std::size_t pendingCount() const { return pending.size() - pendingCursor; }


    bool configure(int x, int z, int renderDistance)
    {
        // One chunk margin covers the difference between player and chunk centre.
        const int radius = (std::clamp(renderDistance, 1, 64) + 1) * 16;
        int spacing = MinimumStep;
        while (2 * ((radius + spacing - 1) / spacing) + 1 > MaxSide) spacing *= 2;
        const int side = 2 * ((radius + spacing - 1) / spacing) + 1;
        const auto align = [spacing](int v) {
            const auto q = v / spacing;
            return (q - (v % spacing < 0)) * spacing;
        };
        x = align(x); z = align(z);
        if (count == side && step == spacing && centerX == x && centerZ == z) return false;
        auto oldCells = std::move(cells);
        auto oldObserved = std::move(observed);
        const int oldCount = count, oldStep = step, oldX = centerX, oldZ = centerZ;
        centerX = x; centerZ = z; count = side; step = spacing;
        cursor = 0; cells.assign(count * count, {}); observed.assign(cells.size(), 0);
        // Match world coordinates, including resolution changes. No interpolation
        // or index reuse across a move can invent a new observation.
        for (int iz = 0; iz < count; ++iz) for (int ix = 0; ix < count; ++ix) {
            const auto dx = std::int64_t(centerX) + (ix-count/2)*step - oldX;
            const auto dz = std::int64_t(centerZ) + (iz-count/2)*step - oldZ;
            if (oldCount == 0 || dx % oldStep || dz % oldStep) continue;
            const auto ox = dx / oldStep + oldCount / 2, oz = dz / oldStep + oldCount / 2;
            if (ox < 0 || oz < 0 || ox >= oldCount || oz >= oldCount) continue;
            cells[iz*count+ix] = oldCells[std::size_t(oz*oldCount+ox)];
            observed[iz*count+ix] = oldObserved[std::size_t(oz*oldCount+ox)];
        }
        pending.clear(); pendingCursor = 0;
        const auto offset = [](int i) { return ((i + 1) / 2) * (i % 2 ? 1 : -1); };
        for (int i = 0; i < count*count; ++i) {
            const int index = (offset(i/count)+count/2)*count + offset(i%count)+count/2;
            if (!observed[index]) pending.push_back(index);
        }
        ++revision;
        return true;
    }

    std::vector<Query> nextBatch() const
    {
        std::vector<Query> result;
        if (cells.empty()) return result;
        result.reserve(BatchSize);
        const bool filling = pendingCount() != 0;
        const int limit = std::min(BatchSize, filling ? int(pendingCount()) : int(cells.size()));
        const auto offset = [](int i) { return ((i + 1) / 2) * (i % 2 ? 1 : -1); };
        for (int i = 0; i < limit; ++i) {
            const int ordinal = (cursor + i) % int(cells.size());
            const int index = filling ? pending[pendingCursor+i] :
                (offset(ordinal/count)+count/2)*count + offset(ordinal%count)+count/2;
            const int dx = index % count - count/2, dz = index / count - count/2;
            const std::int64_t x = std::int64_t(centerX) + dx * step;
            const std::int64_t z = std::int64_t(centerZ) + dz * step;
            if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max() ||
                z < std::numeric_limits<int>::min() || z > std::numeric_limits<int>::max()) continue;
            result.push_back({int(x), int(z), index});
        }
        return result;
    }

    bool accept(const std::vector<Query>& batch, const std::vector<Sample>& samples)
    {
        if (samples.size() != batch.size() || cells.empty()) return false;
        // Reject a response from an earlier centre rather than placing it at a
        // reused cell index. Lock contention leaves both queues untouched.
        for (const auto& q : batch) {
            if (q.cell < 0 || q.cell >= int(cells.size()) ||
                std::int64_t(centerX)+(q.cell%count-count/2)*step != q.x ||
                std::int64_t(centerZ)+(q.cell/count-count/2)*step != q.z) return false;
        }
        bool changed = false;
        for (std::size_t i = 0; i < batch.size(); ++i) {
            auto& old = cells[batch[i].cell];
            const auto& sample = samples[i];
            changed |= old.known != sample.known || old.height != sample.height || old.material != sample.material;
            old = sample; // Eviction clears the live view; the archive keeps history.
            observed[batch[i].cell] = 1;
        }
        if (pendingCount()) pendingCursor += std::min(std::size_t(BatchSize), pendingCount());
        else cursor = (cursor + std::min(BatchSize, int(cells.size()))) % int(cells.size());
        if (changed) ++revision;
        return changed;
    }

    int cellAt(double x, double z) const
    {
        if (!std::isfinite(x) || !std::isfinite(z) || cells.empty()) return -1;
        const double ix = std::floor((x - centerX) / step + count / 2 + .5);
        const double iz = std::floor((z - centerZ) / step + count / 2 + .5);
        if (ix < 0 || iz < 0 || ix >= count || iz >= count) return -1;
        const int index = int(iz) * count + int(ix);
        return cells[index].known ? index : -1;
    }

    std::optional<float> surfaceHeight(float x, float z) const
    {
        if (!std::isfinite(x) || !std::isfinite(z) || cells.empty()) return {};
        const double dx = (double(x) - centerX) / step;
        const double dz = (double(z) - centerZ) / step;
        if (std::abs(dx) > count / 2 || std::abs(dz) > count / 2) return {};
        const int ix = int(std::round(dx)) + count / 2, iz = int(std::round(dz)) + count / 2;
        const auto& sample = cells[iz * count + ix];
        if (!sample.known) return {};
        return float(sample.height);
    }
};
