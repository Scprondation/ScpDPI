#include "scpdpi/vpn/detector.hpp"
#include <algorithm>
#include <cctype>

namespace scpdpi {
namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
bool looks_like_tunnel(const NetworkInterface& iface) {
    const auto text = lower(iface.name + " " + iface.description + " " + iface.kind);
    for (const auto* marker : {"tunnel", "wintun", "wireguard", "openvpn", "xray", "hysteria", "tailscale"})
        if (text.find(marker) != std::string::npos) return true;
    const auto name = lower(iface.name);
    for (const auto* prefix : {"tun", "tap", "wg", "ppp", "utun", "ovpn"})
        if (name.rfind(prefix, 0U) == 0U) return true;
    return false;
}
}

std::vector<VpnInterface> VpnDetector::detect(const std::vector<NetworkInterface>& interfaces,
                                               const std::vector<Route>& routes) const {
    std::vector<VpnInterface> result;
    for (const auto& iface : interfaces) {
        if (iface.kind == "loopback") continue;
        const auto metadata = lower(iface.name + " " + iface.description);
        if (metadata.find("teredo") != std::string::npos) continue;
        const auto route_count = static_cast<std::size_t>(std::count_if(routes.begin(), routes.end(), [&](const Route& route) {
            return route.interface_index == iface.index;
        }));
        const bool default_route = std::any_of(routes.begin(), routes.end(), [&](const Route& route) {
            return route.interface_index == iface.index && route.is_default;
        });
        const bool marker = looks_like_tunnel(iface);
        if (!iface.is_virtual && !marker) continue;

        VpnInterface detection;
        detection.interface_index = iface.index;
        detection.interface_name = iface.name;
        if (iface.is_virtual) detection.evidence.emplace_back("Operating system reports a virtual interface");
        if (marker) detection.evidence.emplace_back("Interface metadata contains a tunnel or VPN hint");
        if (route_count != 0) detection.evidence.emplace_back("Routes use this interface");
        if (default_route) detection.evidence.emplace_back("A default route uses this interface");

        if (iface.is_up && iface.kind == "tunnel" && route_count != 0)
            detection.confidence = VpnInterface::Confidence::Confirmed;
        else if (iface.is_up && (iface.is_virtual || marker) && route_count != 0)
            detection.confidence = VpnInterface::Confidence::Probable;
        else
            detection.confidence = VpnInterface::Confidence::Unknown;
        result.push_back(std::move(detection));
    }
    return result;
}
} // namespace scpdpi
