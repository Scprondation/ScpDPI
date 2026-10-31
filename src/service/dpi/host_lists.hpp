#pragma once

#include "scpdpi/common/result.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

namespace scpdpi::dpi {

class HostLists {
public:
    [[nodiscard]] Result<void> load(const std::filesystem::path& directory);
    [[nodiscard]] bool matches(std::string_view hostname) const;
    [[nodiscard]] std::size_t size() const noexcept { return hosts_.size(); }

private:
    [[nodiscard]] Result<void> load_file(const std::filesystem::path& path, bool required);
    std::unordered_set<std::string> hosts_;
};

} // namespace scpdpi::dpi
