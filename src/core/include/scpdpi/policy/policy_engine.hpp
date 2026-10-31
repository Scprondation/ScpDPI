#pragma once
#include "scpdpi/classifier/traffic_classifier.hpp"
#include <string>
#include <vector>

namespace scpdpi {
enum class PolicyAction { Bypass, Inspect, ApplyProfile };
struct PolicyRule {
    std::string id;
    std::string name;
    int priority{};
    TrafficRoute traffic{TrafficRoute::Unknown};
    PolicyAction action{PolicyAction::Bypass};
    bool active{true};
    // Required when action is ApplyProfile. Kept trailing to preserve existing aggregate initializers.
    std::string target_profile;
};
struct PolicyDecision {
    PolicyAction action{PolicyAction::Bypass};
    std::string rule_id;
    std::string profile;
};
class PolicyEngine {
public:
    [[nodiscard]] static std::vector<PolicyRule> rules_for_profile(const std::string& profile);
    [[nodiscard]] PolicyDecision evaluate(TrafficRoute traffic, const std::vector<PolicyRule>& rules,
                                          const std::string& profile) const;
};
[[nodiscard]] const char* to_string(PolicyAction action) noexcept;
} // namespace scpdpi
