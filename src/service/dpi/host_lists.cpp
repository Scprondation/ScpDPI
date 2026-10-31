#include "host_lists.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string_view>

namespace scpdpi::dpi {
namespace {
constexpr std::size_t max_unique_hosts = 1000000U;

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.remove_suffix(1);
    return value;
}

bool valid_hostname(std::string_view value) {
    if (value.empty() || value.size() > 253U || value.front() == '.' || value.back() == '.') return false;
    std::size_t label_length = 0;
    char previous = '\0';
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '.') {
            if (label_length == 0U || label_length > 63U || previous == '-') return false;
            label_length = 0;
            previous = character;
            continue;
        }
        if (std::isalnum(byte) == 0 && character != '-') return false;
        if (label_length == 0U && character == '-') return false;
        ++label_length;
        previous = character;
    }
    return label_length > 0U && label_length <= 63U && previous != '-';
}

} // namespace

Result<void> HostLists::load(const std::filesystem::path& directory) {
    hosts_.clear();
    const auto registry = load_file(directory / L"russia-registry-domains.txt", true);
    if (!registry) return registry;
    const auto youtube = load_file(directory / L"russia-youtube.txt", true);
    if (!youtube) return youtube;
    const auto discord = load_file(directory / L"russia-discord.txt", true);
    if (!discord) return discord;
    if (hosts_.empty()) return Result<void>::failure({"The Russian DPI domain lists are empty", 0});
    return Result<void>::success();
}

Result<void> HostLists::load_file(const std::filesystem::path& path, bool required) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (!required) return Result<void>::success();
        return Result<void>::failure({"Could not open the DPI host list: " + path.filename().string(), 0});
    }

    std::string line;
    bool first_line = true;
    while (std::getline(input, line)) {
        std::string_view item = trim(line);
        if (first_line && item.starts_with("\xef\xbb\xbf")) item.remove_prefix(3);
        first_line = false;
        if (item.empty() || item.front() == '#' || item.front() == '!') continue;
        if (const auto comment = item.find_first_of(" \t#"); comment != std::string_view::npos)
            item = trim(item.substr(0, comment));
        if (!valid_hostname(item)) continue;
        std::string hostname(item);
        std::transform(hostname.begin(), hostname.end(), hostname.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        hosts_.insert(std::move(hostname));
        if (hosts_.size() > max_unique_hosts)
            return Result<void>::failure({"The Russian DPI host list exceeds the 1000000-domain safety limit", 0});
    }
    if (input.bad()) return Result<void>::failure({"Could not read the DPI host list: " + path.filename().string(), 0});
    return Result<void>::success();
}

bool HostLists::matches(std::string_view hostname) const {
    std::string lowered(hostname);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (hosts_.contains(lowered)) return true;
    std::size_t dot = lowered.find('.');
    while (dot != std::string::npos) {
        if (hosts_.contains(lowered.substr(dot + 1U))) return true;
        dot = lowered.find('.', dot + 1U);
    }
    return false;
}

} // namespace scpdpi::dpi
