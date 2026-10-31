#include "scpdpi/network/providers.hpp"

namespace scpdpi {
#ifdef _WIN32
std::shared_ptr<const IInterfaceProvider> make_windows_interface_provider();
std::shared_ptr<const IRouteProvider> make_windows_route_provider();
#elif defined(__linux__)
std::shared_ptr<const IInterfaceProvider> make_linux_interface_provider();
std::shared_ptr<const IRouteProvider> make_linux_route_provider();
#endif

PlatformProviders make_platform_providers() {
#ifdef _WIN32
    return {make_windows_interface_provider(), make_windows_route_provider()};
#elif defined(__linux__)
    return {make_linux_interface_provider(), make_linux_route_provider()};
#else
    return {};
#endif
}
} // namespace scpdpi
