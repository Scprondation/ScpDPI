#pragma once

#include "scpdpi/common/result.hpp"

#include <filesystem>

namespace scpdpi {

/** Checks the build runtime and removes the obsolete unprivileged startup entry. */
[[nodiscard]] Result<std::filesystem::path> install_and_register_autostart();

/** Copies the current build into Program Files. Call only from the elevated installer. */
[[nodiscard]] Result<std::filesystem::path> install_protected_runtime();
[[nodiscard]] Result<std::filesystem::path> expected_protected_runtime_path(const std::filesystem::path& source_directory);

/** Installs or runs the elevated logon task that hosts packet processing. */
[[nodiscard]] Result<void> ensure_elevated_background_task(const std::filesystem::path& install_dir, bool force_reinstall = false);

/** Called only by the elevated installer process. */
[[nodiscard]] Result<void> install_elevated_background_task();

/** Enables or disables the elevated task's sign-in trigger without stopping a running service. */
[[nodiscard]] Result<void> set_background_task_autostart(bool enabled);

/** Enables the current user's elevated ScpDPI sign-in task. */
[[nodiscard]] Result<void> enable_autostart();

/** Disables the sign-in trigger; installed files and the current service remain available. */
[[nodiscard]] Result<void> remove_autostart();

} // namespace scpdpi
