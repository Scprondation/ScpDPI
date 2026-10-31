#include "autostart.hpp"

#include <array>
#include <string>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winreg.h>
#include <shlobj.h>
#endif

namespace scpdpi {
namespace {

#ifdef _WIN32
Result<std::filesystem::path> current_executable() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size())
        return Result<std::filesystem::path>::failure({"Could not locate the running ScpDPI executable", static_cast<int>(GetLastError())});
    return Result<std::filesystem::path>::success(std::filesystem::path(buffer.data()));
}

Result<void> copy_if_missing(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(source, error) || error)
        return Result<void>::failure({"Required ScpDPI runtime file is missing: " + source.string(), error.value()});
    if (std::filesystem::exists(destination, error) && !error) return Result<void>::success();
    error.clear();
    if (!std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, error))
        return Result<void>::failure({"Could not copy ScpDPI into the per-user app directory: " + destination.string(), error.value()});
    return Result<void>::success();
}

Result<void> remove_legacy_run_entry() {
    HKEY key = nullptr;
    constexpr wchar_t run_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const LSTATUS open_status = RegOpenKeyExW(HKEY_CURRENT_USER, run_key, 0, KEY_SET_VALUE, &key);
    if (open_status == ERROR_FILE_NOT_FOUND) return Result<void>::success();
    if (open_status != ERROR_SUCCESS)
        return Result<void>::failure({"Could not update the current user's Windows startup settings", static_cast<int>(open_status)});
    const LSTATUS delete_status = RegDeleteValueW(key, L"ScpDPI");
    RegCloseKey(key);
    if (delete_status != ERROR_SUCCESS && delete_status != ERROR_FILE_NOT_FOUND)
        return Result<void>::failure({"Could not remove the old ScpDPI startup entry", static_cast<int>(delete_status)});
    return Result<void>::success();
}

Result<void> copy_runtime_tree(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::error_code error;
    if (!std::filesystem::is_directory(source, error) || error)
        return Result<void>::failure({"The Windows DPI runtime is missing from the build output", error.value()});
    for (std::filesystem::recursive_directory_iterator item(source, error), end; !error && item != end; item.increment(error)) {
        const auto relative = std::filesystem::relative(item->path(), source, error);
        if (error) break;
        const auto target = destination / relative;
        if (item->is_directory(error)) {
            std::filesystem::create_directories(target, error);
            if (error) break;
        } else if (item->is_regular_file(error)) {
            std::filesystem::create_directories(target.parent_path(), error);
            if (error) break;
            if (!std::filesystem::exists(target, error)) {
                error.clear();
                std::filesystem::copy_file(item->path(), target, std::filesystem::copy_options::none, error);
                if (error) break;
            }
        }
        if (error) break;
    }
    if (error) return Result<void>::failure({"Could not copy the Windows DPI runtime", error.value()});
    return Result<void>::success();
}
#endif

} // namespace

Result<std::filesystem::path> install_and_register_autostart() {
#ifdef _WIN32
    const auto source_exe = current_executable();
    if (!source_exe) return Result<std::filesystem::path>::failure(source_exe.error());
    const auto source_dir = source_exe.value().parent_path();
    std::error_code error;
    if (!std::filesystem::is_regular_file(source_dir / L"scpdpi_autostart.exe", error) || error)
        return Result<std::filesystem::path>::failure({"The ScpDPI background launcher is missing from the build output", error.value()});
    if (!std::filesystem::is_directory(source_dir / L"scpdpi-dpi", error) || error)
        return Result<std::filesystem::path>::failure({"The Windows DPI runtime is missing from the build output", error.value()});
    const auto legacy_removed = remove_legacy_run_entry();
    if (!legacy_removed) return Result<std::filesystem::path>::failure(legacy_removed.error());
    return Result<std::filesystem::path>::success(source_dir);
#else
    return Result<std::filesystem::path>::failure({"Per-user Windows autostart is not available on this platform", 0});
#endif
}

Result<std::filesystem::path> install_protected_runtime() {
#ifdef _WIN32
    const auto source_exe = current_executable();
    if (!source_exe) return Result<std::filesystem::path>::failure(source_exe.error());
    PWSTR program_files = nullptr;
    const HRESULT known_folder = SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, &program_files);
    if (FAILED(known_folder))
        return Result<std::filesystem::path>::failure({"Could not locate the protected Windows application directory", static_cast<int>(known_folder)});
    const std::filesystem::path install_dir = std::filesystem::path(program_files) / L"ScpDPI";
    CoTaskMemFree(program_files);
    std::error_code error;
    std::filesystem::create_directories(install_dir, error);
    if (error) return Result<std::filesystem::path>::failure({"Could not create the protected ScpDPI install directory", error.value()});

    const auto source_dir = source_exe.value().parent_path();
    auto copied = copy_if_missing(source_exe.value(), install_dir / source_exe.value().filename());
    if (!copied) return Result<std::filesystem::path>::failure(copied.error());
    copied = copy_if_missing(source_dir / L"scpdpi_autostart.exe", install_dir / L"scpdpi_autostart.exe");
    if (!copied) return Result<std::filesystem::path>::failure(copied.error());

    for (std::filesystem::directory_iterator item(source_dir, error), end; !error && item != end; item.increment(error)) {
        if (!item->is_regular_file(error) || error) { error.clear(); continue; }
        const auto extension = item->path().extension().wstring();
        if (extension == L".dll" || extension == L".DLL") {
            copied = copy_if_missing(item->path(), install_dir / item->path().filename());
            if (!copied) return Result<std::filesystem::path>::failure(copied.error());
        }
    }
    if (error) return Result<std::filesystem::path>::failure({"Could not enumerate ScpDPI runtime files", error.value()});
    const auto runtime_copy = copy_runtime_tree(source_dir / L"scpdpi-dpi", install_dir / L"scpdpi-dpi");
    if (!runtime_copy) return Result<std::filesystem::path>::failure(runtime_copy.error());
    const auto legacy_removed = remove_legacy_run_entry();
    if (!legacy_removed) return Result<std::filesystem::path>::failure(legacy_removed.error());
    return Result<std::filesystem::path>::success(install_dir);
#else
    return Result<std::filesystem::path>::failure({"Protected Windows runtime installation is not available on this platform", 0});
#endif
}

Result<void> enable_autostart() {
#ifdef _WIN32
    return set_background_task_autostart(true);
#else
    return Result<void>::failure({"Per-user Windows autostart is not available on this platform", 0});
#endif
}

Result<void> remove_autostart() {
#ifdef _WIN32
    const auto legacy_removed = remove_legacy_run_entry();
    if (!legacy_removed) return legacy_removed;
    return set_background_task_autostart(false);
#else
    return Result<void>::failure({"Per-user Windows autostart is not available on this platform", 0});
#endif
}

} // namespace scpdpi
