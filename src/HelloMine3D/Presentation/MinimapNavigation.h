#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "../Maths/glm.h"

// Presentation memory, cleared with the active world. Only actual interactions
// may add an anchor: never call the procedural structure planner to fill it.
namespace MinimapNavigation
{
    enum class Kind { Container, Workbench, Waystone };
    struct Landmark {
        glm::ivec3 position{0};
        Kind kind = Kind::Container;
    };
    class Memory {
      public:
        static constexpr std::size_t Capacity = 32;
        void clear() { m_landmarks.clear(); }
        void observe(const glm::ivec3& position, Kind kind) {
            auto found = std::find_if(m_landmarks.begin(), m_landmarks.end(),
                [&](const Landmark& mark) { return mark.position == position; });
            if (found != m_landmarks.end()) {
                found->kind = kind;
                return;
            }
            if (m_landmarks.size() == Capacity) m_landmarks.erase(m_landmarks.begin());
            m_landmarks.push_back({position, kind});
        }
        const std::vector<Landmark>& landmarks() const { return m_landmarks; }
      private:
        std::vector<Landmark> m_landmarks;
    };
    inline int cellStep(int range) { return range == 64 ? 1 : range == 256 ? 4 : 2; }
    // Keep the player's arrow and nearby icons distinct, at most eight per map.
    class MarkerLayout {
      public:
        bool reserve(float x, float y, float radius) {
            if (m_count == m_positions.size() || x*x+y*y > (radius-8)*(radius-8) ||
                x*x+y*y < 14*14) return false;
            for (std::size_t i=0;i<m_count;++i) {
                const float dx=x-m_positions[i].x, dy=y-m_positions[i].y;
                if (dx*dx+dy*dy < 13*13) return false;
            }
            m_positions[m_count++] = {x,y};
            return true;
        }
      private:
        std::array<glm::vec2,8> m_positions{};
        std::size_t m_count = 0;
    };
}
