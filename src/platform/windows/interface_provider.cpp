#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "scpdpi/network/providers.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace scpdpi {
namespace {
std::string utf8(const wchar_t* text) {
    if (text == nullptr || *text == L'\0') return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string value(static_cast<std::size_t>(size), '\0');
    (void)WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), size, nullptr, nullptr);
    value.resize(static_cast<std::size_t>(size - 1));
    return value;
}
bool has_hint(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto* hint : {"virtual", "tunnel", "tap", "wintun", "wireguard", "openvpn", "xray", "hyper-v"})
        if (value.find(hint) != std::string::npos) return true;
    return false;
}
std::string address_text(const sockaddr* addr) {
    if (addr == nullptr) return {};
    std::array<char, INET6_ADDRSTRLEN> buffer{};
    if (addr->sa_family == AF_INET) {
        const auto* v4 = reinterpret_cast<const sockaddr_in*>(addr);
        if (InetNtopA(AF_INET, const_cast<IN_ADDR*>(&v4->sin_addr), buffer.data(), static_cast<DWORD>(buffer.size())) != nullptr) return buffer.data();
    } else if (addr->sa_family == AF_INET6) {
        const auto* v6 = reinterpret_cast<const sockaddr_in6*>(addr);
        if (InetNtopA(AF_INET6, const_cast<IN6_ADDR*>(&v6->sin6_addr), buffer.data(), static_cast<DWORD>(buffer.size())) != nullptr) return buffer.data();
    }
    return {};
}
class WindowsInterfaceProvider final : public IInterfaceProvider {
public:
    Result<std::vector<NetworkInterface>> enumerate() const override {
        ULONG size = 16U * 1024U;
        std::vector<unsigned char> storage(size);
        ULONG status = ERROR_BUFFER_OVERFLOW;
        for (int attempt = 0; attempt < 4 && status == ERROR_BUFFER_OVERFLOW; ++attempt) {
            status = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                                          nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()), &size);
            if (status == ERROR_BUFFER_OVERFLOW) storage.resize(size);
        }
        if (status != NO_ERROR) return Result<std::vector<NetworkInterface>>::failure({"GetAdaptersAddresses failed", static_cast<int>(status)});
        std::vector<NetworkInterface> result;
        for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()); adapter != nullptr; adapter = adapter->Next) {
            NetworkInterface iface;
            iface.index = adapter->IfIndex != 0U ? adapter->IfIndex : adapter->Ipv6IfIndex;
            iface.name = utf8(adapter->FriendlyName);
            iface.description = utf8(adapter->Description);
            iface.mtu = adapter->Mtu;
            iface.is_up = adapter->OperStatus == IfOperStatusUp;
            iface.kind = adapter->IfType == IF_TYPE_TUNNEL ? "tunnel" :
                         adapter->IfType == IF_TYPE_ETHERNET_CSMACD ? "ethernet" :
                         adapter->IfType == IF_TYPE_IEEE80211 ? "wifi" :
                         adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ? "loopback" : "other";
            iface.mac_address.reserve(static_cast<std::size_t>(adapter->PhysicalAddressLength) * 3U);
            constexpr char digits[] = "0123456789ABCDEF";
            for (ULONG i = 0; i < adapter->PhysicalAddressLength; ++i) {
                if (i != 0U) iface.mac_address.push_back(':');
                const auto byte = adapter->PhysicalAddress[i];
                iface.mac_address.push_back(digits[byte >> 4U]);
                iface.mac_address.push_back(digits[byte & 0x0FU]);
            }
            iface.is_virtual = adapter->IfType == IF_TYPE_TUNNEL ||
                               has_hint(iface.name + " " + iface.description);
            iface.is_physical = !iface.is_virtual && (adapter->IfType == IF_TYPE_ETHERNET_CSMACD || adapter->IfType == IF_TYPE_IEEE80211);
            for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                const auto text = address_text(unicast->Address.lpSockaddr);
                if (text.empty()) continue;
                auto address = IpAddress::parse(text);
                if (address) iface.addresses.push_back(std::move(address.value()));
            }
            result.push_back(std::move(iface));
        }
        return Result<std::vector<NetworkInterface>>::success(std::move(result));
    }
};
}
std::shared_ptr<const IInterfaceProvider> make_windows_interface_provider() { return std::make_shared<WindowsInterfaceProvider>(); }
} // namespace scpdpi
