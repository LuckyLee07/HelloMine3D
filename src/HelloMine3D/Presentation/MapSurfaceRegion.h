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
        centerX = x; centerZ = z; count = side; step = spacing;
        cursor = 0; cells.assign(count * count, {}); ++revision;
        return true;
    }

    std::vector<Query> nextBatch() const
    {
        std::vector<Query> result;
        if (cells.empty()) return result;
        result.reserve(BatchSize);
        const auto offset = [](int i) { return ((i + 1) / 2) * (i % 2 ? 1 : -1); };
        for (int i = 0; i < std::min(BatchSize, int(cells.size())); ++i) {
            const int ordinal = (cursor + i) % int(cells.size());
            const int dx = offset(ordinal % count), dz = offset(ordinal / count);
            const std::int64_t x = std::int64_t(centerX) + dx * step;
            const std::int64_t z = std::int64_t(centerZ) + dz * step;
            if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max() ||
                z < std::numeric_limits<int>::min() || z > std::numeric_limits<int>::max()) continue;
            result.push_back({int(x), int(z), (dz + count / 2) * count + dx + count / 2});
        }
        return result;
    }

    bool accept(const std::vector<Query>& batch, const std::vector<Sample>& samples)
    {
        if (samples.size() != batch.size() || cells.empty()) return false;
        bool changed = false;
        for (std::size_t i = 0; i < batch.size(); ++i) {
            auto& old = cells[batch[i].cell];
            const auto& sample = samples[i];
            changed |= old.known != sample.known || old.height != sample.height || old.material != sample.material;
            old = sample; // Eviction clears the live view; the archive keeps history.
        }
        cursor = (cursor + std::min(BatchSize, int(cells.size()))) % int(cells.size());
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
