#include "ipc_server.hpp"
#include "console_ui.hpp"
#include "autostart.hpp"
#include "service_client.hpp"
#include "dpi_backend.hpp"
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
#include <shlobj.h>
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
    PWSTR base = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &base);
    if (SUCCEEDED(result) && base != nullptr) {
        const std::filesystem::path path = std::filesystem::path(base) / L"ScpDPI" / L"config.json";
        CoTaskMemFree(base);
        return path;
    }
    if (base != nullptr) CoTaskMemFree(base);
    return std::filesystem::temp_directory_path() / "ScpDPI" / "config.json";
#else
    if (const char* base = std::getenv("XDG_CONFIG_HOME"); base != nullptr && *base != '\0') return std::filesystem::path(base) / "scpdpi" / "config.json";
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') return std::filesystem::path(home) / ".config" / "scpdpi" / "config.json";
    return std::filesystem::path("/tmp") / ("scpdpi-" + std::to_string(static_cast<unsigned long long>(std::getuid()))) / "config.json";
#endif
}

std::string current_executable_path_utf8() {
#ifdef _WIN32
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) return {};
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string path(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(length), path.data(), required, nullptr, nullptr);
    if (written != required) return {};
    return path;
#else
    return {};
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
#ifdef _WIN32
        if (config_.dpi_enabled) {
            const auto started = dpi_.start();
            if (!started) spdlog::error("DPI packet processor failed to start: {}", started.error().message);
            else spdlog::info("DPI packet processor started");
        }
#endif
        running_.store(config_.engine_enabled);
        (void)refresh();
    }
    Result<EngineSnapshot> refresh() {
        auto result = engine_.scan();
        if (result) {
            bool report_due = false;
            bool report_dpi_enabled = false;
            {
                std::lock_guard lock(mutex_);
                latest_ = result.value();
                has_latest_ = true;
                spdlog::debug("Network scan completed: {} interfaces and {} routes", latest_.interfaces.size(), latest_.routes.size());
                const auto now = std::chrono::steady_clock::now();
                if (now - last_diagnostic_log_ >= std::chrono::minutes(1)) {
                    last_diagnostic_log_ = now;
                    report_due = true;
                    report_dpi_enabled = config_.dpi_enabled;
                }
            }
            if (report_due) log_diagnostic_snapshot(result.value(), report_dpi_enabled);
        } else {
            spdlog::error("Network scan failed: {}", result.error().message);
        }
        return result;
    }
    void log_diagnostic_snapshot(const EngineSnapshot& snapshot, bool dpi_enabled) const {
        const auto stats = dpi_.stats();
        const auto dpi_error = dpi_.last_error();
        spdlog::info("Diagnostic snapshot: interfaces={} routes={} vpn_candidates={} dpi_enabled={} dpi_running={} "
                     "dpi_error={} packets={} tcp_tls_candidates={} tcp_tls_tunnel_candidates={} tls_parsed={} tls_unlisted={} "
                     "tls_timeouts={} listed_tls_matches={} discord_tls_matches={} split_packets={} split_failures={} "
                     "send_failures={} passive_resets_dropped={} udp443_candidates={} udp443_tunnel_candidates={} "
                     "quic_fallbacks={} discord_quic_fallbacks={} voice_udp_port_candidates={} voice_udp_tunnel_candidates={} "
                     "voice_discovery_requests={} voice_discovery_direct={} voice_discovery_tunnel={} discord_ip_discovery={} "
                     "discord_ip_discovery_other_ports={} "
                     "udp_decoys_sent={} udp_decoys_failed={} tunnel_interfaces={} strategy_changes={}",
                     snapshot.interfaces.size(), snapshot.routes.size(), snapshot.vpn_interfaces.size(),
                     dpi_enabled, dpi_.running(), dpi_error, stats.packets_seen, stats.tcp_tls_candidates_outbound,
                     stats.tcp_tls_candidates_tunnel, stats.tls_hellos_parsed, stats.tls_hellos_unlisted,
                     stats.tls_hellos_timed_out, stats.domains_matched, stats.discord_tls_matches,
                     stats.packets_resegmented, stats.resegment_failures, stats.packet_send_failures,
                     stats.passive_resets_dropped, stats.udp_443_candidates, stats.udp_443_tunnel_candidates,
                     stats.quic_udp_443_dropped, stats.discord_quic_fallbacks, stats.voice_udp_candidate_packets,
                     stats.voice_udp_tunnel_candidate_packets, stats.voice_udp_requests,
                     stats.voice_udp_direct_requests, stats.voice_udp_tunnel_requests,
                     stats.discord_ip_discovery_requests, stats.discord_ip_discovery_other_ports,
                     stats.voice_udp_decoys_sent, stats.voice_udp_decoy_failures,
                     stats.tunnel_interfaces, stats.strategies_changed);
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
        if (request.command == "enable_dpi") {
            std::lock_guard lock(mutex_);
            const auto started = dpi_.start();
            if (!started) return {{"ok", false}, {"error", started.error().message}};
            config_.dpi_enabled = true;
            const auto saved = config_store_.save(config_, config_path_);
            if (!saved) {
                config_.dpi_enabled = false;
                (void)dpi_.stop();
                return {{"ok", false}, {"error", saved.error().message}};
            }
            spdlog::info("DPI packet processor enabled");
            return {{"ok", true}, {"dpi", {{"enabled", true}, {"running", dpi_.running()}}}};
        }
        if (request.command == "disable_dpi") {
            std::lock_guard lock(mutex_);
            config_.dpi_enabled = false;
            const auto saved = config_store_.save(config_, config_path_);
            if (!saved) return {{"ok", false}, {"error", saved.error().message}};
            const auto stopped = dpi_.stop();
            if (!stopped) return {{"ok", false}, {"error", stopped.error().message}};
            spdlog::info("DPI packet processor disabled");
            return {{"ok", true}, {"dpi", {{"enabled", false}, {"running", false}}}};
        }
        if (request.command == "reload_config") {
            auto loaded = config_store_.load(config_path_);
            if (!loaded) return {{"ok", false}, {"error", loaded.error().message}};
#ifdef _WIN32
            const auto dpi_result = loaded.value().dpi_enabled ? dpi_.start() : dpi_.stop();
            if (!dpi_result) return {{"ok", false}, {"error", dpi_result.error().message}};
#endif
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
            return {{"ok", true}, {"api_version", 2}, {"running", running_.load()}, {"profile", config_.profile},
                    {"dpi", {{"enabled", config_.dpi_enabled}, {"running", dpi_.running()}, {"message", dpi_.last_error()}}},
                    {"snapshot", snapshot_json(result.value())}};
        }
        return status();
    }
    [[nodiscard]] bool running() const noexcept { return running_.load(); }
    [[nodiscard]] std::uint32_t interval_ms() const {
        std::lock_guard lock(mutex_);
        return config_.scan_interval_ms;
    }
    json status() {
        std::lock_guard lock(mutex_);
        json response{{"ok", true}, {"api_version", 2}, {"running", running_.load()}, {"profile", config_.profile}};
        const auto stats = dpi_.stats();
        response["dpi"] = {{"enabled", config_.dpi_enabled}, {"running", dpi_.running()},
                            {"message", dpi_.last_error()}, {"packets_seen", stats.packets_seen},
                            {"tcp_443_outbound", stats.tcp_443_outbound}, {"tcp_443_tunnel", stats.tcp_443_tunnel},
                            {"tcp_tls_candidates_outbound", stats.tcp_tls_candidates_outbound},
                            {"tcp_tls_candidates_tunnel", stats.tcp_tls_candidates_tunnel},
                            {"tls_hellos_parsed", stats.tls_hellos_parsed}, {"tls_hellos_unlisted", stats.tls_hellos_unlisted},
                            {"tls_hellos_timed_out", stats.tls_hellos_timed_out}, {"resegment_attempts", stats.resegment_attempts},
                            {"resegment_failures", stats.resegment_failures}, {"packet_send_failures", stats.packet_send_failures},
                            {"packets_resegmented", stats.packets_resegmented}, {"domains_matched", stats.domains_matched},
                            {"discord_tls_matches", stats.discord_tls_matches},
                            {"udp_443_candidates", stats.udp_443_candidates},
                            {"udp_443_tunnel_candidates", stats.udp_443_tunnel_candidates},
                            {"quic_udp_443_dropped", stats.quic_udp_443_dropped},
                            {"discord_quic_fallbacks", stats.discord_quic_fallbacks},
                            {"voice_udp_candidate_packets", stats.voice_udp_candidate_packets},
                            {"voice_udp_tunnel_candidate_packets", stats.voice_udp_tunnel_candidate_packets},
                            {"voice_udp_requests", stats.voice_udp_requests},
                            {"voice_udp_direct_requests", stats.voice_udp_direct_requests},
                            {"voice_udp_tunnel_requests", stats.voice_udp_tunnel_requests},
                            {"discord_ip_discovery_requests", stats.discord_ip_discovery_requests},
                            {"discord_ip_discovery_other_ports", stats.discord_ip_discovery_other_ports},
                            {"voice_udp_decoys_sent", stats.voice_udp_decoys_sent},
                            {"voice_udp_decoy_failures", stats.voice_udp_decoy_failures},
                            {"passive_resets_dropped", stats.passive_resets_dropped},
                            {"tunnel_interfaces", stats.tunnel_interfaces},
                            {"strategies_changed", stats.strategies_changed}};
        const auto executable_path = current_executable_path_utf8();
        if (!executable_path.empty()) response["process_path"] = executable_path;
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
    DpiBackend dpi_;
    std::atomic<bool> running_{true};
    mutable std::mutex mutex_;
    EngineSnapshot latest_;
    bool has_latest_{};
    std::chrono::steady_clock::time_point last_diagnostic_log_{};
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
        const bool install_background_task = argc > 1 && std::string_view(argv[1]) == "--install-background-task";
        if (help) {
            std::cout << "Usage: scpdpi_service.exe [--console | --snapshot | --serve | --enable-autostart | --remove-autostart]\n"
                         "  no option  Open the interactive console\n"
                         "  --console  Open the interactive console\n"
                         "  --snapshot Print one JSON network snapshot\n"
                         "  --serve    Run the local IPC background service\n"
                         "  --enable-autostart  Enable launch when the current Windows user signs in\n"
                         "  --remove-autostart  Disable launch when the current Windows user signs in\n"
                         "  dpi on|off|status   Control Windows DPI packet processing in the console\n";
            return 0;
        }
        if (install_background_task) {
            const auto installed = scpdpi::install_elevated_background_task();
            if (!installed) { std::cerr << installed.error().message << '\n'; return 1; }
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
