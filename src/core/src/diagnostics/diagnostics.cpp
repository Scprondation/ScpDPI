#include "scpdpi/diagnostics/diagnostics.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace scpdpi {
std::vector<Diagnostic> Diagnostics::analyze(const std::vector<NetworkInterface>& interfaces,
                                              const std::vector<Route>& routes,
                                              const std::vector<VpnInterface>& vpns) const {
    std::vector<Diagnostic> out;
    const auto active = std::count_if(interfaces.begin(), interfaces.end(), [](const auto& i) { return i.is_up; });
    if (active == 0) out.push_back({Diagnostic::Severity::Error, "no_active_interfaces", "No active network interfaces were found."});
    bool has_v4 = false, has_v6 = false;
    for (const auto& iface : interfaces) {
        if (!iface.is_up) continue;
        has_v4 |= std::any_of(iface.addresses.begin(), iface.addresses.end(), [](const auto& a) { return a.family == AddressFamily::IPv4; });
        has_v6 |= std::any_of(iface.addresses.begin(), iface.addresses.end(), [](const auto& a) { return a.family == AddressFamily::IPv6; });
        if (iface.mtu != 0U && iface.mtu < 576U)
            out.push_back({Diagnostic::Severity::Warning, "low_mtu", "Interface " + iface.name + " reports an unusually small MTU."});
    }
    if (!has_v4) out.push_back({Diagnostic::Severity::Warning, "no_ipv4", "No active interface has an IPv4 address."});
    if (!has_v6) out.push_back({Diagnostic::Severity::Info, "no_ipv6", "No active interface has an IPv6 address."});

    const auto count_default = [&](AddressFamily family) {
        return static_cast<std::size_t>(std::count_if(routes.begin(), routes.end(), [&](const Route& r) {
            return r.is_default && r.destination.family == family;
        }));
    };
    const auto defaults_v4 = count_default(AddressFamily::IPv4);
    const auto defaults_v6 = count_default(AddressFamily::IPv6);
    if (defaults_v4 + defaults_v6 == 0U)
        out.push_back({Diagnostic::Severity::Error, "no_default_route", "No IPv4 or IPv6 default route was found."});
    else {
        if (defaults_v4 > 1U) out.push_back({Diagnostic::Severity::Warning, "multiple_ipv4_defaults", "Several IPv4 default routes compete; the lowest route metric is preferred."});
        if (defaults_v6 > 1U) out.push_back({Diagnostic::Severity::Warning, "multiple_ipv6_defaults", "Several IPv6 default routes compete; the lowest route metric is preferred."});
    }

    for (const auto& vpn : vpns) {
        if (vpn.confidence == VpnInterface::Confidence::Unknown)
            out.push_back({Diagnostic::Severity::Warning, "unknown_virtual_interface", "Virtual interface " + vpn.interface_name + " could not be classified confidently."});
    }
    for (const auto& route : routes) {
        if (!route.gateway) continue;
        const auto iface = std::find_if(interfaces.begin(), interfaces.end(), [&](const NetworkInterface& i) { return i.index == route.interface_index; });
        if (iface != interfaces.end() && std::find(iface->addresses.begin(), iface->addresses.end(), *route.gateway) != iface->addresses.end())
            out.push_back({Diagnostic::Severity::Warning, "possible_route_loop", "Route gateway equals a local address on " + iface->name + "; verify the route manually."});
    }
    std::map<std::string, std::set<std::uint32_t>> same_cost_defaults;
    for (const auto& route : routes) if (route.is_default)
        same_cost_defaults[(route.destination.family == AddressFamily::IPv4 ? "4:" : "6:") + std::to_string(route.metric)].insert(route.interface_index);
    for (const auto& [key, indexes] : same_cost_defaults) {
        (void)key;
        if (indexes.size() > 1U) {
            out.push_back({Diagnostic::Severity::Warning, "routing_conflict", "Equal-cost default routes use different interfaces; traffic selection may depend on OS policy."});
            break;
        }
    }
    return out;
}
} // namespace scpdpi
