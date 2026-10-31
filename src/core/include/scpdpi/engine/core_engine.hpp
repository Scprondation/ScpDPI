#pragma once

#include "scpdpi/network/providers.hpp"
#include "scpdpi/classifier/traffic_classifier.hpp"
#include <memory>

namespace scpdpi {

class CoreEngine {
public:
    CoreEngine(std::shared_ptr<const IInterfaceProvider> interfaces,
               std::shared_ptr<const IRouteProvider> routes);
    [[nodiscard]] Result<EngineSnapshot> scan() const;
    [[nodiscard]] TrafficRoute classify(const EngineSnapshot& snapshot, const IpAddress& destination) const;
private:
    std::shared_ptr<const IInterfaceProvider> interfaces_;
    std::shared_ptr<const IRouteProvider> routes_;
};

} // namespace scpdpi
