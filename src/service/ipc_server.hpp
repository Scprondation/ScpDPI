#pragma once
#include "scpdpi/common/result.hpp"
#include "scpdpi/service/ipc_protocol.hpp"
#include <functional>
#include <stop_token>

namespace scpdpi {
class IpcServer {
public:
    using Handler = std::function<nlohmann::json(const IpcRequest&)>;
    [[nodiscard]] Result<void> run(std::stop_token stop, const Handler& handler) const;
};
} // namespace scpdpi
