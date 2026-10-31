#pragma once
#include "scpdpi/common/result.hpp"
#include <filesystem>

namespace scpdpi {
class Logger {
public:
    [[nodiscard]] static Result<void> initialize(const std::filesystem::path& log_file,
                                                  const std::string& level, bool file_enabled);
};
} // namespace scpdpi
