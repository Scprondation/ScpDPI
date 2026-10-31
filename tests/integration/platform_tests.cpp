#include "scpdpi/network/providers.hpp"
#include <gtest/gtest.h>
#include <algorithm>

namespace scpdpi {
namespace {
TEST(PlatformIntegrationTests, EnumeratesNetworkInterfacesWithoutElevation) {
    const auto providers = make_platform_providers();
    ASSERT_NE(providers.interfaces, nullptr);
    const auto result = providers.interfaces->enumerate();
    ASSERT_TRUE(result) << result.error().message;
    for (const auto& iface : result.value()) {
        EXPECT_NE(iface.index, 0U);
        EXPECT_FALSE(iface.name.empty());
        for (const auto& address : iface.addresses) EXPECT_FALSE(address.to_string().empty());
    }
}

TEST(PlatformIntegrationTests, EnumeratesNativeRoutesWithoutElevation) {
    const auto providers = make_platform_providers();
    ASSERT_NE(providers.routes, nullptr);
    const auto result = providers.routes->enumerate();
    ASSERT_TRUE(result) << result.error().message;
    for (const auto& route : result.value()) {
        EXPECT_LE(route.prefix_length, route.destination.family == AddressFamily::IPv4 ? 32U : 128U);
        if (route.is_default) EXPECT_EQ(route.prefix_length, 0U);
        if (route.gateway) EXPECT_EQ(route.gateway->family, route.destination.family);
    }
}
} // namespace
} // namespace scpdpi
