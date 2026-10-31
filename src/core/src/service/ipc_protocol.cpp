#include "scpdpi/service/ipc_protocol.hpp"

namespace scpdpi {
Result<IpcRequest> parse_ipc_request(std::string_view message) {
    constexpr std::size_t max_message = 16U * 1024U;
    if (message.empty() || message.size() > max_message)
        return Result<IpcRequest>::failure({"IPC message must be between 1 and 16384 bytes", 0});
    try {
        auto json = nlohmann::json::parse(message);
        if (!json.is_object() || !json.contains("command") || !json["command"].is_string())
            return Result<IpcRequest>::failure({"IPC request requires a string command", 0});
        const auto command = json["command"].get<std::string>();
        if (command != "status" && command != "scan" && command != "start" && command != "stop" &&
            command != "set_profile" && command != "reload_config" &&
            command != "disable_autostart" && command != "enable_autostart" &&
            command != "enable_dpi" && command != "disable_dpi")
            return Result<IpcRequest>::failure({"Unsupported IPC command", 0});
        const auto payload = json.value("payload", nlohmann::json::object());
        if (!payload.is_object()) return Result<IpcRequest>::failure({"IPC payload must be an object", 0});
        if (command == "set_profile") {
            if (!payload.contains("profile") || !payload["profile"].is_string())
                return Result<IpcRequest>::failure({"set_profile requires a profile string", 0});
            const auto profile = payload["profile"].get<std::string>();
            if (profile != "default" && profile != "vpn-safe" && profile != "diagnostic" && profile != "custom")
                return Result<IpcRequest>::failure({"Unknown profile", 0});
        }
        return Result<IpcRequest>::success({command, payload});
    } catch (const nlohmann::json::exception& ex) {
        return Result<IpcRequest>::failure({std::string("Invalid IPC JSON: ") + ex.what(), 0});
    }
}
} // namespace scpdpi
