#pragma once
#include "scpdpi/network/types.hpp"

namespace scpdpi {
enum class TrafficRoute { Vpn, Direct, Unknown };
class TrafficClassifier {
public:
    [[nodiscard]] TrafficRoute classify(const std::vector<NetworkInterface>& interfaces,
                                        const std::vector<Route>& routes,
                                        const std::vector<VpnInterface>& vpns,
                                        const IpAddress& destination) const;
};
[[nodiscard]] const char* to_string(TrafficRoute route) noexcept;
} // namespace scpdpi
