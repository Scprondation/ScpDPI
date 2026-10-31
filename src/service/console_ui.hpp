#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace scpdpi {

struct ConsoleUiCallbacks {
    std::function<nlohmann::json(const std::string&, const nlohmann::json&)> request;
};

[[nodiscard]] int run_console(const ConsoleUiCallbacks& callbacks);

} // namespace scpdpi
