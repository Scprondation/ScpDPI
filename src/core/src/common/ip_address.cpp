#include "scpdpi/network/types.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>

namespace scpdpi {

Result<IpAddress> IpAddress::parse(const std::string& text) {
    IpAddress result;
    in_addr v4{};
    if (inet_pton(AF_INET, text.c_str(), &v4) == 1) {
        result.family = AddressFamily::IPv4;
        std::memcpy(result.bytes.data(), &v4, 4);
        return Result<IpAddress>::success(result);
    }
    in6_addr v6{};
    if (inet_pton(AF_INET6, text.c_str(), &v6) == 1) {
        result.family = AddressFamily::IPv6;
        std::memcpy(result.bytes.data(), &v6, 16);
        return Result<IpAddress>::success(result);
    }
    return Result<IpAddress>::failure({"Invalid IPv4 or IPv6 address: " + text, 0});
}

std::string IpAddress::to_string() const {
    char buffer[INET6_ADDRSTRLEN]{};
    const auto family_code = family == AddressFamily::IPv4 ? AF_INET : AF_INET6;
    if (inet_ntop(family_code, bytes.data(), buffer, sizeof(buffer)) == nullptr) return {};
    return buffer;
}

} // namespace scpdpi
