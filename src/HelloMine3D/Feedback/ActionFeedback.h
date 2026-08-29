#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../Actor/CombatTypes.h"
#include "../Item/Material.h"
#include "../Sandbox/Events/SandboxEventBus.h"

enum class GameplayFeedbackIntensity
{
    Off,
    Reduced,
    Full
};

enum class ActionFeedbackKind
{
    None,
    BlockBreak,
    BlockPlace,
    AttackMiss,
    AttackHit,
    Guard,
    PlayerHurt,
    ItemPickup
};

struct ActionFeedbackParticle
{
    Material::ID materialId = Material::ID::Nothing;
    float offsetX = 0.f;
    float offsetY = 0.f;
    float size = 0.f;
    float alpha = 0.f;
};

struct ActionFeedbackSnapshot
{
    ActionFeedbackKind kind = ActionFeedbackKind::None;
    std::uint64_t epoch = 0;
    float secondsRemaining = 0.f;
    float recoil = 0.f;
    float hitStopSeconds = 0.f;
    std::vector<ActionFeedbackParticle> particles;
};

/// Renderer-independent, deterministic timeline for short player action
/// feedback. Gameplay outcomes are already committed before this timeline is
/// notified; it can never delay simulation or change a hit result.
class ActionFeedbackTimeline
{
  public:
    static constexpr std::size_t MaxParticles = 48;
    static constexpr std::size_t FullBlockParticleCount = 8;
    static constexpr float MaxParticleLifetimeSeconds = 0.55f;
    static constexpr float MaxHitStopSeconds = 0.045f;

    ~ActionFeedbackTimeline();

    void attach(SandboxEventBus &eventBus);
    void detach() noexcept;
    void setIntensity(GameplayFeedbackIntensity intensity) noexcept;
    GameplayFeedbackIntensity intensity() const noexcept;
    void update(float deltaSeconds) noexcept;
    void submitAttackMiss() noexcept;
    ActionFeedbackSnapshot snapshot() const;

    static float audioGainVariant(std::uint64_t epoch) noexcept;

  private:
    struct ParticleState
    {
        Material::ID materialId = Material::ID::Nothing;
        float offsetX = 0.f;
        float offsetY = 0.f;
        float velocityX = 0.f;
        float velocityY = 0.f;
        float age = 0.f;
        float lifetime = 0.f;
        float size = 0.f;
    };

    void activate(ActionFeedbackKind kind, float duration,
                  float recoil, float hitStopSeconds) noexcept;
    void emit(Material::ID materialId, std::size_t count) noexcept;

    SandboxEventBus *m_eventBus = nullptr;
    std::vector<SandboxEventBus::SubscriptionId> m_subscriptions;
    std::vector<ParticleState> m_particles;
    GameplayFeedbackIntensity m_intensity =
        GameplayFeedbackIntensity::Full;
    ActionFeedbackKind m_kind = ActionFeedbackKind::None;
    std::uint64_t m_epoch = 0;
    float m_secondsRemaining = 0.f;
    float m_recoil = 0.f;
    float m_hitStopSeconds = 0.f;
};

const char *gameplayFeedbackIntensityName(
    GameplayFeedbackIntensity intensity) noexcept;
const char *gameplayFeedbackIntensityToken(
    GameplayFeedbackIntensity intensity) noexcept;
bool tryParseGameplayFeedbackIntensity(
    const std::string &token,
    GameplayFeedbackIntensity &intensity) noexcept;
