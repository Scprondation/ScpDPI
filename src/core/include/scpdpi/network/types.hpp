#pragma once

#include "scpdpi/common/result.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scpdpi {

enum class AddressFamily { IPv4, IPv6 };

struct IpAddress {
    AddressFamily family{AddressFamily::IPv4};
    std::array<std::uint8_t, 16> bytes{};
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] static Result<IpAddress> parse(const std::string& text);
    [[nodiscard]] bool operator==(const IpAddress&) const = default;
};

struct NetworkInterface {
    std::uint32_t index{};
    std::string name;
    std::string description;
    std::string mac_address;
    std::vector<IpAddress> addresses;
    std::uint32_t mtu{};
    bool is_up{};
    bool is_physical{};
    bool is_virtual{};
    std::string kind;
};

struct Route {
    IpAddress destination;
    std::uint8_t prefix_length{};
    std::optional<IpAddress> gateway;
    std::uint32_t interface_index{};
    std::string interface_name;
    std::uint32_t metric{};
    bool is_default{};
};

struct VpnInterface {
    std::uint32_t interface_index{};
    std::string interface_name;
    enum class Confidence { Confirmed, Probable, Unknown } confidence{Confidence::Unknown};
    std::vector<std::string> evidence;
};

struct Diagnostic {
    enum class Severity { Info, Warning, Error } severity{Severity::Info};
    std::string code;
    std::string message;
};

struct Telemetry {
    std::size_t interface_count{};
    std::size_t route_count{};
    std::size_t vpn_interface_count{};
    std::size_t warning_count{};
    std::int64_t updated_at_unix_ms{};
    std::int64_t scan_duration_ms{};
};

struct EngineSnapshot {
    std::vector<NetworkInterface> interfaces;
    std::vector<Route> routes;
    std::vector<VpnInterface> vpn_interfaces;
    std::vector<Diagnostic> diagnostics;
    Telemetry telemetry;
};

} // namespace scpdpi
