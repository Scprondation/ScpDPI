#include "ipc_server.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <vector>
#include <string>
#include <thread>
#include <spdlog/spdlog.h>

namespace scpdpi {
Result<void> IpcServer::run(std::stop_token stop, const Handler& handler) const {
    constexpr wchar_t pipe_name[] = L"\\\\.\\pipe\\ScpDPI.v2";
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return Result<void>::failure({"Could not inspect the service user for IPC security", static_cast<int>(GetLastError())});
    struct TokenGuard { HANDLE value; ~TokenGuard() { if (value != nullptr) CloseHandle(value); } } token_guard{token};
    DWORD token_size = 0;
    (void)GetTokenInformation(token, TokenUser, nullptr, 0, &token_size);
    std::vector<unsigned char> token_data(token_size);
    if (token_size == 0U || !GetTokenInformation(token, TokenUser, token_data.data(), token_size, &token_size))
        return Result<void>::failure({"Could not read the service user SID", static_cast<int>(GetLastError())});
    LPWSTR sid_text = nullptr;
    const auto* user = reinterpret_cast<const TOKEN_USER*>(token_data.data());
    if (!ConvertSidToStringSidW(user->User.Sid, &sid_text))
        return Result<void>::failure({"Could not format the service user SID", static_cast<int>(GetLastError())});
    const std::wstring sddl = std::wstring(L"D:P(A;;GA;;;SY)(A;;GA;;;") + sid_text + L")";
    LocalFree(sid_text);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
        return Result<void>::failure({"Could not create a restricted named-pipe ACL", static_cast<int>(GetLastError())});
    struct DescriptorGuard { PSECURITY_DESCRIPTOR value; ~DescriptorGuard() { if (value != nullptr) LocalFree(value); } } descriptor_guard{descriptor};
    SECURITY_ATTRIBUTES security{static_cast<DWORD>(sizeof(SECURITY_ATTRIBUTES)), descriptor, FALSE};
    while (!stop.stop_requested()) {
        HANDLE pipe = CreateNamedPipeW(pipe_name, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
            PIPE_UNLIMITED_INSTANCES, 4096U, 4096U, 0U, &security);
        if (pipe == INVALID_HANDLE_VALUE) return Result<void>::failure({"Could not create local named pipe", static_cast<int>(GetLastError())});
        BOOL connected = FALSE;
        const auto connect_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!connected && std::chrono::steady_clock::now() < connect_deadline) {
            connected = ConnectNamedPipe(pipe, nullptr);
            const DWORD connect_error = connected ? ERROR_SUCCESS : GetLastError();
            if (!connected && connect_error == ERROR_PIPE_CONNECTED) connected = TRUE;
            else if (!connected && connect_error == ERROR_PIPE_LISTENING)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            else if (!connected) break;
        }
        if (!connected) { CloseHandle(pipe); continue; }
        std::string message;
        std::array<char, 2048> buffer{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (message.size() <= 16U * 1024U && std::chrono::steady_clock::now() < deadline) {
            DWORD received = 0;
            if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &received, nullptr)) {
                const auto error = GetLastError();
                if (error == ERROR_NO_DATA) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
                break;
            }
            if (received == 0U) break;
            message.append(buffer.data(), received);
            if (message.find('\n') != std::string::npos) break;
        }
        auto request = parse_ipc_request(message);
        nlohmann::json response;
        if (!request) response = {{"ok", false}, {"error", request.error().message}};
        else response = handler(request.value());
        auto wire = response.dump() + '\n';
        if (wire.size() > 4U * 1024U * 1024U)
            wire = R"({"ok":false,"error":"Response exceeds the 4 MiB limit"})" "\n";
        std::size_t sent = 0;
        const auto write_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (sent < wire.size() && std::chrono::steady_clock::now() < write_deadline) {
            const auto remaining = wire.size() - sent;
            const auto chunk_size = static_cast<DWORD>(std::min<std::size_t>(remaining, buffer.size()));
            DWORD written = 0;
            if (!WriteFile(pipe, wire.data() + sent, chunk_size, &written, nullptr)) {
                const auto error = GetLastError();
                if (error == ERROR_NO_DATA) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
                spdlog::error("Named pipe response write failed after {} of {} bytes (native error {})", sent, wire.size(), error);
                break;
            }
            if (written == 0U) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            sent += written;
        }
        if (sent != wire.size()) spdlog::error("Named pipe response ended after {} of {} bytes", sent, wire.size());
        else FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
    return Result<void>::success();
}
} // namespace scpdpi

#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <string>
#include <thread>

namespace scpdpi {
namespace {
class FileDescriptor {
public:
    explicit FileDescriptor(int value = -1) : value_(value) {}
    ~FileDescriptor() { if (value_ >= 0) close(value_); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    [[nodiscard]] int get() const noexcept { return value_; }
private: int value_;
};
std::string endpoint_path() {
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime != nullptr && *runtime != '\0')
        return std::string(runtime) + "/scpdpi.sock";
    return "/tmp/scpdpi-" + std::to_string(static_cast<unsigned long long>(getuid())) + ".sock";
}
}
Result<void> IpcServer::run(std::stop_token stop, const Handler& handler) const {
    const auto path = endpoint_path();
    sockaddr_un address{};
    if (path.size() >= sizeof(address.sun_path)) return Result<void>::failure({"IPC socket path exceeds the operating system limit", 0});
    struct stat existing{};
    if (lstat(path.c_str(), &existing) == 0) {
        if (!S_ISSOCK(existing.st_mode) || existing.st_uid != getuid())
            return Result<void>::failure({"Refusing to replace an IPC path not owned by this user", 0});
        if (unlink(path.c_str()) != 0) return Result<void>::failure({"Could not remove stale IPC socket", errno});
    } else if (errno != ENOENT) return Result<void>::failure({"Could not inspect IPC path", errno});

    FileDescriptor server(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (server.get() < 0) return Result<void>::failure({"Could not create Unix domain socket", errno});
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
    const auto old_mask = umask(S_IRWXG | S_IRWXO);
    const int bind_result = bind(server.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address));
    const int bind_error = errno;
    (void)umask(old_mask);
    if (bind_result != 0) return Result<void>::failure({"Could not bind Unix domain socket", bind_error});
    if (chmod(path.c_str(), S_IRUSR | S_IWUSR) != 0) { (void)unlink(path.c_str()); return Result<void>::failure({"Could not restrict IPC socket permissions", errno}); }
    struct SocketPathGuard { std::string path; ~SocketPathGuard() { (void)unlink(path.c_str()); } } path_guard{path};
    if (listen(server.get(), 8) != 0) return Result<void>::failure({"Could not listen on Unix domain socket", errno});

    while (!stop.stop_requested()) {
        pollfd descriptor{server.get(), POLLIN, 0};
        const int ready = poll(&descriptor, 1, 250);
        if (ready < 0) { if (errno == EINTR) continue; return Result<void>::failure({"IPC socket wait failed", errno}); }
        if (ready == 0) continue;
        FileDescriptor client(accept4(server.get(), nullptr, nullptr, SOCK_CLOEXEC));
        if (client.get() < 0) { if (errno == EINTR) continue; return Result<void>::failure({"IPC accept failed", errno}); }
        timeval receive_timeout{5, 0};
        (void)setsockopt(client.get(), SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
        ucred credentials{}; socklen_t credential_size = sizeof(credentials);
        if (getsockopt(client.get(), SOL_SOCKET, SO_PEERCRED, &credentials, &credential_size) != 0 || credentials.uid != getuid()) continue;
        std::string message; std::array<char, 2048> buffer{};
        while (message.size() <= 16U * 1024U) {
            const auto count = recv(client.get(), buffer.data(), buffer.size(), 0);
            if (count < 0) { if (errno == EINTR) continue; break; }
            if (count == 0) break;
            message.append(buffer.data(), static_cast<std::size_t>(count));
            if (message.find('\n') != std::string::npos) break;
        }
        auto request = parse_ipc_request(message);
        nlohmann::json response;
        if (!request) response = {{"ok", false}, {"error", request.error().message}};
        else response = handler(request.value());
        const auto wire = response.dump() + '\n';
        std::size_t sent = 0;
        while (sent < wire.size()) {
            const auto count = send(client.get(), wire.data() + sent, wire.size() - sent, MSG_NOSIGNAL);
            if (count < 0) { if (errno == EINTR) continue; break; }
            if (count == 0) break;
            sent += static_cast<std::size_t>(count);
        }
    }
    return Result<void>::success();
}
} // namespace scpdpi
#endif
