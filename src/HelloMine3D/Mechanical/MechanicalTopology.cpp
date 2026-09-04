#include "MechanicalTopology.h"

#include <algorithm>
#include <array>
#include <deque>
#include <sstream>

namespace
{
const std::array<MechanicalNodeId, 6> NeighborOffsets = {{
    {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
    {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
}};

const std::array<MechanicalNodeId, 3> CanonicalEdgeOffsets = {{
    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}
}};

MechanicalNodeId add(const MechanicalNodeId &left,
                     const MechanicalNodeId &right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}
} // namespace

bool operator==(const MechanicalNodeId &left,
                const MechanicalNodeId &right) noexcept
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool operator!=(const MechanicalNodeId &left,
                const MechanicalNodeId &right) noexcept
{
    return !(left == right);
}

bool operator<(const MechanicalNodeId &left,
               const MechanicalNodeId &right) noexcept
{
    if (left.x != right.x) return left.x < right.x;
    if (left.y != right.y) return left.y < right.y;
    return left.z < right.z;
}

bool operator==(const MechanicalNetworkId &left,
                const MechanicalNetworkId &right) noexcept
{
    return left.anchor == right.anchor;
}

std::string mechanicalNetworkIdString(const MechanicalNetworkId &id)
{
    std::ostringstream stream;
    stream << id.anchor.x << ':' << id.anchor.y << ':' << id.anchor.z;
    return stream.str();
}

bool MechanicalTopology::ChunkLess::operator()(
    const VectorXZ &left, const VectorXZ &right) const noexcept
{
    return left.x < right.x || (left.x == right.x && left.z < right.z);
}

bool MechanicalTopology::setNode(const VectorXZ &chunk,
                                 const MechanicalNodeId &node,
                                 bool present)
{
    auto found = m_chunkNodes.find(chunk);
    const bool existed = found != m_chunkNodes.end() &&
                         found->second.find(node) != found->second.end();
    if (existed == present) {
        return false;
    }

    if (present) {
        m_chunkNodes[chunk].insert(node);
        m_nodes.insert(node);
    }
    else {
        found->second.erase(node);
        if (found->second.empty()) {
            m_chunkNodes.erase(found);
        }
        m_nodes.erase(node);
    }
    rebuild();
    return true;
}

bool MechanicalTopology::replaceChunkNodes(
    const VectorXZ &chunk, const std::vector<MechanicalNodeId> &nodes)
{
    const std::set<MechanicalNodeId> replacement(nodes.begin(), nodes.end());
    const auto found = m_chunkNodes.find(chunk);
    const std::set<MechanicalNodeId> empty;
    const std::set<MechanicalNodeId> &current =
        found == m_chunkNodes.end() ? empty : found->second;
    if (current == replacement) {
        return false;
    }

    for (const MechanicalNodeId &node : current) {
        m_nodes.erase(node);
    }
    if (replacement.empty()) {
        if (found != m_chunkNodes.end()) {
            m_chunkNodes.erase(found);
        }
    }
    else {
        m_chunkNodes[chunk] = replacement;
        m_nodes.insert(replacement.begin(), replacement.end());
    }
    rebuild();
    return true;
}

bool MechanicalTopology::removeChunk(const VectorXZ &chunk)
{
    return replaceChunkNodes(chunk, {});
}

void MechanicalTopology::rebuild()
{
    m_dirty = true;
    ++m_revision;
    ++m_rebuildCount;
    m_components.clear();
    m_nodeComponents.clear();
    m_lastVisitedNodes = 0;

    std::set<MechanicalNodeId> visited;
    for (const MechanicalNodeId &start : m_nodes) {
        if (visited.find(start) != visited.end()) {
            continue;
        }

        MechanicalComponent component;
        std::deque<MechanicalNodeId> pending;
        pending.push_back(start);
        visited.insert(start);
        while (!pending.empty()) {
            const MechanicalNodeId node = pending.front();
            pending.pop_front();
            component.nodes.push_back(node);
            ++m_lastVisitedNodes;
            for (const MechanicalNodeId &offset : NeighborOffsets) {
                const MechanicalNodeId neighbor = add(node, offset);
                if (m_nodes.find(neighbor) != m_nodes.end() &&
                    visited.insert(neighbor).second) {
                    pending.push_back(neighbor);
                }
            }
        }

        std::sort(component.nodes.begin(), component.nodes.end());
        component.id.anchor = component.nodes.front();
        for (const MechanicalNodeId &node : component.nodes) {
            for (const MechanicalNodeId &offset : CanonicalEdgeOffsets) {
                const MechanicalNodeId neighbor = add(node, offset);
                if (m_nodes.find(neighbor) != m_nodes.end()) {
                    component.connections.push_back({node, neighbor});
                }
            }
        }

        const std::size_t componentIndex = m_components.size();
        for (const MechanicalNodeId &node : component.nodes) {
            m_nodeComponents[node] = componentIndex;
        }
        m_components.push_back(std::move(component));
    }
    m_dirty = false;
}

std::optional<MechanicalNodeSnapshot>
MechanicalTopology::nodeSnapshot(const MechanicalNodeId &node) const
{
    const auto found = m_nodeComponents.find(node);
    if (found == m_nodeComponents.end() ||
        found->second >= m_components.size()) {
        return std::nullopt;
    }

    const MechanicalComponent &component = m_components[found->second];
    std::uint8_t faceMask = 0;
    for (std::size_t index = 0; index < NeighborOffsets.size(); ++index) {
        if (m_nodes.find(add(node, NeighborOffsets[index])) != m_nodes.end()) {
            faceMask = static_cast<std::uint8_t>(
                faceMask | (static_cast<std::uint8_t>(1u) << index));
        }
    }
    return MechanicalNodeSnapshot{
        node, component.id, component.nodes.size(),
        component.connections.size(), faceMask, m_revision};
}

MechanicalTopologyDebugSnapshot
MechanicalTopology::debugSnapshot() const noexcept
{
    std::size_t connectionCount = 0;
    for (const MechanicalComponent &component : m_components) {
        connectionCount += component.connections.size();
    }
    return {m_nodes.size(), connectionCount, m_components.size(),
            m_revision, m_rebuildCount, m_lastVisitedNodes, m_dirty};
}
