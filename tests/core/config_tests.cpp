#include "scpdpi/config/config.hpp"
#include <gtest/gtest.h>
#include <fstream>

namespace scpdpi {
namespace {
std::filesystem::path temp_file(const char* name) { return std::filesystem::temp_directory_path() / name; }
struct RemoveFile { std::filesystem::path path; ~RemoveFile() { std::error_code ignored; std::filesystem::remove(path, ignored); } };

TEST(ConfigTests, CreatesDocumentedDefaults) {
    const auto config = ConfigStore::defaults();
    EXPECT_EQ(config.version, "0.1.0");
    EXPECT_EQ(config.scan_interval_ms, 3000U);
    EXPECT_EQ(config.unknown_action, "bypass");
    EXPECT_EQ(config.profile, "default");
}

TEST(ConfigTests, SavesAndLoadsJsonConfiguration) {
    const auto path = temp_file("scpdpi-config-test.json"); RemoveFile cleanup{path};
    auto config = ConfigStore::defaults(); config.profile = "vpn-safe"; config.scan_interval_ms = 5000;
    ASSERT_TRUE(ConfigStore{}.save(config, path));
    const auto loaded = ConfigStore{}.load(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().profile, "vpn-safe");
    EXPECT_EQ(loaded.value().scan_interval_ms, 5000U);
}

TEST(ConfigTests, RejectsMalformedJson) {
    const auto path = temp_file("scpdpi-malformed-config.json"); RemoveFile cleanup{path};
    { std::ofstream out(path); out << "{\"engine\":"; }
    const auto loaded = ConfigStore{}.load(path);
    EXPECT_FALSE(loaded);
}

TEST(ConfigTests, RejectsInvalidInterval) {
    const auto path = temp_file("scpdpi-invalid-config.json"); RemoveFile cleanup{path};
    { std::ofstream out(path); out << R"({"engine":{"scan_interval_ms":100}})"; }
    EXPECT_FALSE(ConfigStore{}.load(path));
}

TEST(ConfigTests, RejectsUnsafeUnknownAction) {
    const auto path = temp_file("scpdpi-unsafe-action-config.json"); RemoveFile cleanup{path};
    { std::ofstream out(path); out << R"({"classification":{"unknown_action":"inspect"}})"; }
    EXPECT_FALSE(ConfigStore{}.load(path));
}
} // namespace
} // namespace scpdpi
