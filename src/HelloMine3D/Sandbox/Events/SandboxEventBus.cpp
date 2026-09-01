#include "SandboxEventBus.h"

#include <algorithm>
#include <utility>

SandboxEventSubscriptionOptions SandboxEventSubscriptionOptions::observer(
    std::string ownerName)
{
    SandboxEventSubscriptionOptions result;
    result.owner = std::move(ownerName);
    return result;
}

SandboxEventSubscriptionOptions
SandboxEventSubscriptionOptions::domainMutation(
    std::string ownerName,
    SandboxEventRepublishPolicy republishPolicy)
{
    SandboxEventSubscriptionOptions result;
    result.owner = std::move(ownerName);
    result.effect = SandboxEventHandlerEffect::DomainMutation;
    result.republish = republishPolicy;
    return result;
}

SandboxEventBus::SubscriptionId
SandboxEventBus::subscribe(SandboxEventType type, Handler handler,
                           SandboxEventSubscriptionOptions options)
{
    const auto id = m_nextSubscriptionId++;
    m_subscriptions[type].push_back(
        {id, std::move(handler), std::move(options)});
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

SandboxEventDispatchResult SandboxEventBus::publish(
    const SandboxEvent &event)
{
    ++m_attemptedPublications;
    SandboxEventDispatchResult result;
    if (m_dispatchDepth > 0 &&
        m_activeRepublishPolicy != SandboxEventRepublishPolicy::Bounded) {
        ++m_rejectedPublications;
        result.rejection =
            SandboxEventDispatchRejection::HandlerRepublishForbidden;
        return result;
    }
    if (m_dispatchDepth >= MaxDispatchDepth) {
        ++m_rejectedPublications;
        result.rejection = SandboxEventDispatchRejection::DepthLimit;
        return result;
    }

    ++m_acceptedPublications;
    ++m_dispatchDepth;
    m_maxObservedDispatchDepth = std::max(
        m_maxObservedDispatchDepth, m_dispatchDepth);

    struct DispatchDepthGuard {
        explicit DispatchDepthGuard(std::size_t &depthValue)
            : depth(depthValue)
        {
        }
        ~DispatchDepthGuard() { --depth; }
        std::size_t &depth;
    } depthGuard(m_dispatchDepth);

    const auto iter = m_subscriptions.find(event.type);
    if (iter == m_subscriptions.end()) {
        return result;
    }

    // Membership is fixed for this publication. Changes made by a handler are
    // visible to later or nested publications, never halfway through this one.
    const std::vector<Subscription> dispatchSubscriptions = iter->second;
    for (const Subscription &subscription : dispatchSubscriptions) {
        if (!subscription.handler) {
            continue;
        }
        if (event.category == SandboxEventCategory::Diagnostic &&
            subscription.options.effect ==
                SandboxEventHandlerEffect::DomainMutation) {
            ++result.rejectedHandlers;
            ++m_rejectedDiagnosticHandlers;
            continue;
        }

        struct RepublishPolicyGuard {
            RepublishPolicyGuard(
                SandboxEventRepublishPolicy &activeValue,
                SandboxEventRepublishPolicy nextValue)
                : active(activeValue)
                , previous(activeValue)
            {
                active = nextValue;
            }
            ~RepublishPolicyGuard() { active = previous; }
            SandboxEventRepublishPolicy &active;
            SandboxEventRepublishPolicy previous;
        } policyGuard(m_activeRepublishPolicy,
                      subscription.options.republish);

        subscription.handler(event);
        ++result.deliveredHandlers;
    }
    return result;
}

SandboxEventBusDebugSnapshot SandboxEventBus::debugSnapshot() const noexcept
{
    SandboxEventBusDebugSnapshot result;
    for (const auto &entry : m_subscriptions) {
        result.subscriptionCount += entry.second.size();
        result.domainMutationSubscriptionCount +=
            static_cast<std::size_t>(std::count_if(
                entry.second.begin(), entry.second.end(),
                [](const Subscription &subscription) {
                    return subscription.options.effect ==
                           SandboxEventHandlerEffect::DomainMutation;
                }));
    }
    result.attemptedPublications = m_attemptedPublications;
    result.acceptedPublications = m_acceptedPublications;
    result.rejectedPublications = m_rejectedPublications;
    result.rejectedDiagnosticHandlers = m_rejectedDiagnosticHandlers;
    result.currentDispatchDepth = m_dispatchDepth;
    result.maxObservedDispatchDepth = m_maxObservedDispatchDepth;
    return result;
}
