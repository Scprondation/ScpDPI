#pragma once

#include "scpdpi/common/result.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace scpdpi::dpi {

struct PacketEngineStats {
    std::uint64_t packets_seen{};
    std::uint64_t tcp_443_outbound{};
    std::uint64_t tcp_443_tunnel{};
    std::uint64_t tcp_tls_candidates_outbound{};
    std::uint64_t tcp_tls_candidates_tunnel{};
    std::uint64_t tls_hellos_parsed{};
    std::uint64_t tls_hellos_unlisted{};
    std::uint64_t tls_hellos_timed_out{};
    std::uint64_t resegment_attempts{};
    std::uint64_t resegment_failures{};
    std::uint64_t packet_send_failures{};
    std::uint64_t packets_resegmented{};
    std::uint64_t domains_matched{};
    std::uint64_t discord_tls_matches{};
    std::uint64_t udp_443_candidates{};
    std::uint64_t udp_443_tunnel_candidates{};
    std::uint64_t quic_udp_443_dropped{};
    std::uint64_t discord_quic_fallbacks{};
    std::uint64_t voice_udp_candidate_packets{};
    std::uint64_t voice_udp_tunnel_candidate_packets{};
    std::uint64_t voice_udp_requests{};
    std::uint64_t voice_udp_direct_requests{};
    std::uint64_t voice_udp_tunnel_requests{};
    std::uint64_t discord_ip_discovery_requests{};
    std::uint64_t discord_ip_discovery_other_ports{};
    std::uint64_t voice_udp_decoys_sent{};
    std::uint64_t voice_udp_decoy_failures{};
    std::uint64_t passive_resets_dropped{};
    std::uint64_t tunnel_interfaces{};
    std::uint64_t strategies_changed{};
};

class PacketEngine {
public:
    PacketEngine();
    ~PacketEngine();
    PacketEngine(const PacketEngine&) = delete;
    PacketEngine& operator=(const PacketEngine&) = delete;

    [[nodiscard]] Result<void> start(const std::filesystem::path& runtime_directory);
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::string last_error() const;
    [[nodiscard]] PacketEngineStats stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace scpdpi::dpi
