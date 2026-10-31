#pragma once

#include "scpdpi/common/result.hpp"

#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace scpdpi {

/** Starts and supervises the pinned Windows DPI packet-processing backend. */
class DpiBackend {
public:
    DpiBackend() = default;
    ~DpiBackend();
    DpiBackend(const DpiBackend&) = delete;
    DpiBackend& operator=(const DpiBackend&) = delete;

    [[nodiscard]] Result<void> start();
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool running();
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
#ifdef _WIN32
    HANDLE process_{};
    HANDLE job_{};
#endif
    std::string last_error_;
};

} // namespace scpdpi
