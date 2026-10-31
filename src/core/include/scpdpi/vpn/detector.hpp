#pragma once
#include "scpdpi/network/types.hpp"
namespace scpdpi {
class VpnDetector {
public:
    [[nodiscard]] std::vector<VpnInterface> detect(const std::vector<NetworkInterface>& interfaces,
                                                   const std::vector<Route>& routes) const;
};
} // namespace scpdpi
