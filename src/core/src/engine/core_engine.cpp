#include "scpdpi/engine/core_engine.hpp"
#include "scpdpi/diagnostics/diagnostics.hpp"
#include "scpdpi/telemetry/telemetry.hpp"
#include "scpdpi/vpn/detector.hpp"
#include <chrono>
#include <stdexcept>

namespace scpdpi {
CoreEngine::CoreEngine(std::shared_ptr<const IInterfaceProvider> interfaces,
                       std::shared_ptr<const IRouteProvider> routes)
    : interfaces_(std::move(interfaces)), routes_(std::move(routes)) {
    if (!interfaces_ || !routes_) throw std::invalid_argument("CoreEngine requires network providers");
}

Result<EngineSnapshot> CoreEngine::scan() const {
    const auto started = std::chrono::steady_clock::now();
    auto interfaces = interfaces_->enumerate();
    if (!interfaces) return Result<EngineSnapshot>::failure(interfaces.error());
    auto routes = routes_->enumerate();
    if (!routes) return Result<EngineSnapshot>::failure(routes.error());
    EngineSnapshot snapshot;
    snapshot.interfaces = std::move(interfaces.value());
    snapshot.routes = std::move(routes.value());
    snapshot.vpn_interfaces = VpnDetector{}.detect(snapshot.interfaces, snapshot.routes);
    snapshot.diagnostics = Diagnostics{}.analyze(snapshot.interfaces, snapshot.routes, snapshot.vpn_interfaces);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    snapshot.telemetry = TelemetryCollector{}.collect(snapshot.interfaces.size(), snapshot.routes.size(),
                                                       snapshot.vpn_interfaces.size(), snapshot.diagnostics, elapsed);
    return Result<EngineSnapshot>::success(std::move(snapshot));
}

TrafficRoute CoreEngine::classify(const EngineSnapshot& snapshot, const IpAddress& destination) const {
    return TrafficClassifier{}.classify(snapshot.interfaces, snapshot.routes, snapshot.vpn_interfaces, destination);
}
} // namespace scpdpi
