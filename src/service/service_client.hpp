#pragma once

#include <nlohmann/json.hpp>

#include "scpdpi/common/result.hpp"

#include <filesystem>
#include <string>

namespace scpdpi {

[[nodiscard]] nlohmann::json request_service(const std::string& command, const nlohmann::json& payload);
[[nodiscard]] Result<void> ensure_background_service(const std::filesystem::path& install_dir);

} // namespace scpdpi
