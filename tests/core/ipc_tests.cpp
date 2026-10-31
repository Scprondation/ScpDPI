#include "scpdpi/service/ipc_protocol.hpp"
#include <gtest/gtest.h>

namespace scpdpi {
TEST(IpcProtocolTests, ParsesSupportedStatusRequest) {
    const auto result = parse_ipc_request(R"({"command":"status"})");
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().command, "status");
}
TEST(IpcProtocolTests, ParsesDisableAutostartRequest) {
    const auto result = parse_ipc_request(R"({"command":"disable_autostart"})");
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().command, "disable_autostart");
}
TEST(IpcProtocolTests, ParsesEnableAutostartRequest) {
    const auto result = parse_ipc_request(R"({"command":"enable_autostart"})");
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().command, "enable_autostart");
}
TEST(IpcProtocolTests, RejectsUnknownCommands) {
    EXPECT_FALSE(parse_ipc_request(R"({"command":"execute_shell"})"));
}
TEST(IpcProtocolTests, RejectsUnknownProfile) {
    EXPECT_FALSE(parse_ipc_request(R"({"command":"set_profile","payload":{"profile":"root"}})"));
}
TEST(IpcProtocolTests, RejectsOversizedMessages) {
    EXPECT_FALSE(parse_ipc_request(std::string(16385, ' ')));
}
} // namespace scpdpi
