#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include "scpdpi/network/providers.hpp"
#include <array>
#include <cstring>
#include <memory>

namespace scpdpi {
namespace {
IpAddress from_v4(const IN_ADDR& raw) {
    IpAddress address; address.family = AddressFamily::IPv4;
    std::memcpy(address.bytes.data(), &raw, 4U); return address;
}
IpAddress from_v6(const IN6_ADDR& raw) {
    IpAddress address; address.family = AddressFamily::IPv6;
    std::memcpy(address.bytes.data(), &raw, 16U); return address;
}
std::string utf8_name(NET_LUID luid) {
    std::array<wchar_t, IF_MAX_STRING_SIZE + 1U> buffer{};
    if (ConvertInterfaceLuidToNameW(&luid, buffer.data(), static_cast<SIZE_T>(buffer.size())) != NO_ERROR) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    (void)WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, result.data(), count, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(count - 1));
    return result;
}
class WindowsRouteProvider final : public IRouteProvider {
public:
    Result<std::vector<Route>> enumerate() const override {
        PMIB_IPFORWARD_TABLE2 table = nullptr;
        const ULONG status = GetIpForwardTable2(AF_UNSPEC, &table);
        if (status != NO_ERROR) return Result<std::vector<Route>>::failure({"GetIpForwardTable2 failed", static_cast<int>(status)});
        std::vector<Route> routes;
        routes.reserve(table->NumEntries);
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const auto& row = table->Table[i];
            Route route;
            route.interface_index = row.InterfaceIndex;
            route.interface_name = utf8_name(row.InterfaceLuid);
            route.metric = row.Metric;
            MIB_IPINTERFACE_ROW interface_row{};
            interface_row.Family = row.DestinationPrefix.Prefix.si_family;
            interface_row.InterfaceIndex = row.InterfaceIndex;
            if (GetIpInterfaceEntry(&interface_row) == NO_ERROR) {
                const auto effective_metric = static_cast<std::uint64_t>(route.metric) + interface_row.Metric;
                constexpr std::uint64_t max_metric = 0xFFFFFFFFULL;
                route.metric = static_cast<std::uint32_t>(effective_metric > max_metric ? max_metric : effective_metric);
            }
            if (row.DestinationPrefix.Prefix.si_family == AF_INET) {
                route.destination = from_v4(row.DestinationPrefix.Prefix.Ipv4.sin_addr);
                route.prefix_length = row.DestinationPrefix.PrefixLength;
                route.is_default = route.prefix_length == 0U;
                if (row.NextHop.si_family == AF_INET && row.NextHop.Ipv4.sin_addr.s_addr != 0U)
                    route.gateway = from_v4(row.NextHop.Ipv4.sin_addr);
            } else if (row.DestinationPrefix.Prefix.si_family == AF_INET6) {
                route.destination = from_v6(row.DestinationPrefix.Prefix.Ipv6.sin6_addr);
                route.prefix_length = row.DestinationPrefix.PrefixLength;
                route.is_default = route.prefix_length == 0U;
                const auto& gateway = row.NextHop.Ipv6.sin6_addr;
                if (row.NextHop.si_family == AF_INET6 && !IN6_IS_ADDR_UNSPECIFIED(&gateway)) route.gateway = from_v6(gateway);
            } else continue;
            routes.push_back(std::move(route));
        }
        FreeMibTable(table);
        return Result<std::vector<Route>>::success(std::move(routes));
    }
};
}
std::shared_ptr<const IRouteProvider> make_windows_route_provider() { return std::make_shared<WindowsRouteProvider>(); }
} // namespace scpdpi
