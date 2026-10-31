#include "dpi_backend.hpp"

#include <array>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace scpdpi {
namespace {

std::filesystem::path runtime_directory() {
#ifdef _WIN32
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) return {};
    return std::filesystem::path(buffer.data()).parent_path() / L"scpdpi-dpi";
#else
    return std::filesystem::current_path() / "scpdpi-dpi";
#endif
}

} // namespace

Result<void> DpiBackend::start() {
    return engine_.start(runtime_directory());
}

Result<void> DpiBackend::stop() {
    return engine_.stop();
}

bool DpiBackend::running() const noexcept {
    return engine_.running();
}

std::string DpiBackend::last_error() const {
    return engine_.last_error();
}

dpi::PacketEngineStats DpiBackend::stats() const noexcept {
    return engine_.stats();
}

} // namespace scpdpi
