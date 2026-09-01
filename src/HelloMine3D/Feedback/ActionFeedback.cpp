#include "ActionFeedback.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "../Actor/ActorTypes.h"
#include "../Sandbox/Events/BlockEvents.h"
#include "../Sandbox/Events/EntityEvents.h"

namespace
{
    float visualScale(GameplayFeedbackIntensity intensity) noexcept
    {
        switch (intensity)
        {
            case GameplayFeedbackIntensity::Off: return 0.f;
            case GameplayFeedbackIntensity::Reduced: return 0.45f;
            case GameplayFeedbackIntensity::Full: return 1.f;
        }
        return 0.f;
    }
}

ActionFeedbackTimeline::~ActionFeedbackTimeline()
{
    detach();
}

void ActionFeedbackTimeline::attach(SandboxEventBus &eventBus)
{
    if (m_eventBus == &eventBus)
    {
        return;
    }
    detach();
    m_eventBus = &eventBus;
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::BlockBreak, [this](const SandboxEvent &event)
        {
            const auto &block = static_cast<const BlockBreakEvent &>(event);
            activate(ActionFeedbackKind::BlockBreak, 0.34f, 0.7f, 0.f);
            emit(Material::toMaterial(block.blockId).id,
                 FullBlockParticleCount);
        }, SandboxEventSubscriptionOptions::observer(
            "ActionFeedbackTimeline")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::BlockPlace, [this](const SandboxEvent &event)
        {
            const auto &block = static_cast<const BlockPlaceEvent &>(event);
            activate(ActionFeedbackKind::BlockPlace, 0.28f, 0.45f, 0.f);
            emit(Material::toMaterial(block.blockId).id,
                 FullBlockParticleCount / 2);
        }, SandboxEventSubscriptionOptions::observer(
            "ActionFeedbackTimeline")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::EntityDamage, [this](const SandboxEvent &event)
        {
            const auto &damage = static_cast<const EntityDamageEvent &>(event);
            if (damage.sourceId == DefaultPlayerActorId &&
                damage.id != DefaultPlayerActorId)
            {
                activate(ActionFeedbackKind::AttackHit, 0.24f, 1.f,
                         MaxHitStopSeconds);
            }
            else if (damage.id == DefaultPlayerActorId)
            {
                activate(ActionFeedbackKind::PlayerHurt, 0.38f, 0.6f,
                         MaxHitStopSeconds * 0.5f);
            }
        }, SandboxEventSubscriptionOptions::observer(
            "ActionFeedbackTimeline")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::CombatGuard, [this](const SandboxEvent &event)
        {
            const auto &guard = static_cast<const CombatGuardEvent &>(event);
            if (guard.defenderId == DefaultPlayerActorId)
            {
                activate(ActionFeedbackKind::Guard, 0.32f, 0.8f,
                         MaxHitStopSeconds * 0.5f);
            }
        }, SandboxEventSubscriptionOptions::observer(
            "ActionFeedbackTimeline")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::ItemPickup, [this](const SandboxEvent &event)
        {
            const auto &pickup = static_cast<const ItemPickupEvent &>(event);
            if (pickup.pickerId == DefaultPlayerActorId)
            {
                activate(ActionFeedbackKind::ItemPickup, 0.30f, 0.35f, 0.f);
                emit(pickup.materialId, 3);
            }
        }, SandboxEventSubscriptionOptions::observer(
            "ActionFeedbackTimeline")));
}

void ActionFeedbackTimeline::detach() noexcept
{
    if (m_eventBus != nullptr)
    {
        for (const SandboxEventBus::SubscriptionId id : m_subscriptions)
        {
            m_eventBus->unsubscribe(id);
        }
    }
    m_subscriptions.clear();
    m_eventBus = nullptr;
}

void ActionFeedbackTimeline::setIntensity(
    GameplayFeedbackIntensity intensity) noexcept
{
    m_intensity = intensity;
    if (m_intensity == GameplayFeedbackIntensity::Off)
    {
        m_particles.clear();
        m_recoil = 0.f;
        m_hitStopSeconds = 0.f;
    }
}

GameplayFeedbackIntensity ActionFeedbackTimeline::intensity() const noexcept
{
    return m_intensity;
}

void ActionFeedbackTimeline::update(float deltaSeconds) noexcept
{
    const float elapsed = std::clamp(deltaSeconds, 0.f, 0.25f);
    m_secondsRemaining = std::max(0.f, m_secondsRemaining - elapsed);
    m_hitStopSeconds = std::max(0.f, m_hitStopSeconds - elapsed);
    if (m_secondsRemaining <= 0.f)
    {
        m_kind = ActionFeedbackKind::None;
        m_recoil = 0.f;
    }
    for (ParticleState &particle : m_particles)
    {
        particle.age += elapsed;
        particle.velocityY += 90.f * elapsed;
        particle.offsetX += particle.velocityX * elapsed;
        particle.offsetY += particle.velocityY * elapsed;
    }
    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(),
                       [](const ParticleState &particle)
                       {
                           return particle.age >= particle.lifetime;
                       }),
        m_particles.end());
}

void ActionFeedbackTimeline::submitAttackMiss() noexcept
{
    if (m_kind == ActionFeedbackKind::AttackMiss &&
        m_secondsRemaining > 0.08f)
    {
        return;
    }
    activate(ActionFeedbackKind::AttackMiss, 0.18f, 0.5f, 0.f);
}

ActionFeedbackSnapshot ActionFeedbackTimeline::snapshot() const
{
    ActionFeedbackSnapshot result;
    result.kind = m_kind;
    result.epoch = m_epoch;
    result.secondsRemaining = m_secondsRemaining;
    const float scale = visualScale(m_intensity);
    result.recoil = m_recoil * scale;
    result.hitStopSeconds = m_hitStopSeconds * scale;
    result.particles.reserve(m_particles.size());
    for (const ParticleState &particle : m_particles)
    {
        const float lifetime = std::max(0.001f, particle.lifetime);
        const float alpha = std::clamp(
            1.f - particle.age / lifetime, 0.f, 1.f);
        result.particles.push_back({
            particle.materialId, particle.offsetX, particle.offsetY,
            particle.size, alpha});
    }
    return result;
}

float ActionFeedbackTimeline::audioGainVariant(std::uint64_t epoch) noexcept
{
    const std::uint64_t mixed = epoch * 1103515245ull + 12345ull;
    const int step = static_cast<int>((mixed >> 16u) % 9u) - 4;
    return 1.f + static_cast<float>(step) * 0.015f;
}

void ActionFeedbackTimeline::activate(ActionFeedbackKind kind,
                                      float duration, float recoil,
                                      float hitStopSeconds) noexcept
{
    ++m_epoch;
    m_kind = kind;
    m_secondsRemaining = std::clamp(duration, 0.f, 0.6f);
    m_recoil = std::clamp(recoil, 0.f, 1.f);
    m_hitStopSeconds = std::clamp(
        hitStopSeconds, 0.f, MaxHitStopSeconds);
}

void ActionFeedbackTimeline::emit(Material::ID materialId,
                                  std::size_t count) noexcept
{
    if (m_intensity == GameplayFeedbackIntensity::Off ||
        materialId == Material::ID::Nothing)
    {
        return;
    }
    if (m_intensity == GameplayFeedbackIntensity::Reduced)
    {
        count = (count + 1) / 2;
    }
    count = std::min(count, MaxParticles);
    while (m_particles.size() + count > MaxParticles)
    {
        m_particles.erase(m_particles.begin());
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::uint64_t seed =
            (m_epoch + 1u) * 2654435761ull + index * 2246822519ull;
        const float direction = index % 2 == 0 ? -1.f : 1.f;
        ParticleState particle;
        particle.materialId = materialId;
        particle.offsetX = static_cast<float>(
            static_cast<int>((seed >> 4u) % 15u) - 7);
        particle.offsetY = static_cast<float>(
            static_cast<int>((seed >> 9u) % 11u) - 5);
        particle.velocityX = direction *
            (28.f + static_cast<float>((seed >> 14u) % 38u));
        particle.velocityY =
            -62.f - static_cast<float>((seed >> 20u) % 48u);
        particle.lifetime = 0.32f +
            static_cast<float>((seed >> 26u) % 18u) * 0.01f;
        particle.lifetime = std::min(
            particle.lifetime, MaxParticleLifetimeSeconds);
        particle.size = 5.f + static_cast<float>((seed >> 32u) % 5u);
        m_particles.push_back(particle);
    }
}

const char *gameplayFeedbackIntensityName(
    GameplayFeedbackIntensity intensity) noexcept
{
    switch (intensity)
    {
        case GameplayFeedbackIntensity::Off: return "Off";
        case GameplayFeedbackIntensity::Reduced: return "Reduced";
        case GameplayFeedbackIntensity::Full: return "Full";
    }
    return "Unknown";
}

const char *gameplayFeedbackIntensityToken(
    GameplayFeedbackIntensity intensity) noexcept
{
    switch (intensity)
    {
        case GameplayFeedbackIntensity::Off: return "off";
        case GameplayFeedbackIntensity::Reduced: return "reduced";
        case GameplayFeedbackIntensity::Full: return "full";
    }
    return "unknown";
}

bool tryParseGameplayFeedbackIntensity(
    const std::string &token,
    GameplayFeedbackIntensity &intensity) noexcept
{
    if (token == "off")
    {
        intensity = GameplayFeedbackIntensity::Off;
        return true;
    }
    if (token == "reduced")
    {
        intensity = GameplayFeedbackIntensity::Reduced;
        return true;
    }
    if (token == "full")
    {
        intensity = GameplayFeedbackIntensity::Full;
        return true;
    }
    return false;
}
