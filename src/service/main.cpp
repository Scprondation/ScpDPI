#include "ipc_server.hpp"
#include "console_ui.hpp"
#include "autostart.hpp"
#include "service_client.hpp"
#include "scpdpi/config/config.hpp"
#include "scpdpi/common/logger.hpp"
#include "scpdpi/engine/core_engine.hpp"
#include "scpdpi/network/providers.hpp"
#include "scpdpi/policy/policy_engine.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>
#ifndef _WIN32
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
using namespace scpdpi;
using nlohmann::json;

json address_json(const IpAddress& address) { return {{"address", address.to_string()}, {"family", address.family == AddressFamily::IPv4 ? "ipv4" : "ipv6"}}; }
json snapshot_json(const EngineSnapshot& snapshot) {
    json interfaces = json::array(), routes = json::array(), vpns = json::array(), diagnostics = json::array();
    for (const auto& iface : snapshot.interfaces) {
        json addresses = json::array(); for (const auto& address : iface.addresses) addresses.push_back(address_json(address));
        interfaces.push_back({{"index", iface.index}, {"name", iface.name}, {"description", iface.description},
                              {"mac", iface.mac_address}, {"mtu", iface.mtu}, {"up", iface.is_up},
                              {"physical", iface.is_physical}, {"virtual", iface.is_virtual}, {"kind", iface.kind}, {"addresses", addresses}});
    }
    for (const auto& route : snapshot.routes) {
        routes.push_back({{"destination", route.destination.to_string()}, {"prefix", route.prefix_length},
                          {"gateway", route.gateway ? route.gateway->to_string() : ""}, {"interface", route.interface_name},
                          {"interface_index", route.interface_index}, {"metric", route.metric}, {"default", route.is_default}});
    }
    for (const auto& vpn : snapshot.vpn_interfaces) {
        const auto confidence = vpn.confidence == VpnInterface::Confidence::Confirmed ? "confirmed" :
                                vpn.confidence == VpnInterface::Confidence::Probable ? "probable" : "unknown";
        vpns.push_back({{"name", vpn.interface_name}, {"index", vpn.interface_index}, {"confidence", confidence}, {"evidence", vpn.evidence}});
    }
    for (const auto& diagnostic : snapshot.diagnostics) {
        const auto severity = diagnostic.severity == Diagnostic::Severity::Error ? "error" :
                              diagnostic.severity == Diagnostic::Severity::Warning ? "warning" : "info";
        diagnostics.push_back({{"code", diagnostic.code}, {"severity", severity}, {"message", diagnostic.message}});
    }
    const auto& t = snapshot.telemetry;
    return {{"interfaces", interfaces}, {"routes", routes}, {"vpn_interfaces", vpns}, {"diagnostics", diagnostics},
            {"telemetry", {{"interface_count", t.interface_count}, {"route_count", t.route_count},
                           {"vpn_interface_count", t.vpn_interface_count}, {"warning_count", t.warning_count},
                           {"updated_at_unix_ms", t.updated_at_unix_ms}, {"scan_duration_ms", t.scan_duration_ms}}}};
}

std::filesystem::path user_config_path() {
#ifdef _WIN32
    std::array<wchar_t, 32768> base{};
    const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", base.data(), static_cast<DWORD>(base.size()));
    if (size > 0U && size < base.size()) return std::filesystem::path(base.data()) / L"ScpDPI" / L"config.json";
    return std::filesystem::temp_directory_path() / "ScpDPI" / "config.json";
#else
    if (const char* base = std::getenv("XDG_CONFIG_HOME"); base != nullptr && *base != '\0') return std::filesystem::path(base) / "scpdpi" / "config.json";
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') return std::filesystem::path(home) / ".config" / "scpdpi" / "config.json";
    return std::filesystem::path("/tmp") / ("scpdpi-" + std::to_string(static_cast<unsigned long long>(std::getuid()))) / "config.json";
#endif
}

class ControlPlane {
public:
    ControlPlane() : providers_(make_platform_providers()), engine_(providers_.interfaces, providers_.routes), config_path_(user_config_path()) {
        const auto config = config_store_.load(config_path_);
        if (config) config_ = config.value();
        const auto logging = Logger::initialize(config_path_.parent_path() / "scpdpi.log", config_.log_level, config_.file_logging);
        if (!logging) std::cerr << "ScpDPI logger initialization failed: " << logging.error().message << '\n';
        else spdlog::info("ScpDPI service initialized with profile {}", config_.profile);
        running_.store(config_.engine_enabled);
        (void)refresh();
    }
    Result<EngineSnapshot> refresh() {
        auto result = engine_.scan();
        if (result) {
            std::lock_guard lock(mutex_);
            latest_ = result.value();
            has_latest_ = true;
            spdlog::debug("Network scan completed: {} interfaces and {} routes", latest_.interfaces.size(), latest_.routes.size());
        } else {
            spdlog::error("Network scan failed: {}", result.error().message);
        }
        return result;
    }
    json handle(const IpcRequest& request) {
        if (request.command == "start" || request.command == "stop") {
            const bool enabled = request.command == "start";
            std::lock_guard lock(mutex_);
            config_.engine_enabled = enabled;
            const auto saved = config_store_.save(config_, config_path_);
            if (!saved) return {{"ok", false}, {"error", saved.error().message}};
            running_.store(enabled);
            return {{"ok", true}, {"running", enabled}};
        }
        if (request.command == "set_profile") {
            std::lock_guard lock(mutex_);
            config_.profile = request.payload.at("profile").get<std::string>();
            const auto saved = config_store_.save(config_, config_path_);
            if (!saved) return {{"ok", false}, {"error", saved.error().message}};
            return {{"ok", true}, {"profile", config_.profile}};
        }
        if (request.command == "reload_config") {
            auto loaded = config_store_.load(config_path_);
            if (!loaded) return {{"ok", false}, {"error", loaded.error().message}};
            { std::lock_guard lock(mutex_); config_ = loaded.value(); }
            running_.store(loaded.value().engine_enabled);
            return status();
        }
        if (request.command == "disable_autostart") {
            const auto removed = remove_autostart();
            if (!removed) return {{"ok", false}, {"error", removed.error().message}};
            return {{"ok", true}};
        }
        if (request.command == "enable_autostart") {
            const auto enabled = enable_autostart();
            if (!enabled) return {{"ok", false}, {"error", enabled.error().message}};
            return {{"ok", true}};
        }
        if (request.command == "scan") {
            auto result = refresh();
            if (!result) return {{"ok", false}, {"error", result.error().message}};
            std::lock_guard lock(mutex_);
            return {{"ok", true}, {"running", running_.load()}, {"profile", config_.profile}, {"snapshot", snapshot_json(result.value())}};
        }
        return status();
    }
    [[nodiscard]] bool running() const noexcept { return running_.load(); }
    [[nodiscard]] std::uint32_t interval_ms() const {
        std::lock_guard lock(mutex_);
        return config_.scan_interval_ms;
    }
    json status() const {
        std::lock_guard lock(mutex_);
        json response{{"ok", true}, {"running", running_.load()}, {"profile", config_.profile}};
        if (has_latest_) response["snapshot"] = snapshot_json(latest_);
        else response["snapshot"] = nullptr;
        return response;
    }
private:
    PlatformProviders providers_;
    CoreEngine engine_;
    std::filesystem::path config_path_;
    ConfigStore config_store_;
    AppConfig config_;
    std::atomic<bool> running_{true};
    mutable std::mutex mutex_;
    EngineSnapshot latest_;
    bool has_latest_{};
};
}

int main(int argc, char** argv) {
    try {
        const bool serve = argc > 1 && std::string_view(argv[1]) == "--serve";
        const bool snapshot = argc > 1 && std::string_view(argv[1]) == "--snapshot";
        const bool console = argc == 1 || std::string_view(argv[1]) == "--console";
        const bool enable_startup = argc > 1 && std::string_view(argv[1]) == "--enable-autostart";
        const bool remove_startup = argc > 1 && std::string_view(argv[1]) == "--remove-autostart";
        const bool help = argc > 1 && std::string_view(argv[1]) == "--help";
        if (help) {
            std::cout << "Usage: scpdpi_service.exe [--console | --snapshot | --serve | --enable-autostart | --remove-autostart]\n"
                         "  no option  Open the interactive console\n"
                         "  --console  Open the interactive console\n"
                         "  --snapshot Print one JSON network snapshot\n"
                         "  --serve    Run the local IPC background service\n"
                         "  --enable-autostart  Enable launch when the current Windows user signs in\n"
                         "  --remove-autostart  Disable launch when the current Windows user signs in\n";
            return 0;
        }
        if (remove_startup) {
            const auto removed = scpdpi::remove_autostart();
            if (!removed) { std::cerr << removed.error().message << '\n'; return 1; }
            std::cout << "ScpDPI autostart disabled.\n";
            return 0;
        }
        if (enable_startup) {
            const auto enabled = scpdpi::enable_autostart();
            if (!enabled) { std::cerr << enabled.error().message << '\n'; return 1; }
            std::cout << "ScpDPI autostart enabled.\n";
            return 0;
        }
        if (argc > 1 && !serve && !snapshot && !console) {
            std::cerr << "Unknown option. Use --help to see available options.\n";
            return 2;
        }

        if (console) {
#ifdef _WIN32
            const auto installed = scpdpi::install_and_register_autostart();
            if (!installed) { std::cerr << installed.error().message << '\n'; return 1; }
            const auto started = scpdpi::ensure_background_service(installed.value());
            if (!started) { std::cerr << started.error().message << '\n'; return 1; }
            return scpdpi::run_console({
                [](const std::string& command, const json& payload) {
                    return scpdpi::request_service(command, payload);
                }
            });
#else
            ControlPlane control;
            std::jthread scanner([&](std::stop_token stop) {
                while (!stop.stop_requested()) {
                    if (control.running()) (void)control.refresh();
                    std::this_thread::sleep_for(std::chrono::milliseconds(control.interval_ms()));
                }
            });
            const int exit_code = scpdpi::run_console({
                [&](const std::string& command, const json& payload) {
                    return control.handle(scpdpi::IpcRequest{command, payload});
                }
            });
            scanner.request_stop();
            return exit_code;
#endif
        }

        auto providers = scpdpi::make_platform_providers();
        if (!providers.interfaces || !providers.routes) {
            std::cerr << "This operating system does not have a ScpDPI network provider.\n";
            return 2;
        }
        if (snapshot) {
            scpdpi::CoreEngine engine(providers.interfaces, providers.routes);
            auto result = engine.scan();
            if (!result) { std::cerr << result.error().message << '\n'; return 1; }
            std::cout << snapshot_json(result.value()).dump(2) << '\n';
            return 0;
        }
        ControlPlane control;
        std::jthread scanner([&](std::stop_token stop) {
            while (!stop.stop_requested()) {
                if (control.running()) (void)control.refresh();
                std::this_thread::sleep_for(std::chrono::milliseconds(control.interval_ms()));
            }
        });
        scpdpi::IpcServer server;
        auto result = server.run({}, [&](const scpdpi::IpcRequest& request) { return control.handle(request); });
        scanner.request_stop();
        if (!result) { std::cerr << result.error().message << '\n'; return 1; }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ScpDPI service error: " << ex.what() << '\n';
        return 1;
    }
}
