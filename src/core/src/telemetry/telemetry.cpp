#include "scpdpi/telemetry/telemetry.hpp"
#include <algorithm>
#include <chrono>

namespace scpdpi {
Telemetry TelemetryCollector::collect(std::size_t interfaces, std::size_t routes, std::size_t vpns,
                                      const std::vector<Diagnostic>& diagnostics, std::int64_t duration_ms) const {
    const auto warnings = std::count_if(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& item) {
        return item.severity == Diagnostic::Severity::Warning || item.severity == Diagnostic::Severity::Error;
    });
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return {interfaces, routes, vpns, static_cast<std::size_t>(warnings), now, duration_ms};
}
} // namespace scpdpi
