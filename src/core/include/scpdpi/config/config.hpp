#pragma once

#include "scpdpi/common/result.hpp"
#include <filesystem>
#include <string>

namespace scpdpi {

struct AppConfig {
    std::string version{"0.1.0"};
    bool engine_enabled{true};
    std::uint32_t scan_interval_ms{3000};
    std::string unknown_action{"bypass"};
    std::string profile{"default"};
    std::string log_level{"info"};
    bool file_logging{true};
    bool dpi_enabled{
#ifdef _WIN32
        true
#else
        false
#endif
    };
};

class ConfigStore {
public:
    [[nodiscard]] Result<AppConfig> load(const std::filesystem::path& path) const;
    [[nodiscard]] Result<void> save(const AppConfig& config, const std::filesystem::path& path) const;
    [[nodiscard]] static AppConfig defaults();
};

} // namespace scpdpi
