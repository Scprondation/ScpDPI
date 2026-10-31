#pragma once

#include "scpdpi/common/result.hpp"

#include <filesystem>

namespace scpdpi {

/** Copies the current Windows build into the per-user app directory and registers it at logon. */
[[nodiscard]] Result<std::filesystem::path> install_and_register_autostart();

/** Enables the current user's ScpDPI logon entry. */
[[nodiscard]] Result<void> enable_autostart();

/** Removes only the current user's ScpDPI logon entry. Installed files remain available. */
[[nodiscard]] Result<void> remove_autostart();

} // namespace scpdpi
