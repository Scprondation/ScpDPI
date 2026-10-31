#pragma once
#include "scpdpi/network/types.hpp"
namespace scpdpi {
class Diagnostics {
public:
    [[nodiscard]] std::vector<Diagnostic> analyze(const std::vector<NetworkInterface>& interfaces,
                                                   const std::vector<Route>& routes,
                                                   const std::vector<VpnInterface>& vpns) const;
};
} // namespace scpdpi
