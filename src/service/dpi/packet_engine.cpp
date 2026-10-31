#include "packet_engine.hpp"

#include "host_lists.hpp"
#include "tls_client_hello.hpp"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <windivert.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <limits>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace scpdpi::dpi {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t max_hello_bytes = 64U * 1024U;
constexpr std::size_t max_pending_flows = 512U;
constexpr std::size_t max_segment_payload = 1200U;
constexpr auto pending_timeout = std::chrono::milliseconds(1800);
constexpr auto response_timeout = std::chrono::seconds(8);

std::string windows_error(const char* action, DWORD code) {
    return std::string(action) + " (Windows error " + std::to_string(code) + ")";
}

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

bool looks_like_tunnel(const IP_ADAPTER_ADDRESSES& adapter) {
    if (adapter.IfType == IF_TYPE_TUNNEL) return true;
    std::wstring description = adapter.Description == nullptr ? L"" : adapter.Description;
    std::wstring friendly = adapter.FriendlyName == nullptr ? L"" : adapter.FriendlyName;
    description = lower(std::move(description));
    friendly = lower(std::move(friendly));
    constexpr std::array<std::wstring_view, 7> hints{
        L"wintun", L"wireguard", L"vpn", L"tunnel", L"tun_", L"tap-", L"incy"
    };
    for (const auto hint : hints) {
        if (description.find(hint) != std::wstring::npos || friendly.find(hint) != std::wstring::npos) return true;
    }
    return false;
}

std::unordered_set<UINT32> tunnel_interfaces() {
    ULONG size = 16U * 1024U;
    std::vector<std::byte> storage(size);
    ULONG result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr,
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(storage.data()), &size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        storage.resize(size);
        result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr,
            reinterpret_cast<PIP_ADAPTER_ADDRESSES>(storage.data()), &size);
    }
    std::unordered_set<UINT32> indices;
    if (result != NO_ERROR) return indices;
    for (auto* adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(storage.data()); adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        if (!looks_like_tunnel(*adapter)) continue;
        if (adapter->IfIndex != 0U) indices.insert(adapter->IfIndex);
        if (adapter->Ipv6IfIndex != 0U) indices.insert(adapter->Ipv6IfIndex);
    }
    return indices;
}

struct ParsedPacket {
    PWINDIVERT_IPHDR ipv4{};
    PWINDIVERT_IPV6HDR ipv6{};
    PWINDIVERT_TCPHDR tcp{};
    PWINDIVERT_UDPHDR udp{};
    const std::uint8_t* payload{};
    UINT payload_length{};
    std::size_t payload_offset{};
    std::size_t ipv4_offset{};
    std::size_t ipv6_offset{};
    std::size_t tcp_offset{};
};

bool parse_packet(std::vector<std::uint8_t>& packet, ParsedPacket& parsed) {
    UINT8 protocol = 0;
    PVOID payload = nullptr;
    UINT payload_length = 0;
    if (!WinDivertHelperParsePacket(packet.data(), static_cast<UINT>(packet.size()),
            &parsed.ipv4, &parsed.ipv6, &protocol, nullptr, nullptr, &parsed.tcp, &parsed.udp,
            &payload, &payload_length, nullptr, nullptr)) return false;
    parsed.payload = static_cast<const std::uint8_t*>(payload);
    parsed.payload_length = payload_length;
    if (payload != nullptr) parsed.payload_offset = static_cast<std::size_t>(parsed.payload - packet.data());
    if (parsed.ipv4 != nullptr) parsed.ipv4_offset = static_cast<std::size_t>(reinterpret_cast<std::uint8_t*>(parsed.ipv4) - packet.data());
    if (parsed.ipv6 != nullptr) parsed.ipv6_offset = static_cast<std::size_t>(reinterpret_cast<std::uint8_t*>(parsed.ipv6) - packet.data());
    if (parsed.tcp != nullptr) parsed.tcp_offset = static_cast<std::size_t>(reinterpret_cast<std::uint8_t*>(parsed.tcp) - packet.data());
    return parsed.ipv4 != nullptr || parsed.ipv6 != nullptr;
}

std::string flow_key(const ParsedPacket& packet) {
    if (packet.tcp == nullptr || (packet.ipv4 == nullptr && packet.ipv6 == nullptr)) return {};
    std::string key;
    key.reserve(packet.ipv6 != nullptr ? 37U : 13U);
    key.push_back(packet.ipv6 != nullptr ? '6' : '4');
    if (packet.ipv4 != nullptr) {
        key.append(reinterpret_cast<const char*>(&packet.ipv4->SrcAddr), sizeof(packet.ipv4->SrcAddr));
        key.append(reinterpret_cast<const char*>(&packet.ipv4->DstAddr), sizeof(packet.ipv4->DstAddr));
    } else {
        key.append(reinterpret_cast<const char*>(packet.ipv6->SrcAddr), sizeof(packet.ipv6->SrcAddr));
        key.append(reinterpret_cast<const char*>(packet.ipv6->DstAddr), sizeof(packet.ipv6->DstAddr));
    }
    key.append(reinterpret_cast<const char*>(&packet.tcp->SrcPort), sizeof(packet.tcp->SrcPort));
    key.append(reinterpret_cast<const char*>(&packet.tcp->DstPort), sizeof(packet.tcp->DstPort));
    return key;
}

std::string reverse_key(std::string key) {
    if (key.empty()) return key;
    const std::size_t address_size = key.front() == '6' ? 16U : 4U;
    const std::size_t source_offset = 1U;
    const std::size_t destination_offset = source_offset + address_size;
    const std::size_t ports_offset = destination_offset + address_size;
    for (std::size_t i = 0; i < address_size; ++i) std::swap(key[source_offset + i], key[destination_offset + i]);
    std::swap(key[ports_offset], key[ports_offset + 2U]);
    std::swap(key[ports_offset + 1U], key[ports_offset + 3U]);
    return key;
}

struct CapturedPacket {
    std::vector<std::uint8_t> bytes;
    WINDIVERT_ADDRESS address{};
    std::uint32_t sequence{};
    std::size_t payload_offset{};
    std::size_t payload_length{};
    bool fin{};
    bool push{};
};

struct PendingHello {
    std::uint32_t base_sequence{};
    std::vector<std::uint8_t> stream;
    std::vector<CapturedPacket> packets;
    Clock::time_point touched{Clock::now()};
    bool fin{};
    bool push{};
};

struct StrategyState {
    std::size_t next{};
    std::size_t best{};
    bool has_best{};
};

struct Attempt {
    std::string host;
    std::size_t strategy{};
    Clock::time_point started{Clock::now()};
};

std::vector<std::size_t> strategy_cuts(std::size_t sni_offset, std::size_t sni_length, std::size_t stream_length,
                                       std::size_t strategy) {
    std::vector<std::size_t> cuts;
    if (sni_offset == 0U || sni_offset >= stream_length || sni_length == 0U) return cuts;
    if (strategy == 0U) {
        cuts = {sni_offset, sni_offset + std::max<std::size_t>(1U, sni_length / 2U)};
    } else if (strategy == 1U) {
        cuts = {sni_offset + 1U, sni_offset + sni_length};
    } else {
        cuts = {1U, sni_offset, sni_offset + 1U, sni_offset + sni_length};
    }
    std::erase_if(cuts, [stream_length](std::size_t value) { return value == 0U || value >= stream_length; });
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    return cuts;
}

} // namespace

struct PacketEngine::Impl {
    HANDLE handle{INVALID_HANDLE_VALUE};
    std::jthread worker;
    std::jthread sweeper;
    std::atomic<bool> active{};
    mutable std::mutex error_mutex;
    std::string error;
    std::mutex flow_mutex;
    std::mutex send_mutex;
    mutable std::mutex tunnel_mutex;
    HostLists hosts;
    std::unordered_set<UINT32> tunnels;
    std::unordered_map<std::string, PendingHello> pending;
    std::unordered_map<std::string, Attempt> attempts;
    std::unordered_map<std::string, StrategyState> strategies;
    std::atomic<std::uint64_t> packets_seen{};
    std::atomic<std::uint64_t> tcp_443_outbound{};
    std::atomic<std::uint64_t> tcp_443_tunnel{};
    std::atomic<std::uint64_t> tls_hellos_parsed{};
    std::atomic<std::uint64_t> tls_hellos_unlisted{};
    std::atomic<std::uint64_t> tls_hellos_timed_out{};
    std::atomic<std::uint64_t> resegment_attempts{};
    std::atomic<std::uint64_t> resegment_failures{};
    std::atomic<std::uint64_t> packet_send_failures{};
    std::atomic<std::uint64_t> packets_resegmented{};
    std::atomic<std::uint64_t> domains_matched{};
    std::atomic<std::uint64_t> quic_udp_443_dropped{};
    std::atomic<std::uint64_t> tunnel_interfaces_count{};
    std::atomic<std::uint64_t> strategies_changed{};

    void set_error(std::string message) {
        std::lock_guard lock(error_mutex);
        error = std::move(message);
    }

    bool send_packet(std::span<const std::uint8_t> bytes, const WINDIVERT_ADDRESS& address) {
        if (bytes.empty()) return true;
        std::lock_guard lock(send_mutex);
        if (!WinDivertSend(handle, bytes.data(), static_cast<UINT>(bytes.size()), nullptr, &address)) {
            const DWORD code = GetLastError();
            packet_send_failures.fetch_add(1U, std::memory_order_relaxed);
            set_error(windows_error("ScpDPI could not reinject a network packet", code));
            return false;
        }
        return true;
    }

    bool is_tunnel(const WINDIVERT_ADDRESS& address) const {
        if (address.Network.IfIdx == 0U) return false;
        std::lock_guard lock(tunnel_mutex);
        return tunnels.contains(address.Network.IfIdx);
    }

    bool has_tunnels() const {
        std::lock_guard lock(tunnel_mutex);
        return !tunnels.empty();
    }

    void refresh_tunnels() {
        auto current = tunnel_interfaces();
        const auto count = static_cast<std::uint64_t>(current.size());
        {
            std::lock_guard lock(tunnel_mutex);
            tunnels = std::move(current);
        }
        tunnel_interfaces_count.store(count, std::memory_order_relaxed);
    }

    bool send_original(const CapturedPacket& captured) {
        return send_packet(captured.bytes, captured.address);
    }

    void release_hello(PendingHello& hello) {
        std::sort(hello.packets.begin(), hello.packets.end(), [base = hello.base_sequence](const auto& left, const auto& right) {
            return static_cast<std::int32_t>(left.sequence - base) < static_cast<std::int32_t>(right.sequence - base);
        });
        for (const auto& packet : hello.packets) (void)send_original(packet);
    }

    void fail_attempt(const std::string& key) {
        const auto attempt = attempts.find(key);
        if (attempt == attempts.end()) return;
        auto& state = strategies[attempt->second.host];
        if (!state.has_best) state.next = (attempt->second.strategy + 1U) % 3U;
        strategies_changed.fetch_add(1U, std::memory_order_relaxed);
        attempts.erase(attempt);
    }

    void succeed_attempt(const std::string& key) {
        const auto attempt = attempts.find(key);
        if (attempt == attempts.end()) return;
        auto& state = strategies[attempt->second.host];
        state.best = attempt->second.strategy;
        state.has_best = true;
        state.next = state.best;
        attempts.erase(attempt);
    }

    void observe_inbound(const std::string& key, const ParsedPacket& packet) {
        const auto outgoing_key = reverse_key(key);
        const auto attempt = attempts.find(outgoing_key);
        if (attempt == attempts.end()) return;
        if (packet.tcp->Rst != 0U) {
            fail_attempt(outgoing_key);
            return;
        }
        if (packet.payload_length == 0U || packet.payload == nullptr) return;
        if (packet.payload[0] == 0x15U) fail_attempt(outgoing_key);
        else if (packet.payload[0] == 0x16U || packet.payload[0] == 0x17U) succeed_attempt(outgoing_key);
    }

    std::vector<std::vector<std::uint8_t>> split_stream(const CapturedPacket& first, const PendingHello& hello,
            const std::vector<std::size_t>& cuts, std::size_t strategy) {
        std::vector<std::size_t> boundaries{0U};
        for (const auto cut : cuts) boundaries.push_back(cut);
        boundaries.push_back(hello.stream.size());
        std::vector<std::size_t> chunks;
        for (std::size_t index = 1U; index < boundaries.size(); ++index) {
            for (std::size_t position = boundaries[index - 1U]; position < boundaries[index];) {
                position = std::min(position + max_segment_payload, boundaries[index]);
                if (chunks.empty() || chunks.back() != position) chunks.push_back(position);
            }
        }
        if (chunks.empty() || chunks.back() != hello.stream.size()) chunks.push_back(hello.stream.size());

        std::vector<std::vector<std::uint8_t>> packets;
        packets.reserve(chunks.size());
        std::size_t begin = 0U;
        auto original = first.bytes;
        ParsedPacket original_headers;
        if (!parse_packet(original, original_headers) || original_headers.tcp == nullptr ||
            original_headers.payload_offset != first.payload_offset) return {};
        const std::size_t tcp_header_length = static_cast<std::size_t>(original_headers.tcp->HdrLength) * 4U;
        if (tcp_header_length < 20U || original_headers.tcp_offset + tcp_header_length > first.payload_offset) return {};
        for (std::size_t index = 0U; index < chunks.size(); ++index) {
            const std::size_t end = chunks[index];
            if (end <= begin || end > hello.stream.size()) return {};
            std::vector<std::uint8_t> packet(first.bytes.begin(),
                first.bytes.begin() + static_cast<std::ptrdiff_t>(first.payload_offset));
            packet.insert(packet.end(), hello.stream.begin() + static_cast<std::ptrdiff_t>(begin),
                          hello.stream.begin() + static_cast<std::ptrdiff_t>(end));
            std::uint32_t sequence = WinDivertHelperHtonl(hello.base_sequence + static_cast<std::uint32_t>(begin));
            std::memcpy(packet.data() + original_headers.tcp_offset + offsetof(WINDIVERT_TCPHDR, SeqNum), &sequence, sizeof(sequence));
            auto& flags = packet[original_headers.tcp_offset + 13U];
            flags = static_cast<std::uint8_t>(flags & static_cast<std::uint8_t>(~0x09U));
            if (index + 1U == chunks.size() && hello.fin) flags = static_cast<std::uint8_t>(flags | 0x01U);
            if (index + 1U == chunks.size() && hello.push) flags = static_cast<std::uint8_t>(flags | 0x08U);
            if (original_headers.ipv4 != nullptr) {
                const std::size_t header_length = static_cast<std::size_t>(original_headers.ipv4->HdrLength) * 4U;
                if (header_length < 20U || packet.size() < original_headers.ipv4_offset + header_length ||
                    packet.size() - original_headers.ipv4_offset - header_length > std::numeric_limits<std::uint16_t>::max()) return {};
                const auto ip_length = WinDivertHelperHtons(static_cast<std::uint16_t>(packet.size() - original_headers.ipv4_offset - header_length));
                std::memcpy(packet.data() + original_headers.ipv4_offset + offsetof(WINDIVERT_IPHDR, Length), &ip_length, sizeof(ip_length));
            } else {
                if (original_headers.ipv6 == nullptr || packet.size() < original_headers.ipv6_offset + sizeof(WINDIVERT_IPV6HDR) ||
                    packet.size() - original_headers.ipv6_offset - sizeof(WINDIVERT_IPV6HDR) > std::numeric_limits<std::uint16_t>::max()) return {};
                const auto ipv6_length = WinDivertHelperHtons(static_cast<std::uint16_t>(packet.size() - original_headers.ipv6_offset - sizeof(WINDIVERT_IPV6HDR)));
                std::memcpy(packet.data() + original_headers.ipv6_offset + offsetof(WINDIVERT_IPV6HDR, Length), &ipv6_length, sizeof(ipv6_length));
            }
            WINDIVERT_ADDRESS address = first.address;
            if (!WinDivertHelperCalcChecksums(packet.data(), static_cast<UINT>(packet.size()), &address, 0U)) return {};
            packets.push_back(std::move(packet));
            begin = end;
        }
        (void)strategy;
        return packets;
    }

    void adapt_or_pass(const std::string& key, PendingHello& hello, const TlsClientHello& parsed_hello) {
        const CapturedPacket& first = hello.packets.front();
        if (!hosts.matches(parsed_hello.hostname)) {
            tls_hellos_unlisted.fetch_add(1U, std::memory_order_relaxed);
            release_hello(hello);
            return;
        }
        domains_matched.fetch_add(1U, std::memory_order_relaxed);
        auto& state = strategies[parsed_hello.hostname];
        const std::size_t strategy = state.has_best ? state.best : state.next;
        const auto cuts = strategy_cuts(parsed_hello.sni_offset, parsed_hello.sni_length, hello.stream.size(), strategy);
        auto segments = split_stream(first, hello, cuts, strategy);
        if (cuts.empty() || segments.empty()) {
            resegment_failures.fetch_add(1U, std::memory_order_relaxed);
            set_error(cuts.empty() ? "ScpDPI could not find a safe TLS split boundary" : "ScpDPI could not build valid split TLS packets");
            release_hello(hello);
            return;
        }
        resegment_attempts.fetch_add(1U, std::memory_order_relaxed);
        for (std::size_t index = 0U; index < segments.size(); ++index) {
            WINDIVERT_ADDRESS address = first.address;
            if (!send_packet(segments[index], address)) {
                resegment_failures.fetch_add(1U, std::memory_order_relaxed);
                // Preserve connectivity if injection of a crafted segment fails. Any
                // already sent prefix is safe to retransmit because TCP deduplicates it.
                release_hello(hello);
                return;
            }
            packets_resegmented.fetch_add(1U, std::memory_order_relaxed);
        }
        attempts[key] = Attempt{parsed_hello.hostname, strategy, Clock::now()};
    }

    void handle_outbound_tcp(std::vector<std::uint8_t>& bytes, WINDIVERT_ADDRESS& address, const ParsedPacket& parsed) {
        if (parsed.tcp == nullptr || parsed.payload_length == 0U || parsed.payload == nullptr || parsed.tcp->Syn != 0U ||
            parsed.tcp->Fin != 0U || parsed.tcp->Rst != 0U || parsed.tcp->Urg != 0U) {
            (void)send_packet(bytes, address);
            return;
        }
        const std::string key = flow_key(parsed);
        if (key.empty()) { (void)send_packet(bytes, address); return; }
        const auto sequence = WinDivertHelperNtohl(parsed.tcp->SeqNum);
        const bool fin = parsed.tcp->Fin != 0U;
        const bool push = parsed.tcp->Psh != 0U;
        const std::size_t payload_offset = parsed.payload_offset;
        const std::size_t payload_length = parsed.payload_length;
        const auto* payload = parsed.payload;
        std::lock_guard lock(flow_mutex);
        auto found = pending.find(key);
        if (found == pending.end()) {
            if (payload[0] != 0x16U || pending.size() >= max_pending_flows || payload_offset > bytes.size()) {
                (void)send_packet(bytes, address);
                return;
            }
            PendingHello hello;
            hello.base_sequence = sequence;
            hello.stream.assign(payload, payload + payload_length);
            hello.fin = fin;
            hello.push = push;
            hello.touched = Clock::now();
            hello.packets.push_back({std::move(bytes), address, sequence, payload_offset, payload_length, hello.fin, hello.push});
            found = pending.emplace(key, std::move(hello)).first;
        } else {
            auto& hello = found->second;
            const auto offset = static_cast<std::int32_t>(sequence - hello.base_sequence);
            if (offset < 0 || static_cast<std::size_t>(offset) > max_hello_bytes ||
                payload_length > max_hello_bytes - static_cast<std::size_t>(offset)) {
                release_hello(hello);
                pending.erase(found);
                (void)send_packet(bytes, address);
                return;
            }
            const std::size_t start = static_cast<std::size_t>(offset);
            const std::size_t end = start + payload_length;
            if (start < hello.stream.size()) {
                const std::size_t overlap = std::min(hello.stream.size() - start, payload_length);
                if (!std::equal(payload, payload + overlap, hello.stream.begin() + static_cast<std::ptrdiff_t>(start)) ||
                    end > hello.stream.size()) {
                    release_hello(hello);
                    pending.erase(found);
                    (void)send_packet(bytes, address);
                    return;
                }
                // A retransmission of held bytes must not be delivered twice.
                if (end <= hello.stream.size()) {
                    hello.touched = Clock::now();
                    return;
                }
            }
            if (start != hello.stream.size()) {
                release_hello(hello);
                pending.erase(found);
                (void)send_packet(bytes, address);
                return;
            }
            hello.stream.insert(hello.stream.end(), payload, payload + payload_length);
            hello.packets.push_back({std::move(bytes), address, sequence, payload_offset, payload_length, fin, push});
            hello.fin = hello.fin || fin;
            hello.push = hello.push || push;
            hello.touched = Clock::now();
        }

        auto& hello = found->second;
        const auto parse_result = parse_tls_client_hello(hello.stream);
        if (parse_result.state == TlsParseState::incomplete) return;
        if (parse_result.state == TlsParseState::invalid) {
            tls_hellos_unlisted.fetch_add(1U, std::memory_order_relaxed);
            release_hello(hello);
            pending.erase(found);
            return;
        }
        tls_hellos_parsed.fetch_add(1U, std::memory_order_relaxed);
        if (parse_result.hello.hostname.empty()) {
            tls_hellos_unlisted.fetch_add(1U, std::memory_order_relaxed);
            release_hello(hello);
            pending.erase(found);
            return;
        }
        adapt_or_pass(key, hello, parse_result.hello);
        pending.erase(found);
    }

    void process_packet(std::vector<std::uint8_t>& bytes, WINDIVERT_ADDRESS& address) {
        packets_seen.fetch_add(1U, std::memory_order_relaxed);
        ParsedPacket parsed;
        if (!parse_packet(bytes, parsed)) { (void)send_packet(bytes, address); return; }

        const bool tunnel = is_tunnel(address);
        if (parsed.udp != nullptr && address.Outbound != 0U &&
            WinDivertHelperNtohs(parsed.udp->DstPort) == 443U) {
            // Drop QUIC inside the VPN so apps retry over the tunnel with TCP/TLS.
            // Keep physical UDP/443 intact whenever a tunnel exists: it can carry
            // the VPN's own transport and must not be mistaken for application QUIC.
            if (tunnel || !has_tunnels()) quic_udp_443_dropped.fetch_add(1U, std::memory_order_relaxed);
            else (void)send_packet(bytes, address);
            return;
        }
        if (parsed.tcp == nullptr) { (void)send_packet(bytes, address); return; }
        if (address.Outbound == 0U) {
            const auto key = flow_key(parsed);
            if (!key.empty()) {
                std::lock_guard lock(flow_mutex);
                observe_inbound(key, parsed);
            }
            (void)send_packet(bytes, address);
            return;
        }
        if (WinDivertHelperNtohs(parsed.tcp->DstPort) != 443U) {
            (void)send_packet(bytes, address);
            return;
        }
        tcp_443_outbound.fetch_add(1U, std::memory_order_relaxed);
        if (tunnel) tcp_443_tunnel.fetch_add(1U, std::memory_order_relaxed);
        handle_outbound_tcp(bytes, address, parsed);
    }

    void expire(Clock::time_point now) {
        std::lock_guard lock(flow_mutex);
        for (auto it = pending.begin(); it != pending.end();) {
            if (now - it->second.touched >= pending_timeout) {
                tls_hellos_timed_out.fetch_add(1U, std::memory_order_relaxed);
                release_hello(it->second);
                it = pending.erase(it);
            } else ++it;
        }
        for (auto it = attempts.begin(); it != attempts.end();) {
            if (now - it->second.started >= response_timeout) {
                const std::string key = it->first;
                ++it;
                fail_attempt(key);
            } else ++it;
        }
    }

    void run(std::stop_token stop) {
        std::vector<std::uint8_t> bytes(WINDIVERT_MTU_MAX);
        while (!stop.stop_requested()) {
            UINT received = 0U;
            WINDIVERT_ADDRESS address{};
            if (!WinDivertRecv(handle, bytes.data(), static_cast<UINT>(bytes.size()), &received, &address)) {
                const DWORD code = GetLastError();
                if (stop.stop_requested() || code == ERROR_NO_DATA || code == ERROR_OPERATION_ABORTED) break;
                set_error(windows_error("ScpDPI packet reception failed", code));
                break;
            }
            if (received == 0U || received > bytes.size()) continue;
            bytes.resize(received);
            process_packet(bytes, address);
            bytes.resize(WINDIVERT_MTU_MAX);
        }
        active.store(false, std::memory_order_release);
    }

    Result<void> start(const std::filesystem::path& runtime_directory) {
        if (active.load(std::memory_order_acquire)) return Result<void>::success();
        const auto list_result = hosts.load(runtime_directory);
        if (!list_result) { set_error(list_result.error().message); return list_result; }
        refresh_tunnels();
        constexpr char filter[] = "!impostor and !loopback and (tcp.DstPort == 443 or tcp.SrcPort == 443 or (outbound and udp.DstPort == 443))";
        handle = WinDivertOpen(filter, WINDIVERT_LAYER_NETWORK, WINDIVERT_PRIORITY_HIGHEST, 0U);
        if (handle == INVALID_HANDLE_VALUE) {
            const DWORD code = GetLastError();
            const std::string message = windows_error("ScpDPI could not open its Windows packet filter; administrator access and the WinDivert driver are required", code);
            set_error(message);
            return Result<void>::failure({message, static_cast<int>(code)});
        }
        (void)WinDivertSetParam(handle, WINDIVERT_PARAM_QUEUE_LENGTH, WINDIVERT_PARAM_QUEUE_LENGTH_MAX);
        (void)WinDivertSetParam(handle, WINDIVERT_PARAM_QUEUE_TIME, WINDIVERT_PARAM_QUEUE_TIME_MAX);
        (void)WinDivertSetParam(handle, WINDIVERT_PARAM_QUEUE_SIZE, WINDIVERT_PARAM_QUEUE_SIZE_MAX);
        {
            std::lock_guard lock(error_mutex);
            error.clear();
        }
        active.store(true, std::memory_order_release);
        try {
            worker = std::jthread([this](std::stop_token token) { run(token); });
            sweeper = std::jthread([this](std::stop_token token) {
                auto last_tunnel_refresh = Clock::now();
                while (!token.stop_requested()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                    const auto now = Clock::now();
                    expire(now);
                    if (now - last_tunnel_refresh >= std::chrono::seconds(2)) {
                        refresh_tunnels();
                        last_tunnel_refresh = now;
                    }
                }
            });
        } catch (const std::exception& exception) {
            active.store(false, std::memory_order_release);
            (void)WinDivertClose(handle);
            handle = INVALID_HANDLE_VALUE;
            const std::string message = std::string("Could not start ScpDPI's packet worker: ") + exception.what();
            set_error(message);
            return Result<void>::failure({message, 0});
        }
        return Result<void>::success();
    }

    Result<void> stop() {
        if (handle == INVALID_HANDLE_VALUE) return Result<void>::success();
        (void)WinDivertShutdown(handle, WINDIVERT_SHUTDOWN_RECV);
        if (worker.joinable()) { worker.request_stop(); worker.join(); }
        if (sweeper.joinable()) { sweeper.request_stop(); sweeper.join(); }
        {
            std::lock_guard lock(flow_mutex);
            for (auto& [key, hello] : pending) { (void)key; release_hello(hello); }
            pending.clear();
        }
        const BOOL closed = WinDivertClose(handle);
        const DWORD code = closed ? ERROR_SUCCESS : GetLastError();
        handle = INVALID_HANDLE_VALUE;
        active.store(false, std::memory_order_release);
        if (!closed) {
            const auto message = windows_error("ScpDPI could not close its Windows packet filter", code);
            set_error(message);
            return Result<void>::failure({message, static_cast<int>(code)});
        }
        return Result<void>::success();
    }
};

PacketEngine::PacketEngine() : impl_(std::make_unique<Impl>()) {}
PacketEngine::~PacketEngine() { (void)stop(); }
Result<void> PacketEngine::start(const std::filesystem::path& runtime_directory) { return impl_->start(runtime_directory); }
Result<void> PacketEngine::stop() { return impl_->stop(); }
bool PacketEngine::running() const noexcept { return impl_->active.load(std::memory_order_acquire); }
std::string PacketEngine::last_error() const { std::lock_guard lock(impl_->error_mutex); return impl_->error; }
PacketEngineStats PacketEngine::stats() const noexcept {
    PacketEngineStats value;
    value.packets_seen = impl_->packets_seen.load(std::memory_order_relaxed);
    value.tcp_443_outbound = impl_->tcp_443_outbound.load(std::memory_order_relaxed);
    value.tcp_443_tunnel = impl_->tcp_443_tunnel.load(std::memory_order_relaxed);
    value.tls_hellos_parsed = impl_->tls_hellos_parsed.load(std::memory_order_relaxed);
    value.tls_hellos_unlisted = impl_->tls_hellos_unlisted.load(std::memory_order_relaxed);
    value.tls_hellos_timed_out = impl_->tls_hellos_timed_out.load(std::memory_order_relaxed);
    value.resegment_attempts = impl_->resegment_attempts.load(std::memory_order_relaxed);
    value.resegment_failures = impl_->resegment_failures.load(std::memory_order_relaxed);
    value.packet_send_failures = impl_->packet_send_failures.load(std::memory_order_relaxed);
    value.packets_resegmented = impl_->packets_resegmented.load(std::memory_order_relaxed);
    value.domains_matched = impl_->domains_matched.load(std::memory_order_relaxed);
    value.quic_udp_443_dropped = impl_->quic_udp_443_dropped.load(std::memory_order_relaxed);
    value.tunnel_interfaces = impl_->tunnel_interfaces_count.load(std::memory_order_relaxed);
    value.strategies_changed = impl_->strategies_changed.load(std::memory_order_relaxed);
    return value;
}

} // namespace scpdpi::dpi

#else
namespace scpdpi::dpi {
struct PacketEngine::Impl {};
PacketEngine::PacketEngine() : impl_(std::make_unique<Impl>()) {}
PacketEngine::~PacketEngine() = default;
Result<void> PacketEngine::start(const std::filesystem::path&) {
    return Result<void>::failure({"The ScpDPI packet engine is currently available only on Windows", 0});
}
Result<void> PacketEngine::stop() { return Result<void>::success(); }
bool PacketEngine::running() const noexcept { return false; }
std::string PacketEngine::last_error() const { return {}; }
PacketEngineStats PacketEngine::stats() const noexcept { return {}; }
} // namespace scpdpi::dpi
#endif
