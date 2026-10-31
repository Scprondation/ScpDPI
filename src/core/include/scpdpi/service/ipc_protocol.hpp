#pragma once

#include "scpdpi/common/result.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace scpdpi {

struct IpcRequest {
    std::string command;
    nlohmann::json payload{nlohmann::json::object()};
};

/** Parses one bounded newline-delimited JSON message and rejects unknown commands. */
[[nodiscard]] Result<IpcRequest> parse_ipc_request(std::string_view message);

} // namespace scpdpi
