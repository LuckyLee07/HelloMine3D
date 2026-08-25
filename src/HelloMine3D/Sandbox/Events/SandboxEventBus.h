#ifndef SANDBOXEVENTBUS_H_INCLUDED
#define SANDBOXEVENTBUS_H_INCLUDED

#include <functional>
#include <unordered_map>
#include <vector>

enum class SandboxEventType {
    BlockBreak,
    BlockPlace,
    BlockUse,
    BlockChanged,
    ChunkGenerated,
    ChunkLoaded,
    ChunkUnloaded,
    ChunkSaved,
    EntitySpawn,
    EntityDamage,
    EntityDeath,
    ItemPickup,
    PlayerSpawn,
    PlayerTeleport,
    PlayerInventoryChanged,
    CraftCompleted,
    SmeltCompleted,
    FoodConsumed,
    CombatWindup,
    CombatGuard,
    WaystoneActivated,
    VictoryRewardClaimed,
};

struct SandboxEvent {
    explicit SandboxEvent(SandboxEventType eventType)
        : type(eventType)
    {
    }

    virtual ~SandboxEvent() = default;

    SandboxEventType type;
};

class SandboxEventBus {
  public:
    using Handler = std::function<void(const SandboxEvent &)>;
    using SubscriptionId = unsigned;

    SubscriptionId subscribe(SandboxEventType type, Handler handler);
    void unsubscribe(SubscriptionId id);
    void publish(const SandboxEvent &event) const;

  private:
    struct Subscription {
        SubscriptionId id = 0;
        Handler handler;
    };

    std::unordered_map<SandboxEventType, std::vector<Subscription>>
        m_subscriptions;
    SubscriptionId m_nextSubscriptionId = 1;
};

#endif // SANDBOXEVENTBUS_H_INCLUDED
