#include "console_ui.hpp"

#include <cstdint>
#include <iostream>
#include <string_view>

namespace scpdpi {
namespace {
using Json = nlohmann::json;

void print_error(const Json& response) {
    std::cout << "Error: " << response.value("error", std::string("request failed")) << '\n';
}

Json request_snapshot(const ConsoleUiCallbacks& callbacks, const std::string& command) {
    const Json response = callbacks.request(command, Json::object());
    if (!response.value("ok", false)) {
        print_error(response);
        return Json::object();
    }
    return response;
}

void print_summary(const Json& response) {
    if (!response.value("ok", false)) {
        print_error(response);
        return;
    }
    std::cout << "Core: " << (response.value("running", false) ? "running" : "stopped")
              << "  |  Profile: " << response.value("profile", std::string("default")) << '\n';
    const auto dpi = response.value("dpi", Json::object());
    std::cout << "DPI: " << (dpi.value("running", false) ? "running" : "stopped")
              << (dpi.value("enabled", false) ? "  |  enabled" : "  |  disabled") << '\n';
    if (!dpi.value("message", std::string{}).empty())
        std::cout << "  " << dpi.value("message", std::string{}) << '\n';
    if (dpi.value("packets_seen", std::uint64_t{}) != 0U || dpi.value("domains_matched", std::uint64_t{}) != 0U) {
        std::cout << "  Packets inspected: " << dpi.value("packets_seen", std::uint64_t{})
                  << "  |  outbound TLS: " << dpi.value("tcp_443_outbound", std::uint64_t{})
                  << "  (tunnel: " << dpi.value("tcp_443_tunnel", std::uint64_t{}) << ")"
                  << "  |  ClientHello parsed: " << dpi.value("tls_hellos_parsed", std::uint64_t{}) << '\n'
                  << "  Listed TLS hosts: " << dpi.value("domains_matched", std::uint64_t{})
                  << "  |  unlisted: " << dpi.value("tls_hellos_unlisted", std::uint64_t{})
                  << "  |  timed out: " << dpi.value("tls_hellos_timed_out", std::uint64_t{})
                  << "  |  split attempts/failures: " << dpi.value("resegment_attempts", std::uint64_t{})
                  << '/' << dpi.value("resegment_failures", std::uint64_t{})
                  << "  |  reinject failures: " << dpi.value("packet_send_failures", std::uint64_t{})
                  << "  |  split packets: " << dpi.value("packets_resegmented", std::uint64_t{})
                  << "  |  Discord TLS matches: " << dpi.value("discord_tls_matches", std::uint64_t{})
                  << "  |  YouTube TLS matches: " << dpi.value("youtube_tls_matches", std::uint64_t{})
                  << "  |  TLS outcomes (rx/alert/reset/timeout): D "
                  << dpi.value("discord_tls_responses", std::uint64_t{}) << '/'
                  << dpi.value("discord_tls_alerts", std::uint64_t{}) << '/'
                  << dpi.value("discord_tls_resets", std::uint64_t{}) << '/'
                  << dpi.value("discord_tls_response_timeouts", std::uint64_t{}) << "  YT "
                  << dpi.value("youtube_tls_responses", std::uint64_t{}) << '/'
                  << dpi.value("youtube_tls_alerts", std::uint64_t{}) << '/'
                  << dpi.value("youtube_tls_resets", std::uint64_t{}) << '/'
                  << dpi.value("youtube_tls_response_timeouts", std::uint64_t{}) << '\n'
                  << "  |  listed QUIC fallbacks: " << dpi.value("quic_udp_443_dropped", std::uint64_t{})
                  << "  (Discord: " << dpi.value("discord_quic_fallbacks", std::uint64_t{}) << ')'
                  << "  (YouTube: " << dpi.value("youtube_quic_fallbacks", std::uint64_t{}) << ')'
                  << "  |  filtered passive resets: " << dpi.value("passive_resets_dropped", std::uint64_t{})
                  << "  |  Tunnel interfaces: " << dpi.value("tunnel_interfaces", std::uint64_t{}) << '\n'
                  << "  TLS ports inspected: " << dpi.value("tcp_tls_candidates_outbound", std::uint64_t{})
                  << "  (tunnel: " << dpi.value("tcp_tls_candidates_tunnel", std::uint64_t{}) << ')'
                  << "  |  UDP/443 candidates: " << dpi.value("udp_443_candidates", std::uint64_t{})
                  << "  (tunnel: " << dpi.value("udp_443_tunnel_candidates", std::uint64_t{}) << ')'
                  << "  |  Voice UDP port candidates: " << dpi.value("voice_udp_candidate_packets", std::uint64_t{})
                  << "  (tunnel: " << dpi.value("voice_udp_tunnel_candidate_packets", std::uint64_t{}) << ')'
                  << "  |  Discord discovery/STUN requests: " << dpi.value("voice_udp_requests", std::uint64_t{})
                  << "  (direct/tunnel: " << dpi.value("voice_udp_direct_requests", std::uint64_t{})
                  << '/' << dpi.value("voice_udp_tunnel_requests", std::uint64_t{}) << ')'
                  << "  |  Discord IP discovery: " << dpi.value("discord_ip_discovery_requests", std::uint64_t{})
                  << "  (responses: " << dpi.value("discord_ip_discovery_responses", std::uint64_t{}) << ')'
                  << "  (other UDP ports: " << dpi.value("discord_ip_discovery_other_ports", std::uint64_t{}) << ')'
                  << "  |  UDP decoys sent/failed: " << dpi.value("voice_udp_decoys_sent", std::uint64_t{})
                  << '/' << dpi.value("voice_udp_decoy_failures", std::uint64_t{}) << '\n';
    }
    const auto snapshot = response.value("snapshot", Json::object());
    if (!snapshot.is_object()) {
        std::cout << "No network snapshot is available yet.\n";
        return;
    }
    const auto interfaces = snapshot.value("interfaces", Json::array());
    const auto routes = snapshot.value("routes", Json::array());
    const auto vpns = snapshot.value("vpn_interfaces", Json::array());
    const auto telemetry = snapshot.value("telemetry", Json::object());
    std::cout << "Interfaces: " << interfaces.size() << "  |  Routes: " << routes.size()
              << "  |  VPN candidates: " << vpns.size()
              << "  |  Warnings: " << telemetry.value("warning_count", 0) << '\n';
}

void print_interfaces(const Json& snapshot) {
    const auto interfaces = snapshot.value("interfaces", Json::array());
    if (interfaces.empty()) {
        std::cout << "No network interfaces were reported.\n";
        return;
    }
    for (const auto& iface : interfaces) {
        std::cout << "\n" << iface.value("name", std::string("(unnamed)"))
                  << "  [" << iface.value("kind", std::string("unknown")) << "]"
                  << (iface.value("up", false) ? " up" : " down")
                  << (iface.value("virtual", false) ? " virtual" : " physical") << '\n';
        const auto description = iface.value("description", std::string{});
        if (!description.empty()) std::cout << "  " << description << '\n';
        const auto mac = iface.value("mac", std::string{});
        std::cout << "  MTU " << iface.value("mtu", 0U)
                  << "  MAC " << (mac.empty() ? "unavailable" : mac) << '\n';
        const auto addresses = iface.value("addresses", Json::array());
        if (addresses.empty()) std::cout << "  Addresses: none\n";
        for (const auto& address : addresses)
            std::cout << "  " << address.value("family", std::string("ip")) << ": "
                      << address.value("address", std::string("unavailable")) << '\n';
    }
}

void print_routes(const Json& snapshot) {
    const auto routes = snapshot.value("routes", Json::array());
    if (routes.empty()) {
        std::cout << "No routes were reported.\n";
        return;
    }
    for (const auto& route : routes) {
        const auto gateway = route.value("gateway", std::string{});
        std::cout << (route.value("default", false) ? "default" : "route") << "  "
                  << route.value("destination", std::string("?")) << '/'
                  << route.value("prefix", 0U) << "  via "
                  << (gateway.empty() ? "on-link" : gateway)
                  << "  dev " << route.value("interface", std::string("?"))
                  << "  metric " << route.value("metric", 0U) << '\n';
    }
}

void print_vpn(const Json& snapshot) {
    const auto vpns = snapshot.value("vpn_interfaces", Json::array());
    if (vpns.empty()) {
        std::cout << "No VPN interface evidence detected. This does not identify VPN protocols.\n";
        return;
    }
    for (const auto& vpn : vpns) {
        std::cout << vpn.value("name", std::string("(unnamed)")) << "  confidence: "
                  << vpn.value("confidence", std::string("unknown")) << '\n';
        for (const auto& evidence : vpn.value("evidence", Json::array()))
            if (evidence.is_string()) std::cout << "  - " << evidence.get<std::string>() << '\n';
    }
    std::cout << "Interface evidence cannot establish the VPN protocol.\n";
}

void print_diagnostics(const Json& snapshot) {
    const auto diagnostics = snapshot.value("diagnostics", Json::array());
    if (diagnostics.empty()) {
        std::cout << "No diagnostic findings.\n";
        return;
    }
    for (const auto& finding : diagnostics)
        std::cout << '[' << finding.value("severity", std::string("info")) << "] "
                  << finding.value("code", std::string("unknown")) << ": "
                  << finding.value("message", std::string{}) << '\n';
}

void print_help() {
    std::cout << "Commands:\n"
                 "  status                 Show Core state and latest scan counts\n"
                 "  scan                   Run a fresh read-only network scan\n"
                 "  interfaces             Show detected interfaces and addresses\n"
                 "  routes                 Show the current routing table\n"
                 "  vpn                    Show VPN interface evidence and confidence\n"
                 "  diagnostics            Show routing and interface findings\n"
                 "  profile <name>         Select default, vpn-safe, diagnostic, or custom\n"
                 "  start | stop           Start or stop periodic scans\n"
                 "  autostart on | off     Enable or disable launch when you sign in to Windows\n"
                 "  dpi on | off | status  Start, stop, or check DPI packet processing\n"
                 "  help                   Show this list\n"
                 "  quit                   Exit the console\n";
}

void print_report(const ConsoleUiCallbacks& callbacks, std::string_view section) {
    const auto response = request_snapshot(callbacks, "scan");
    if (response.empty()) return;
    print_summary(response);
    const auto snapshot = response.value("snapshot", Json::object());
    if (!snapshot.is_object()) {
        std::cout << "No network snapshot is available.\n";
        return;
    }
    std::cout << '\n';
    if (section == "interfaces") print_interfaces(snapshot);
    else if (section == "routes") print_routes(snapshot);
    else if (section == "vpn") print_vpn(snapshot);
    else if (section == "diagnostics") print_diagnostics(snapshot);
}

} // namespace

int run_console(const ConsoleUiCallbacks& callbacks) {
    std::cout << "ScpDPI 0.1.0 — network console and DPI processor\n";
    print_summary(request_snapshot(callbacks, "status"));
    std::cout << "Type 'help' for commands. First run needs one-time administrator approval.\n\n";

    std::string line;
    while (std::cout << "scpdpi> " && std::getline(std::cin, line)) {
        if (line.size() > 512U) {
            std::cout << "Input is too long (limit: 512 characters).\n";
            continue;
        }
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        const auto end = line.find_first_of(" \t", first);
        const std::string command = line.substr(first, end == std::string::npos ? end : end - first);
        const auto arg_start = end == std::string::npos ? end : line.find_first_not_of(" \t", end);
        const std::string argument = arg_start == std::string::npos ? std::string{} : line.substr(arg_start);

        if (command == "quit" || command == "exit") break;
        if (command == "help") { print_help(); continue; }
        if (command == "status") {
            print_summary(request_snapshot(callbacks, "status"));
            continue;
        }
        if (command == "scan" || command == "interfaces" || command == "routes" ||
            command == "vpn" || command == "diagnostics") {
            print_report(callbacks, command == "scan" ? std::string_view{} : std::string_view(command));
            continue;
        }
        if (command == "start" || command == "stop") {
            const auto response = callbacks.request(command, Json::object());
            if (!response.value("ok", false)) print_error(response);
            else {
                std::cout << "Periodic scans " << (response.value("running", false) ? "started" : "stopped") << ".\n";
            }
            continue;
        }
        if (command == "profile") {
            if (argument != "default" && argument != "vpn-safe" && argument != "diagnostic" && argument != "custom") {
                std::cout << "Choose default, vpn-safe, diagnostic, or custom.\n";
                continue;
            }
            const auto response = callbacks.request("set_profile", Json{{"profile", argument}});
            if (!response.value("ok", false)) print_error(response);
            else std::cout << "Active profile: " << response.value("profile", argument) << '\n';
            continue;
        }
        if (command == "autostart") {
            if (argument != "on" && argument != "off") {
                std::cout << "Use 'autostart on' or 'autostart off'.\n";
                continue;
            }
            const auto response = callbacks.request(argument == "on" ? "enable_autostart" : "disable_autostart", Json::object());
            if (!response.value("ok", false)) print_error(response);
            else if (argument == "on") std::cout << "Windows autostart enabled.\n";
            else std::cout << "Windows autostart disabled. ScpDPI keeps running until Windows shuts down.\n";
            continue;
        }
        if (command == "dpi") {
            if (argument == "status") {
                print_summary(request_snapshot(callbacks, "status"));
                continue;
            }
            if (argument != "on" && argument != "off") {
                std::cout << "Use 'dpi on', 'dpi off', or 'dpi status'.\n";
                continue;
            }
            const auto response = callbacks.request(argument == "on" ? "enable_dpi" : "disable_dpi", Json::object());
            if (!response.value("ok", false)) print_error(response);
            else std::cout << "DPI packet processing " << (argument == "on" ? "enabled" : "disabled") << ".\n";
            continue;
        }
        std::cout << "Unknown command. Type 'help' to see available commands.\n";
    }

    std::cout << "ScpDPI console closed.\n";
    return 0;
}

} // namespace scpdpi
