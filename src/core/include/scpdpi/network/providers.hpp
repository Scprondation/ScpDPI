#pragma once

#include "scpdpi/network/types.hpp"
#include <memory>

namespace scpdpi {

class IInterfaceProvider {
public:
    virtual ~IInterfaceProvider() = default;
    [[nodiscard]] virtual Result<std::vector<NetworkInterface>> enumerate() const = 0;
};

class IRouteProvider {
public:
    virtual ~IRouteProvider() = default;
    [[nodiscard]] virtual Result<std::vector<Route>> enumerate() const = 0;
};

struct PlatformProviders {
    std::shared_ptr<const IInterfaceProvider> interfaces;
    std::shared_ptr<const IRouteProvider> routes;
};

[[nodiscard]] PlatformProviders make_platform_providers();

} // namespace scpdpi
