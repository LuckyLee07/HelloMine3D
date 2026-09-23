#ifndef EXPLORATIONMARKERS_H_INCLUDED
#define EXPLORATIONMARKERS_H_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Player-authored locations. The World facade checks exploration truth before
// creating or moving a marker; this value layer never predicts terrain.
class ExplorationMarkers {
  public:
    static constexpr std::size_t Capacity = 64;
    static constexpr std::size_t MaxNameBytes = 64;
    static constexpr std::size_t MaxNameCodepoints = 24;

    enum class Kind : std::uint8_t { Note = 0, Home = 1 };
    enum class Result { Created, Changed, Removed, Unchanged, Full,
                        Missing, Invalid };

    struct Marker {
        std::uint32_t id = 0;
        int worldX = 0;
        int worldZ = 0;
        std::string name;
        Kind kind = Kind::Note;
    };

    static bool validName(const std::string& value) noexcept
    {
        if (value.empty() || value.size() > MaxNameBytes ||
            value.front() == ' ' || value.back() == ' ') {
            return false;
        }
        std::size_t codepoints = 0;
        for (std::size_t index = 0; index < value.size();) {
            const unsigned char first =
                static_cast<unsigned char>(value[index]);
            std::size_t length = 0;
            std::uint32_t scalar = 0;
            std::uint32_t minimum = 0;
            if (first < 0x80u) {
                length = 1;
                scalar = first;
            }
            else if (first >= 0xc2u && first <= 0xdfu) {
                length = 2;
                scalar = first & 0x1fu;
                minimum = 0x80u;
            }
            else if (first >= 0xe0u && first <= 0xefu) {
                length = 3;
                scalar = first & 0x0fu;
                minimum = 0x800u;
            }
            else if (first >= 0xf0u && first <= 0xf4u) {
                length = 4;
                scalar = first & 0x07u;
                minimum = 0x10000u;
            }
            else {
                return false;
            }
            if (index + length > value.size()) {
                return false;
            }
            for (std::size_t offset = 1; offset < length; ++offset) {
                const unsigned char next =
                    static_cast<unsigned char>(value[index + offset]);
                if ((next & 0xc0u) != 0x80u) {
                    return false;
                }
                scalar = (scalar << 6) | (next & 0x3fu);
            }
            if (scalar < minimum || scalar > 0x10ffffu ||
                (scalar >= 0xd800u && scalar <= 0xdfffu) ||
                scalar < 0x20u ||
                (scalar >= 0x7fu && scalar <= 0x9fu) ||
                ++codepoints > MaxNameCodepoints) {
                return false;
            }
            index += length;
        }
        return true;
    }

    Result create(int worldX, int worldZ, std::string name,
                  Kind kind = Kind::Note, std::uint32_t* createdId = nullptr)
    {
        if (!validName(name) || !validKind(kind) || m_nextId == 0) {
            return Result::Invalid;
        }
        if (m_markers.size() == Capacity) {
            return Result::Full;
        }
        const std::uint32_t id = m_nextId;
        m_markers.push_back({id, worldX, worldZ, std::move(name),
                             Kind::Note});
        ++m_nextId;
        if (kind == Kind::Home) {
            clearHome();
            m_markers.back().kind = Kind::Home;
        }
        if (createdId != nullptr) {
            *createdId = id;
        }
        return Result::Created;
    }

    Result rename(std::uint32_t id, std::string name)
    {
        if (!validName(name)) {
            return Result::Invalid;
        }
        Marker* marker = findMutable(id);
        if (marker == nullptr) {
            return Result::Missing;
        }
        if (marker->name == name) {
            return Result::Unchanged;
        }
        marker->name = std::move(name);
        return Result::Changed;
    }

    Result move(std::uint32_t id, int worldX, int worldZ)
    {
        Marker* marker = findMutable(id);
        if (marker == nullptr) {
            return Result::Missing;
        }
        if (marker->worldX == worldX && marker->worldZ == worldZ) {
            return Result::Unchanged;
        }
        marker->worldX = worldX;
        marker->worldZ = worldZ;
        return Result::Changed;
    }

    Result setHome(std::uint32_t id)
    {
        Marker* marker = findMutable(id);
        if (marker == nullptr) {
            return Result::Missing;
        }
        if (marker->kind == Kind::Home) {
            return Result::Unchanged;
        }
        clearHome();
        marker->kind = Kind::Home;
        return Result::Changed;
    }

    Result track(std::uint32_t id)
    {
        if (id != 0 && find(id) == nullptr) {
            return Result::Missing;
        }
        if (m_trackedId == id) {
            return Result::Unchanged;
        }
        m_trackedId = id;
        return Result::Changed;
    }

    Result erase(std::uint32_t id)
    {
        const auto found = std::find_if(m_markers.begin(), m_markers.end(),
            [id](const Marker& marker) { return marker.id == id; });
        if (found == m_markers.end()) {
            return Result::Missing;
        }
        m_markers.erase(found);
        if (m_trackedId == id) {
            m_trackedId = 0;
        }
        return Result::Removed;
    }

    const Marker* find(std::uint32_t id) const noexcept
    {
        const auto found = std::find_if(m_markers.begin(), m_markers.end(),
            [id](const Marker& marker) { return marker.id == id; });
        return found == m_markers.end() ? nullptr : &*found;
    }
    const Marker* home() const noexcept
    {
        const auto found = std::find_if(m_markers.begin(), m_markers.end(),
            [](const Marker& marker) { return marker.kind == Kind::Home; });
        return found == m_markers.end() ? nullptr : &*found;
    }
    const Marker* tracked() const noexcept { return find(m_trackedId); }
    std::uint32_t trackedId() const noexcept { return m_trackedId; }
    const std::vector<Marker>& all() const noexcept { return m_markers; }
    std::size_t size() const noexcept { return m_markers.size(); }
    void clear() noexcept
    {
        m_markers.clear();
        m_nextId = 1;
        m_trackedId = 0;
    }

  private:
    friend class ExplorationMapStore;

    static bool validKind(Kind kind) noexcept
    {
        return kind == Kind::Note || kind == Kind::Home;
    }
    Marker* findMutable(std::uint32_t id) noexcept
    {
        return const_cast<Marker*>(
            static_cast<const ExplorationMarkers*>(this)->find(id));
    }
    void clearHome() noexcept
    {
        for (Marker& marker : m_markers) {
            if (marker.kind == Kind::Home) {
                marker.kind = Kind::Note;
            }
        }
    }

    std::vector<Marker> m_markers;
    std::uint32_t m_nextId = 1;
    std::uint32_t m_trackedId = 0;
};

#endif // EXPLORATIONMARKERS_H_INCLUDED
