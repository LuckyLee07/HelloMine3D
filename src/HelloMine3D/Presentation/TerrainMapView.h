#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

// Read-only orthographic relief. Unknown samples never acquire geometry.
// All coordinates are relative to the sampled region, preserving block scale.
namespace TerrainMapView
{
    struct Point { float x = 0, y = 0, depth = 0; };
    struct View
    {
        float yaw = -.65f, pitch = .85f, zoom = 1.f, panX = 0, panY = 0;
        void constrain() noexcept
        {
            if (!std::isfinite(yaw)) yaw = -.65f;
            yaw = std::remainder(yaw, 6.28318530718f);
            pitch = std::clamp(std::isfinite(pitch) ? pitch : .85f, .35f, 1.35f);
            zoom = std::clamp(std::isfinite(zoom) ? zoom : 1.f, .5f, 4.f);
            panX = std::clamp(std::isfinite(panX) ? panX : 0.f, -1.f, 1.f);
            panY = std::clamp(std::isfinite(panY) ? panY : 0.f, -1.f, 1.f);
        }
    };
    struct Gesture
    {
        int button = -1;
        float originX = 0, originY = 0;
        View start;
        bool dragged = false;
        void begin(const View& view, float x, float y, int mouseButton) noexcept
        {
            start = view; originX = x; originY = y; button = mouseButton; dragged = false;
        }
        void update(View& view, float x, float y, float width, float height, bool down) noexcept
        {
            if (button < 0) return;
            const float dx = x - originX, dy = y - originY;
            dragged = dragged || dx * dx + dy * dy > 36.f;
            if (dragged)
            {
                if (button == 0) { view.yaw = start.yaw + dx * .008f; view.pitch = start.pitch + dy * .006f; }
                else { view.panX = start.panX + dx / std::max(1.f,width); view.panY = start.panY + dy / std::max(1.f,height); }
                view.constrain();
            }
            // Use the release position even when no held frame was rendered.
            if (!down) button = -1;
        }
    };
    struct Projection
    {
        float cy, sy, cp, sp;
        explicit Projection(const View& input)
        {
            auto view = input; view.constrain();
            cy = std::cos(view.yaw); sy = std::sin(view.yaw);
            cp = std::cos(view.pitch); sp = std::sin(view.pitch);
        }
        Point operator()(float x, float height, float z) const noexcept
        {
            const float rx = cy * x - sy * z;
            const float rz = sy * x + cy * z;
            return {rx, sp * rz - cp * height, cp * rz + sp * height};
        }
    };
    struct Face
    {
        std::array<Point, 4> points{};
        float depth = 0, shade = 1;
        int cell = 0;
        bool top = true;
    };
    struct Bounds
    {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        bool valid = false;
        void include(const Point& p) noexcept
        {
            if (!valid) { minX = maxX = p.x; minY = maxY = p.y; valid = true; }
            else { minX = std::min(minX,p.x); maxX = std::max(maxX,p.x);
                   minY = std::min(minY,p.y); maxY = std::max(maxY,p.y); }
        }
        float fittedScale(float width, float height, float minimumSpan) const noexcept
        {
            return .90f * std::min(std::max(1.f,width) / std::max(minimumSpan,maxX-minX),
                                  std::max(1.f,height) / std::max(minimumSpan,maxY-minY));
        }
    };
    // Display zoom is independent of the atlas' immutable 4 m recording cells.
    // Coarser query levels keep the existing 65x65 result budget when zooming out.
    struct OverviewScale
    {
        static constexpr int CellCount = 129;
        int step = 4;
        float zoom = 1.f;
        // Fit the actual local region, independent of how many rows have
        // arrived. Zoom and query resolution must be solved together.
        void fitRegion(float width, float height, float metres) noexcept
        {
            const float perPixel = std::max(4.f, metres) /
                (std::max(1.f, std::min(width, height)) * .82f);
            step = 4;
            zoom = step * float(CellCount) / (std::max({1.f,width,height}) * perPixel);
            while (zoom < 1.f && step < 64) { step *= 2; zoom *= 2.f; }
            zoom = std::clamp(zoom, 1.f, 16.f);
        }
        void change(float factor) noexcept
        {
            if (!std::isfinite(factor) || factor <= 0.f) return;
            zoom = std::clamp(zoom * factor, .0625f, 16.f);
            while (zoom < 1.f && step < 64) { zoom *= 2.f; step *= 2; }
            while (zoom >= 2.f && step > 4) { zoom *= .5f; step /= 2; }
            zoom = std::clamp(zoom,1.f,16.f);
        }
        float cellPixels(float width, float height) const noexcept
        { return std::max({1.f,width,height}) / float(CellCount) * zoom; }
    };
    inline bool contains(const Face& face, float x, float y) noexcept
    {
        bool positive = false, negative = false;
        float area = 0.f;
        for (std::size_t i = 0; i < 4; ++i)
        {
            const auto& a = face.points[i]; const auto& b = face.points[(i + 1) % 4];
            const float cross = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
            positive |= cross > .0001f; negative |= cross < -.0001f;
            area += a.x * b.y - b.x * a.y;
        }
        return std::abs(area) > .0001f && !(positive && negative);
    }
    template<class Cells>
    void build(const Cells& cells, int count, int step, float baseHeight,
               const View& view, std::vector<Face>& faces)
    {
        faces.clear();
        if (count < 1 || count > 129 || step < 1 || step > 32 ||
            cells.size() != static_cast<std::size_t>(count * count) || !std::isfinite(baseHeight)) return;
        faces.reserve(cells.size() * 3);
        const Projection project(view);
        const auto add = [&](int cell, bool top, float shade, const std::array<Point, 4>& points) {
            Face face; face.cell = cell; face.top = top; face.shade = shade; face.points = points;
            for (const auto& point : points) face.depth += point.depth * .25f;
            faces.push_back(face);
        };
        for (int z = 0; z < count; ++z)
        for (int x = 0; x < count; ++x)
        {
            const int index = z * count + x;
            if (!cells[index].known) continue;
            const float x0 = (x - count / 2 - .5f) * step, x1 = x0 + step;
            const float z0 = (z - count / 2 - .5f) * step, z1 = z0 + step;
            const float h = float(cells[index].height) - baseHeight;
            add(index, true, 1.f, {{project(x0,h,z0), project(x1,h,z0), project(x1,h,z1), project(x0,h,z1)}});
            // Only exposed walls between two observations; unknown edges are
            // gaps, not invented cliffs. Back-facing walls are discarded.
            const int dx[] = {-1, 1, 0, 0}, dz[] = {0, 0, -1, 1};
            for (int side = 0; side < 4; ++side)
            {
                if (dx[side] * project.sy + dz[side] * project.cy <= .0001f) continue;
                const int nx = x + dx[side], nz = z + dz[side];
                if (nx < 0 || nz < 0 || nx >= count || nz >= count) continue;
                const auto& other = cells[nz * count + nx];
                if (!other.known || other.height >= cells[index].height) continue;
                const float low = float(other.height) - baseHeight;
                const float ax = side == 1 ? x1 : x0, az = side == 3 ? z1 : z0;
                const float bx = side < 2 ? ax : x1, bz = side < 2 ? z1 : az;
                add(index, false, side < 2 ? .70f : .82f,
                    {{project(ax,h,az), project(bx,h,bz), project(bx,low,bz), project(ax,low,az)}});
            }
        }
        std::stable_sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) {
            return a.depth < b.depth;
        });
    }
    inline int pick(const std::vector<Face>& faces, float x, float y) noexcept
    {
        if (!std::isfinite(x) || !std::isfinite(y)) return -1;
        for (auto it = faces.rbegin(); it != faces.rend(); ++it)
            if (contains(*it, x, y)) return it->cell;
        return -1;
    }
}
