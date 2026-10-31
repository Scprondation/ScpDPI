#pragma once
#include "scpdpi/network/types.hpp"
namespace scpdpi {
class TelemetryCollector {
public:
    [[nodiscard]] Telemetry collect(std::size_t interfaces, std::size_t routes, std::size_t vpns,
                                    const std::vector<Diagnostic>& diagnostics, std::int64_t duration_ms) const;
};
} // namespace scpdpi
