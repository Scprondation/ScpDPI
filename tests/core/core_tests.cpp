#include "scpdpi/classifier/traffic_classifier.hpp"
#include "scpdpi/diagnostics/diagnostics.hpp"
#include "scpdpi/engine/core_engine.hpp"
#include "scpdpi/policy/policy_engine.hpp"
#include "scpdpi/vpn/detector.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <stdexcept>

namespace scpdpi {
namespace {
IpAddress ip(const char* text) {
    auto result = IpAddress::parse(text);
    if (!result) throw std::runtime_error(result.error().message);
    return result.value();
}
NetworkInterface physical() {
    NetworkInterface value; value.index = 1; value.name = "ethernet"; value.kind = "ethernet";
    value.is_up = true; value.is_physical = true; value.addresses.push_back(ip("192.0.2.10")); return value;
}
NetworkInterface tunnel() {
    NetworkInterface value; value.index = 2; value.name = "wg0"; value.description = "WireGuard";
    value.kind = "tunnel"; value.is_up = true; value.is_virtual = true; return value;
}
Route route(std::uint32_t index, const char* destination, std::uint8_t prefix, std::uint32_t metric = 10) {
    Route value; value.interface_index = index; value.destination = ip(destination); value.prefix_length = prefix;
    value.metric = metric; value.is_default = prefix == 0; return value;
}

class MockInterfaceProvider final : public IInterfaceProvider {
public:
    explicit MockInterfaceProvider(std::vector<NetworkInterface> data) : data_(std::move(data)) {}
    Result<std::vector<NetworkInterface>> enumerate() const override { return Result<std::vector<NetworkInterface>>::success(data_); }
private: std::vector<NetworkInterface> data_;
};
class MockRouteProvider final : public IRouteProvider {
public:
    explicit MockRouteProvider(std::vector<Route> data) : data_(std::move(data)) {}
    Result<std::vector<Route>> enumerate() const override { return Result<std::vector<Route>>::success(data_); }
private: std::vector<Route> data_;
};

TEST(TrafficClassifierTests, ClassifiesVpnRouteFromDetectedInterface) {
    const auto detected = VpnDetector{}.detect({tunnel()}, {route(2, "0.0.0.0", 0)});
    EXPECT_EQ(TrafficClassifier{}.classify({tunnel()}, {route(2, "0.0.0.0", 0)}, detected, ip("8.8.8.8")), TrafficRoute::Vpn);
}

TEST(TrafficClassifierTests, ClassifiesPhysicalDefaultRouteAsDirect) {
    EXPECT_EQ(TrafficClassifier{}.classify({physical()}, {route(1, "0.0.0.0", 0)}, {}, ip("1.1.1.1")), TrafficRoute::Direct);
}

TEST(TrafficClassifierTests, UnknownRouteRemainsUnknown) {
    EXPECT_EQ(TrafficClassifier{}.classify({physical()}, {}, {}, ip("203.0.113.8")), TrafficRoute::Unknown);
}

TEST(TrafficClassifierTests, UsesLongestIpv6Prefix) {
    auto specific = route(1, "2001:db8:1::", 48);
    EXPECT_EQ(TrafficClassifier{}.classify({physical()}, {route(1, "::", 0, 100), specific}, {}, ip("2001:db8:1::1234")), TrafficRoute::Direct);
    EXPECT_EQ(TrafficClassifier{}.classify({physical()}, {route(2, "::", 0, 100)}, {}, ip("2001:db8:ffff::1")), TrafficRoute::Unknown);
}

TEST(TrafficClassifierTests, AmbiguousEqualCostRoutesStayUnknown) {
    EXPECT_EQ(TrafficClassifier{}.classify({physical(), tunnel()},
        {route(1, "0.0.0.0", 0, 10), route(2, "0.0.0.0", 0, 10)},
        VpnDetector{}.detect({tunnel()}, {route(2, "0.0.0.0", 0, 10)}), ip("8.8.4.4")), TrafficRoute::Unknown);
}

TEST(VpnDetectorTests, UsesVirtualTypeAndRouteEvidenceWithoutClaimingProtocol) {
    const auto found = VpnDetector{}.detect({tunnel()}, {route(2, "0.0.0.0", 0)});
    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found.front().confidence, VpnInterface::Confidence::Confirmed);
    EXPECT_FALSE(found.front().evidence.empty());
}

TEST(PolicyEngineTests, HigherPriorityActiveRuleWins) {
    const std::vector<PolicyRule> rules{
        {"low", "Low", 1, TrafficRoute::Direct, PolicyAction::Bypass, true},
        {"high-disabled", "Disabled", 100, TrafficRoute::Direct, PolicyAction::Inspect, false},
        {"high", "High", 10, TrafficRoute::Direct, PolicyAction::Inspect, true}};
    const auto decision = PolicyEngine{}.evaluate(TrafficRoute::Direct, rules, "custom");
    EXPECT_EQ(decision.rule_id, "high");
    EXPECT_EQ(decision.action, PolicyAction::Inspect);
}

TEST(PolicyEngineTests, VpnSafeProfileBypassesDirectAndUnknownTraffic) {
    const auto rules = PolicyEngine::rules_for_profile("vpn-safe");
    EXPECT_EQ(PolicyEngine{}.evaluate(TrafficRoute::Direct, rules, "vpn-safe").action, PolicyAction::Bypass);
    EXPECT_EQ(PolicyEngine{}.evaluate(TrafficRoute::Unknown, rules, "vpn-safe").action, PolicyAction::Bypass);
}

TEST(PolicyEngineTests, UnknownTrafficBypassesEvenWithoutAnExplicitRule) {
    const auto decision = PolicyEngine{}.evaluate(TrafficRoute::Unknown, {}, "custom");
    EXPECT_EQ(decision.action, PolicyAction::Bypass);
    EXPECT_EQ(decision.rule_id, "safe-default-bypass");
}

TEST(PolicyEngineTests, ApplyProfileReturnsTheConfiguredTarget) {
    const std::vector<PolicyRule> rules{
        {"switch-to-vpn-safe", "Use VPN Safe", 10, TrafficRoute::Direct,
         PolicyAction::ApplyProfile, true, "vpn-safe"}};
    const auto decision = PolicyEngine{}.evaluate(TrafficRoute::Direct, rules, "default");
    EXPECT_EQ(decision.action, PolicyAction::ApplyProfile);
    EXPECT_EQ(decision.rule_id, "switch-to-vpn-safe");
    EXPECT_EQ(decision.profile, "vpn-safe");
}

TEST(PolicyEngineTests, ApplyProfileWithoutTargetFallsBackToBypass) {
    const std::vector<PolicyRule> rules{
        {"missing-target", "Missing target", 10, TrafficRoute::Direct,
         PolicyAction::ApplyProfile, true}};
    const auto decision = PolicyEngine{}.evaluate(TrafficRoute::Direct, rules, "default");
    EXPECT_EQ(decision.action, PolicyAction::Bypass);
    EXPECT_EQ(decision.rule_id, "invalid-apply-profile-target");
    EXPECT_EQ(decision.profile, "default");
}

TEST(DiagnosticsTests, HandlesMissingInterfacesAndDefaultRoutes) {
    const auto findings = Diagnostics{}.analyze({}, {}, {});
    const auto has = [&](const char* code) {
        return std::any_of(findings.begin(), findings.end(), [&](const auto& item) { return item.code == code; });
    };
    EXPECT_TRUE(has("no_active_interfaces"));
    EXPECT_TRUE(has("no_default_route"));
    EXPECT_TRUE(has("no_ipv4"));
}

TEST(CoreEngineTests, UsesInjectedMockNetworkProviders) {
    auto interfaces = std::make_shared<MockInterfaceProvider>(std::vector<NetworkInterface>{physical()});
    auto routes = std::make_shared<MockRouteProvider>(std::vector<Route>{route(1, "0.0.0.0", 0)});
    const auto snapshot = CoreEngine(interfaces, routes).scan();
    ASSERT_TRUE(snapshot);
    ASSERT_EQ(snapshot.value().interfaces.size(), 1U);
    ASSERT_EQ(snapshot.value().routes.size(), 1U);
    EXPECT_EQ(snapshot.value().telemetry.interface_count, 1U);
    EXPECT_EQ(snapshot.value().telemetry.route_count, 1U);
}
} // namespace
} // namespace scpdpi
