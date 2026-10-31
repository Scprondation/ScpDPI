#include "scpdpi/policy/policy_engine.hpp"
#include <algorithm>

namespace scpdpi {
std::vector<PolicyRule> PolicyEngine::rules_for_profile(const std::string& profile) {
    if (profile == "vpn-safe") {
        return {{"unknown-bypass", "Bypass unknown routes", 1000, TrafficRoute::Unknown, PolicyAction::Bypass, true},
                {"vpn-inspect", "Observe VPN route", 100, TrafficRoute::Vpn, PolicyAction::Inspect, true},
                {"direct-bypass", "Leave direct traffic unchanged", 50, TrafficRoute::Direct, PolicyAction::Bypass, true}};
    }
    if (profile == "diagnostic") {
        return {{"unknown-bypass", "Bypass unknown routes", 1000, TrafficRoute::Unknown, PolicyAction::Bypass, true},
                {"vpn-inspect", "Observe VPN route", 100, TrafficRoute::Vpn, PolicyAction::Inspect, true},
                {"direct-inspect", "Observe direct route", 50, TrafficRoute::Direct, PolicyAction::Inspect, true}};
    }
    if (profile == "custom") {
        return {{"unknown-bypass", "Bypass unknown routes", 1000, TrafficRoute::Unknown, PolicyAction::Bypass, true}};
    }
    if (profile == "default") {
        return {{"unknown-bypass", "Bypass unknown routes", 1000, TrafficRoute::Unknown, PolicyAction::Bypass, true},
                {"vpn-inspect", "Observe VPN route", 100, TrafficRoute::Vpn, PolicyAction::Inspect, true},
                {"direct-inspect", "Observe direct route", 50, TrafficRoute::Direct, PolicyAction::Inspect, true}};
    }
    return {{"unknown-bypass", "Bypass unknown routes", 1000, TrafficRoute::Unknown, PolicyAction::Bypass, true}};
}

PolicyDecision PolicyEngine::evaluate(TrafficRoute traffic, const std::vector<PolicyRule>& rules,
                                      const std::string& profile) const {
    const PolicyRule* chosen = nullptr;
    for (const auto& rule : rules) {
        if (!rule.active || rule.traffic != traffic) continue;
        if (chosen == nullptr || rule.priority > chosen->priority) chosen = &rule;
    }
    if (chosen != nullptr) {
        if (chosen->action == PolicyAction::ApplyProfile) {
            if (chosen->target_profile.empty())
                return {PolicyAction::Bypass, "invalid-apply-profile-target", profile};
            return {PolicyAction::ApplyProfile, chosen->id, chosen->target_profile};
        }
        return {chosen->action, chosen->id, profile};
    }
    return {PolicyAction::Bypass, "safe-default-bypass", profile};
}

const char* to_string(PolicyAction action) noexcept {
    switch (action) {
    case PolicyAction::Bypass: return "bypass";
    case PolicyAction::Inspect: return "inspect";
    case PolicyAction::ApplyProfile: return "apply-profile";
    }
    return "bypass";
}
} // namespace scpdpi
