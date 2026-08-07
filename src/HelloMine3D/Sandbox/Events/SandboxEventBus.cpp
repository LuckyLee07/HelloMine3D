#include "SandboxEventBus.h"

#include <algorithm>
#include <utility>

SandboxEventBus::SubscriptionId
SandboxEventBus::subscribe(SandboxEventType type, Handler handler)
{
    const auto id = m_nextSubscriptionId++;
    m_subscriptions[type].push_back({id, std::move(handler)});
    return id;
}

void SandboxEventBus::unsubscribe(SubscriptionId id)
{
    for (auto &entry : m_subscriptions) {
        auto &subscriptions = entry.second;
        subscriptions.erase(
            std::remove_if(subscriptions.begin(), subscriptions.end(),
                           [id](const Subscription &subscription) {
                               return subscription.id == id;
                           }),
            subscriptions.end());
    }
}

void SandboxEventBus::publish(const SandboxEvent &event) const
{
    const auto iter = m_subscriptions.find(event.type);
    if (iter == m_subscriptions.end()) {
        return;
    }

    for (const auto &subscription : iter->second) {
        if (subscription.handler) {
            subscription.handler(event);
        }
    }
}
