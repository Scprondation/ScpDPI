#pragma once

#include "dpi/packet_engine.hpp"

#include <cstdint>
#include <string>

namespace scpdpi {

/** Owns ScpDPI's in-process Windows packet engine. */
class DpiBackend {
public:
    DpiBackend() = default;
    ~DpiBackend() = default;
    DpiBackend(const DpiBackend&) = delete;
    DpiBackend& operator=(const DpiBackend&) = delete;

    [[nodiscard]] Result<void> start();
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::string last_error() const;
    [[nodiscard]] dpi::PacketEngineStats stats() const noexcept;

private:
    dpi::PacketEngine engine_;
};

} // namespace scpdpi
