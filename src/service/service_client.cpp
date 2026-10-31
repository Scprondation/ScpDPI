#include "service_client.hpp"
#include "autostart.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace scpdpi {
namespace {
using Json = nlohmann::json;
constexpr std::size_t max_response_size = 4U * 1024U * 1024U;

Json request_error(std::string message, int native_code = 0) {
    Json result{{"ok", false}, {"error", std::move(message)}};
    if (native_code != 0) result["native_code"] = native_code;
    return result;
}

#ifdef _WIN32
Json exchange(const std::string& wire) {
    constexpr wchar_t endpoint[] = L"\\\\.\\pipe\\ScpDPI.v2";
    if (!WaitNamedPipeW(endpoint, 1500U))
        return request_error("ScpDPI background service is not responding", static_cast<int>(GetLastError()));
    HANDLE pipe = CreateFileW(endpoint, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
        return request_error("Could not connect to the ScpDPI background service", static_cast<int>(GetLastError()));
    struct PipeGuard { HANDLE pipe; ~PipeGuard() { if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe); } } guard{pipe};

    std::size_t sent = 0;
    while (sent < wire.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(wire.size() - sent, 16U * 1024U));
        DWORD written = 0;
        if (!WriteFile(pipe, wire.data() + sent, chunk, &written, nullptr) || written == 0U)
            return request_error("Could not send a request to the ScpDPI background service", static_cast<int>(GetLastError()));
        sent += written;
    }

    std::string response;
    std::array<char, 4096> buffer{};
    while (response.size() <= max_response_size) {
        DWORD received = 0;
        if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &received, nullptr))
            return request_error("Could not read the ScpDPI background service response", static_cast<int>(GetLastError()));
        if (received == 0U) break;
        response.append(buffer.data(), received);
        if (response.find('\n') != std::string::npos) break;
    }
    const auto newline = response.find('\n');
    if (newline == std::string::npos) return request_error("ScpDPI service returned an incomplete or oversized response");
    try {
        const auto parsed = Json::parse(response.begin(), response.begin() + static_cast<std::ptrdiff_t>(newline));
        if (!parsed.is_object()) return request_error("ScpDPI service returned an invalid response object");
        return parsed;
    } catch (const Json::exception& error) {
        return request_error(std::string("ScpDPI service returned invalid JSON: ") + error.what());
    }
}
#else
std::string endpoint_path() {
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime != nullptr && *runtime != '\0')
        return std::string(runtime) + "/scpdpi.sock";
    return "/tmp/scpdpi-" + std::to_string(static_cast<unsigned long long>(getuid())) + ".sock";
}

Json exchange(const std::string& wire) {
    const int socket_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) return request_error("Could not create a local service socket", errno);
    struct SocketGuard { int fd; ~SocketGuard() { if (fd >= 0) close(fd); } } guard{socket_fd};
    const auto path = endpoint_path();
    sockaddr_un address{};
    if (path.size() >= sizeof(address.sun_path)) return request_error("ScpDPI socket path exceeds the system limit");
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
        return request_error("ScpDPI local service is not running", errno);
    std::size_t sent = 0;
    while (sent < wire.size()) {
        const auto count = send(socket_fd, wire.data() + sent, wire.size() - sent, MSG_NOSIGNAL);
        if (count < 0) { if (errno == EINTR) continue; return request_error("Could not send a request to ScpDPI", errno); }
        if (count == 0) return request_error("ScpDPI socket closed while sending a request");
        sent += static_cast<std::size_t>(count);
    }
    std::string response;
    std::array<char, 4096> buffer{};
    while (response.size() <= max_response_size) {
        const auto count = recv(socket_fd, buffer.data(), buffer.size(), 0);
        if (count < 0) { if (errno == EINTR) continue; return request_error("Could not read ScpDPI service response", errno); }
        if (count == 0) break;
        response.append(buffer.data(), static_cast<std::size_t>(count));
        if (response.find('\n') != std::string::npos) break;
    }
    const auto newline = response.find('\n');
    if (newline == std::string::npos) return request_error("ScpDPI service returned an incomplete or oversized response");
    try {
        const auto parsed = Json::parse(response.begin(), response.begin() + static_cast<std::ptrdiff_t>(newline));
        return parsed.is_object() ? parsed : request_error("ScpDPI service returned an invalid response object");
    } catch (const Json::exception& error) {
        return request_error(std::string("ScpDPI service returned invalid JSON: ") + error.what());
    }
}
#endif
}

Json request_service(const std::string& command, const Json& payload) {
    const std::string wire = Json{{"command", command}, {"payload", payload}}.dump() + '\n';
    return exchange(wire);
}

Result<void> ensure_background_service(const std::filesystem::path& install_dir) {
#ifdef _WIN32
    const auto expected_path = expected_protected_runtime_path(install_dir);
    if (!expected_path) return Result<void>::failure(expected_path.error());
    const auto matches_expected_service = [&](const Json& status) {
        if (!status.is_object() || !status.value("ok", false) || !status.contains("process_path") ||
            !status["process_path"].is_string()) return false;
        const std::string utf8_path = status["process_path"].get<std::string>();
        if (utf8_path.empty()) return false;
        const int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path.data(),
                                                     static_cast<int>(utf8_path.size()), nullptr, 0);
        if (wide_length <= 0) return false;
        std::wstring running_path(static_cast<std::size_t>(wide_length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path.data(), static_cast<int>(utf8_path.size()),
                                running_path.data(), wide_length) != wide_length) return false;
        return CompareStringOrdinal(running_path.c_str(), -1, expected_path.value().c_str(), -1, TRUE) == CSTR_EQUAL;
    };
    const auto current_status = request_service("status", Json::object());
    const bool restart_running_service = matches_expected_service(current_status);
    if (restart_running_service) {
        const auto restarted = restart_elevated_background_task();
        if (!restarted) return restarted;
    }
    const bool stale_service_running = current_status.is_object() && current_status.value("ok", false);
    if (!restart_running_service) {
        const auto task = ensure_elevated_background_task(install_dir, stale_service_running);
        if (!task) return task;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (matches_expected_service(request_service("status", Json::object()))) return Result<void>::success();
        Sleep(150U);
    }
    return Result<void>::failure({"ScpDPI background process did not open its local control pipe", 0});
#else
    (void)install_dir;
    return Result<void>::failure({"Automatic background startup is implemented for Windows only", 0});
#endif
}

} // namespace scpdpi
