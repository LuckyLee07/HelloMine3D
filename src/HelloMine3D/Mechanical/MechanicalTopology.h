#ifndef MECHANICALTOPOLOGY_H_INCLUDED
#define MECHANICALTOPOLOGY_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../Maths/Vector2XZ.h"
#include "../Maths/glm.h"

class BlockCapabilityAccess;
class World;

struct MechanicalNodeId
{
    int x = 0;
    int y = 0;
    int z = 0;
};

bool operator==(const MechanicalNodeId &left,
                const MechanicalNodeId &right) noexcept;
bool operator!=(const MechanicalNodeId &left,
                const MechanicalNodeId &right) noexcept;
bool operator<(const MechanicalNodeId &left,
               const MechanicalNodeId &right) noexcept;

struct MechanicalNetworkId
{
    MechanicalNodeId anchor;
};

bool operator==(const MechanicalNetworkId &left,
                const MechanicalNetworkId &right) noexcept;
std::string mechanicalNetworkIdString(const MechanicalNetworkId &id);

enum class MechanicalFace : std::uint8_t
{
    NegativeX = 0,
    PositiveX,
    NegativeY,
    PositiveY,
    NegativeZ,
    PositiveZ
};

struct MechanicalConnection
{
    MechanicalNodeId first;
    MechanicalNodeId second;
};

struct MechanicalComponent
{
    MechanicalNetworkId id;
    std::vector<MechanicalNodeId> nodes;
    std::vector<MechanicalConnection> connections;
};

struct MechanicalNodeSnapshot
{
    MechanicalNodeId nodeId;
    MechanicalNetworkId networkId;
    std::size_t nodeCount = 0;
    std::size_t connectionCount = 0;
    std::uint8_t connectedFaceMask = 0;
    std::uint64_t topologyRevision = 0;
};

struct MechanicalTopologyDebugSnapshot
{
    std::size_t nodes = 0;
    std::size_t connections = 0;
    std::size_t components = 0;
    std::uint64_t revision = 0;
    std::uint64_t rebuildCount = 0;
    std::size_t lastVisitedNodes = 0;
    bool dirty = false;
};

enum class MechanicalPortKind
{
    None,
    CrusherAllFaces
};

class MechanicalPort
{
  public:
    std::optional<MechanicalNodeSnapshot> view(World &world) const;

    const glm::ivec3 &position() const noexcept { return m_position; }
    MechanicalPortKind kind() const noexcept { return m_kind; }

  private:
    friend class BlockCapabilityAccess;
    MechanicalPort(const glm::ivec3 &position,
                   MechanicalPortKind kind) noexcept
        : m_position(position), m_kind(kind)
    {
    }

    glm::ivec3 m_position{0};
    MechanicalPortKind m_kind = MechanicalPortKind::None;
};

class MechanicalTopology
{
  public:
    bool setNode(const VectorXZ &chunk, const MechanicalNodeId &node,
                 bool present);
    bool replaceChunkNodes(const VectorXZ &chunk,
                           const std::vector<MechanicalNodeId> &nodes);
    bool removeChunk(const VectorXZ &chunk);

    std::optional<MechanicalNodeSnapshot>
    nodeSnapshot(const MechanicalNodeId &node) const;
    MechanicalTopologyDebugSnapshot debugSnapshot() const noexcept;

  private:
    struct ChunkLess
    {
        bool operator()(const VectorXZ &left,
                        const VectorXZ &right) const noexcept;
    };

    void rebuild();

    std::map<VectorXZ, std::set<MechanicalNodeId>, ChunkLess> m_chunkNodes;
    std::set<MechanicalNodeId> m_nodes;
    std::vector<MechanicalComponent> m_components;
    std::map<MechanicalNodeId, std::size_t> m_nodeComponents;
    std::uint64_t m_revision = 0;
    std::uint64_t m_rebuildCount = 0;
    std::size_t m_lastVisitedNodes = 0;
    bool m_dirty = false;
};

#endif // MECHANICALTOPOLOGY_H_INCLUDED
