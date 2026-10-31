#pragma once
#include "scpdpi/common/result.hpp"
#include <string>

namespace scpdpi {
struct IntegrationStatus { bool available{}; std::string detail{"Not Implemented in 0.1.0"}; };
class IXrayIntegration { public: virtual ~IXrayIntegration() = default; [[nodiscard]] virtual Result<IntegrationStatus> inspect() const = 0; };
class IWireGuardIntegration { public: virtual ~IWireGuardIntegration() = default; [[nodiscard]] virtual Result<IntegrationStatus> inspect() const = 0; };
class IOpenVpnIntegration { public: virtual ~IOpenVpnIntegration() = default; [[nodiscard]] virtual Result<IntegrationStatus> inspect() const = 0; };
class IHysteriaIntegration { public: virtual ~IHysteriaIntegration() = default; [[nodiscard]] virtual Result<IntegrationStatus> inspect() const = 0; };

template <class Interface>
class UnavailableIntegration final : public Interface {
public:
    [[nodiscard]] Result<IntegrationStatus> inspect() const override {
        return Result<IntegrationStatus>::success({false, "Not Implemented in ScpDPI 0.1.0"});
    }
};
} // namespace scpdpi
