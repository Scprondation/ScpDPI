#include "scpdpi/classifier/traffic_classifier.hpp"
#include <algorithm>

namespace scpdpi {
namespace {
bool contains(const Route& route, const IpAddress& address) {
    if (route.destination.family != address.family) return false;
    const std::size_t bytes = address.family == AddressFamily::IPv4 ? 4U : 16U;
    if (route.prefix_length > bytes * 8U) return false;
    const std::size_t full = route.prefix_length / 8U;
    const unsigned remainder = route.prefix_length % 8U;
    for (std::size_t i = 0; i < full && i < bytes; ++i)
        if (route.destination.bytes[i] != address.bytes[i]) return false;
    if (remainder != 0U && full < bytes) {
        const auto mask = static_cast<std::uint8_t>(0xFFU << (8U - remainder));
        if ((route.destination.bytes[full] & mask) != (address.bytes[full] & mask)) return false;
    }
    return true;
}
}

TrafficRoute TrafficClassifier::classify(const std::vector<NetworkInterface>& interfaces,
                                          const std::vector<Route>& routes,
                                          const std::vector<VpnInterface>& vpns,
                                          const IpAddress& destination) const {
    const Route* best = nullptr;
    bool ambiguous = false;
    for (const auto& route : routes) {
        if (!contains(route, destination)) continue;
        if (best == nullptr || route.prefix_length > best->prefix_length ||
            (route.prefix_length == best->prefix_length && route.metric < best->metric)) {
            best = &route;
            ambiguous = false;
        } else if (route.prefix_length == best->prefix_length && route.metric == best->metric &&
                   route.interface_index != best->interface_index) {
            ambiguous = true;
        }
    }
    if (best == nullptr || ambiguous) return TrafficRoute::Unknown;
    const auto vpn = std::find_if(vpns.begin(), vpns.end(), [&](const VpnInterface& item) {
        return item.interface_index == best->interface_index && item.confidence != VpnInterface::Confidence::Unknown;
    });
    if (vpn != vpns.end()) return TrafficRoute::Vpn;
    const auto iface = std::find_if(interfaces.begin(), interfaces.end(), [&](const NetworkInterface& item) {
        return item.index == best->interface_index;
    });
    if (iface == interfaces.end() || !iface->is_up) return TrafficRoute::Unknown;
    if (iface->is_physical || !iface->is_virtual) return TrafficRoute::Direct;
    return TrafficRoute::Unknown;
}

const char* to_string(TrafficRoute route) noexcept {
    switch (route) {
    case TrafficRoute::Vpn: return "vpn";
    case TrafficRoute::Direct: return "direct";
    case TrafficRoute::Unknown: return "unknown";
    }
    return "unknown";
}
} // namespace scpdpi
