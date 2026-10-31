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
    [[nodiscard]] bool matches_discord(std::string_view hostname) const;
    [[nodiscard]] bool matches_youtube(std::string_view hostname) const;
    [[nodiscard]] std::size_t size() const noexcept { return hosts_.size(); }

private:
    [[nodiscard]] Result<void> load_file(const std::filesystem::path& path, bool required,
                                         std::unordered_set<std::string>* category_hosts = nullptr);
    std::unordered_set<std::string> hosts_;
    std::unordered_set<std::string> discord_hosts_;
    std::unordered_set<std::string> youtube_hosts_;
};

} // namespace scpdpi::dpi
