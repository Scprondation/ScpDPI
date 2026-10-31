#include "scpdpi/network/providers.hpp"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <string>

namespace scpdpi {
namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
bool virtual_hint(const std::string& name) {
    const auto text = lower(name);
    for (const auto* hint : {"tun", "tap", "wg", "wireguard", "veth", "virbr", "docker", "br-", "zt", "tailscale", "ppp"})
        if (text.rfind(hint, 0U) == 0U || text.find(hint) != std::string::npos) return true;
    return false;
}
class LinuxInterfaceProvider final : public IInterfaceProvider {
public:
    Result<std::vector<NetworkInterface>> enumerate() const override {
        ifaddrs* raw = nullptr;
        if (getifaddrs(&raw) != 0) return Result<std::vector<NetworkInterface>>::failure({"getifaddrs failed", errno});
        struct IfaddrsGuard { ifaddrs* value; ~IfaddrsGuard() { if (value != nullptr) freeifaddrs(value); } } guard{raw};
        std::map<std::uint32_t, NetworkInterface> found;
        for (auto* item = raw; item != nullptr; item = item->ifa_next) {
            if (item->ifa_name == nullptr) continue;
            const auto index = if_nametoindex(item->ifa_name);
            if (index == 0U) continue;
            auto& iface = found[index];
            iface.index = index;
            iface.name = item->ifa_name;
            iface.description = item->ifa_name;
            iface.is_up = (item->ifa_flags & IFF_UP) != 0;
            if (item->ifa_addr == nullptr) continue;
            const auto family = item->ifa_addr->sa_family;
            if (family == AF_INET || family == AF_INET6) {
                std::array<char, INET6_ADDRSTRLEN> buffer{};
                const void* source = family == AF_INET
                    ? static_cast<const void*>(&reinterpret_cast<const sockaddr_in*>(item->ifa_addr)->sin_addr)
                    : static_cast<const void*>(&reinterpret_cast<const sockaddr_in6*>(item->ifa_addr)->sin6_addr);
                if (inet_ntop(family, source, buffer.data(), static_cast<socklen_t>(buffer.size())) != nullptr) {
                    auto parsed = IpAddress::parse(buffer.data());
                    if (parsed) iface.addresses.push_back(std::move(parsed.value()));
                }
            } else if (family == AF_PACKET) {
                const auto* link = reinterpret_cast<const sockaddr_ll*>(item->ifa_addr);
                if (link->sll_halen > 0) {
                    constexpr char hex[] = "0123456789ABCDEF";
                    for (unsigned char i = 0; i < link->sll_halen; ++i) {
                        if (i != 0U) iface.mac_address.push_back(':');
                        const auto byte = link->sll_addr[i];
                        iface.mac_address.push_back(hex[byte >> 4U]);
                        iface.mac_address.push_back(hex[byte & 0x0FU]);
                    }
                    if (link->sll_hatype == ARPHRD_ETHER) iface.kind = "ethernet";
                    else if (link->sll_hatype == ARPHRD_LOOPBACK) iface.kind = "loopback";
                    else if (link->sll_hatype == ARPHRD_TUNNEL || link->sll_hatype == ARPHRD_TUNNEL6 || link->sll_hatype == ARPHRD_NONE)
                        iface.kind = "tunnel";
                }
            }
            if ((item->ifa_flags & IFF_LOOPBACK) != 0) iface.kind = "loopback";
            else if ((item->ifa_flags & IFF_POINTOPOINT) != 0) iface.kind = "tunnel";
        }

        const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        for (auto& [index, iface] : found) {
            (void)index;
            ifreq request{};
            const auto name_length = std::min(iface.name.size(), sizeof(request.ifr_name) - 1U);
            std::memcpy(request.ifr_name, iface.name.data(), name_length);
            request.ifr_name[name_length] = '\0';
            if (fd >= 0 && ioctl(fd, SIOCGIFMTU, &request) == 0 && request.ifr_mtu > 0)
                iface.mtu = static_cast<std::uint32_t>(request.ifr_mtu);
            if (iface.kind.empty()) iface.kind = "other";
            const bool loopback = iface.kind == "loopback";
            const bool link = iface.kind == "ethernet";
            const bool tunnel = iface.kind == "tunnel" || virtual_hint(iface.name);
            iface.is_virtual = !loopback && (tunnel || !link);
            iface.is_physical = !loopback && link && !tunnel;
            if (tunnel) iface.kind = "tunnel";
        }
        if (fd >= 0) close(fd);
        std::vector<NetworkInterface> result;
        result.reserve(found.size());
        for (auto& [index, iface] : found) { (void)index; result.push_back(std::move(iface)); }
        return Result<std::vector<NetworkInterface>>::success(std::move(result));
    }
};
}
std::shared_ptr<const IInterfaceProvider> make_linux_interface_provider() { return std::make_shared<LinuxInterfaceProvider>(); }
} // namespace scpdpi
