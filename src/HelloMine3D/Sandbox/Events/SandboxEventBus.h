#ifndef SANDBOXEVENTBUS_H_INCLUDED
#define SANDBOXEVENTBUS_H_INCLUDED

#include <cstddef>
#include <functional>
#include <string>
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

enum class SandboxEventCategory {
    Domain,
    Diagnostic,
};

struct SandboxEvent {
    explicit SandboxEvent(
        SandboxEventType eventType,
        SandboxEventCategory eventCategory = SandboxEventCategory::Domain)
        : type(eventType)
        , category(eventCategory)
    {
    }

    virtual ~SandboxEvent() = default;

    const SandboxEventType type;
    const SandboxEventCategory category;
};

enum class SandboxEventHandlerEffect {
    ObserveOnly,
    DomainMutation,
};

enum class SandboxEventRepublishPolicy {
    Forbidden,
    Bounded,
};

struct SandboxEventSubscriptionOptions {
    std::string owner = "UnspecifiedObserver";
    SandboxEventHandlerEffect effect =
        SandboxEventHandlerEffect::ObserveOnly;
    SandboxEventRepublishPolicy republish =
        SandboxEventRepublishPolicy::Forbidden;

    static SandboxEventSubscriptionOptions observer(std::string ownerName);
    static SandboxEventSubscriptionOptions domainMutation(
        std::string ownerName,
        SandboxEventRepublishPolicy republishPolicy =
            SandboxEventRepublishPolicy::Forbidden);
};

enum class SandboxEventDispatchRejection {
    None,
    HandlerRepublishForbidden,
    DepthLimit,
};

struct SandboxEventDispatchResult {
    std::size_t deliveredHandlers = 0;
    std::size_t rejectedHandlers = 0;
    SandboxEventDispatchRejection rejection =
        SandboxEventDispatchRejection::None;

    bool publicationAccepted() const noexcept
    {
        return rejection == SandboxEventDispatchRejection::None;
    }
};

struct SandboxEventBusDebugSnapshot {
    std::size_t subscriptionCount = 0;
    std::size_t domainMutationSubscriptionCount = 0;
    std::size_t attemptedPublications = 0;
    std::size_t acceptedPublications = 0;
    std::size_t rejectedPublications = 0;
    std::size_t rejectedDiagnosticHandlers = 0;
    std::size_t currentDispatchDepth = 0;
    std::size_t maxObservedDispatchDepth = 0;
};

class SandboxEventBus {
  public:
    using Handler = std::function<void(const SandboxEvent &)>;
    using SubscriptionId = unsigned;
    static constexpr std::size_t MaxDispatchDepth = 8;

    SubscriptionId subscribe(
        SandboxEventType type, Handler handler,
        SandboxEventSubscriptionOptions options = {});
    void unsubscribe(SubscriptionId id);
    SandboxEventDispatchResult publish(const SandboxEvent &event);
    SandboxEventBusDebugSnapshot debugSnapshot() const noexcept;

  private:
    struct Subscription {
        SubscriptionId id = 0;
        Handler handler;
        SandboxEventSubscriptionOptions options;
    };

    std::unordered_map<SandboxEventType, std::vector<Subscription>>
        m_subscriptions;
    SubscriptionId m_nextSubscriptionId = 1;
    std::size_t m_attemptedPublications = 0;
    std::size_t m_acceptedPublications = 0;
    std::size_t m_rejectedPublications = 0;
    std::size_t m_rejectedDiagnosticHandlers = 0;
    std::size_t m_dispatchDepth = 0;
    std::size_t m_maxObservedDispatchDepth = 0;
    SandboxEventRepublishPolicy m_activeRepublishPolicy =
        SandboxEventRepublishPolicy::Forbidden;
};

#endif // SANDBOXEVENTBUS_H_INCLUDED
