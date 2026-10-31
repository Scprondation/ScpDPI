#include "scpdpi/network/providers.hpp"
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <vector>

namespace scpdpi {
namespace {
template <class T>
bool copy_attr(const rtattr* attr, T& value) {
    if (attr == nullptr || RTA_PAYLOAD(attr) < static_cast<int>(sizeof(T))) return false;
    std::memcpy(&value, RTA_DATA(attr), sizeof(T));
    return true;
}
class LinuxRouteProvider final : public IRouteProvider {
public:
    Result<std::vector<Route>> enumerate() const override {
        const int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
        if (fd < 0) return Result<std::vector<Route>>::failure({"Could not open RTNETLINK socket", errno});
        struct SocketGuard { int fd; ~SocketGuard() { if (fd >= 0) close(fd); } } guard{fd};
        sockaddr_nl local{}; local.nl_family = AF_NETLINK;
        if (bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0)
            return Result<std::vector<Route>>::failure({"Could not bind RTNETLINK socket", errno});

        std::vector<Route> routes;
        for (const int family : {AF_INET, AF_INET6}) {
            struct Request { nlmsghdr header; rtmsg message; } request{};
            request.header.nlmsg_len = NLMSG_LENGTH(sizeof(rtmsg));
            request.header.nlmsg_type = RTM_GETROUTE;
            request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
            request.header.nlmsg_seq = static_cast<std::uint32_t>(getpid()) ^ static_cast<std::uint32_t>(family);
            request.message.rtm_family = static_cast<unsigned char>(family);
            sockaddr_nl kernel{}; kernel.nl_family = AF_NETLINK;
            if (sendto(fd, &request, request.header.nlmsg_len, 0, reinterpret_cast<sockaddr*>(&kernel), sizeof(kernel)) < 0)
                return Result<std::vector<Route>>::failure({"RTNETLINK route request failed", errno});

            bool done = false;
            while (!done) {
                alignas(nlmsghdr) char buffer[64U * 1024U];
                const auto received = recv(fd, buffer, sizeof(buffer), 0);
                if (received < 0) {
                    if (errno == EINTR) continue;
                    return Result<std::vector<Route>>::failure({"RTNETLINK route response failed", errno});
                }
                if (received == 0) return Result<std::vector<Route>>::failure({"RTNETLINK closed before route dump completed", 0});
                int remaining = static_cast<int>(received);
                for (auto* header = reinterpret_cast<nlmsghdr*>(buffer); NLMSG_OK(header, remaining); header = NLMSG_NEXT(header, remaining)) {
                    if (header->nlmsg_seq != request.header.nlmsg_seq) continue;
                    if (header->nlmsg_type == NLMSG_DONE) { done = true; break; }
                    if (header->nlmsg_type == NLMSG_ERROR) {
                        const auto* error = reinterpret_cast<const nlmsgerr*>(NLMSG_DATA(header));
                        if (error->error != 0) return Result<std::vector<Route>>::failure({"RTNETLINK reported a route error", -error->error});
                        continue;
                    }
                    if (header->nlmsg_type != RTM_NEWROUTE || NLMSG_PAYLOAD(header, 0) < sizeof(rtmsg)) continue;
                    const auto* message = reinterpret_cast<const rtmsg*>(NLMSG_DATA(header));
                    if (message->rtm_family != family || message->rtm_type != RTN_UNICAST || message->rtm_dst_len > (family == AF_INET ? 32U : 128U)) continue;
                    std::uint32_t table = message->rtm_table;
                    const rtattr* destination = nullptr;
                    const rtattr* gateway = nullptr;
                    const rtattr* output = nullptr;
                    const rtattr* priority = nullptr;
                    int attr_len = RTM_PAYLOAD(header);
                    for (auto* attr = RTM_RTA(message); RTA_OK(attr, attr_len); attr = RTA_NEXT(attr, attr_len)) {
                        switch (attr->rta_type) {
                        case RTA_DST: destination = attr; break;
                        case RTA_GATEWAY: gateway = attr; break;
                        case RTA_OIF: output = attr; break;
                        case RTA_PRIORITY: priority = attr; break;
                        case RTA_TABLE: (void)copy_attr(attr, table); break;
                        default: break;
                        }
                    }
                    if (table != RT_TABLE_MAIN && table != RT_TABLE_DEFAULT && table != RT_TABLE_LOCAL) continue;
                    Route route;
                    route.destination.family = family == AF_INET ? AddressFamily::IPv4 : AddressFamily::IPv6;
                    route.prefix_length = message->rtm_dst_len;
                    if (destination != nullptr) {
                        const auto count = family == AF_INET ? 4U : 16U;
                        if (RTA_PAYLOAD(destination) < static_cast<int>(count)) continue;
                        std::memcpy(route.destination.bytes.data(), RTA_DATA(destination), count);
                    }
                    if (gateway != nullptr) {
                        const auto count = family == AF_INET ? 4U : 16U;
                        if (RTA_PAYLOAD(gateway) >= static_cast<int>(count)) {
                            IpAddress value; value.family = route.destination.family;
                            std::memcpy(value.bytes.data(), RTA_DATA(gateway), count);
                            route.gateway = value;
                        }
                    }
                    if (output != nullptr) (void)copy_attr(output, route.interface_index);
                    if (priority != nullptr) (void)copy_attr(priority, route.metric);
                    route.is_default = route.prefix_length == 0U;
                    char name[IF_NAMESIZE]{};
                    if (route.interface_index != 0U && if_indextoname(route.interface_index, name) != nullptr) route.interface_name = name;
                    routes.push_back(std::move(route));
                }
            }
        }
        return Result<std::vector<Route>>::success(std::move(routes));
    }
};
}
std::shared_ptr<const IRouteProvider> make_linux_route_provider() { return std::make_shared<LinuxRouteProvider>(); }
} // namespace scpdpi
