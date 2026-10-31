#include "autostart.hpp"

#include <array>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winreg.h>
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

Result<std::filesystem::path> local_app_data() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size())
        return Result<std::filesystem::path>::failure({"LOCALAPPDATA is unavailable; ScpDPI cannot install per-user startup", static_cast<int>(GetLastError())});
    return Result<std::filesystem::path>::success(std::filesystem::path(buffer.data()));
}

Result<std::filesystem::path> startup_marker_path() {
    const auto app_data = local_app_data();
    if (!app_data) return Result<std::filesystem::path>::failure(app_data.error());
    return Result<std::filesystem::path>::success(app_data.value() / L"Programs" / L"ScpDPI" / L".startup-configured");
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

Result<void> write_run_entry(const std::filesystem::path& helper) {
    HKEY key = nullptr;
    constexpr wchar_t run_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const LSTATUS open_status = RegCreateKeyExW(HKEY_CURRENT_USER, run_key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (open_status != ERROR_SUCCESS)
        return Result<void>::failure({"Could not open the current user's Windows startup settings", static_cast<int>(open_status)});
    const std::wstring command = L"\"" + helper.wstring() + L"\"";
    const DWORD byte_count = static_cast<DWORD>((command.size() + 1U) * sizeof(wchar_t));
    const LSTATUS write_status = RegSetValueExW(key, L"ScpDPI", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), byte_count);
    RegCloseKey(key);
    if (write_status != ERROR_SUCCESS)
        return Result<void>::failure({"Could not register ScpDPI to start for the current Windows user", static_cast<int>(write_status)});
    return Result<void>::success();
}

Result<void> write_startup_marker(const std::filesystem::path& marker, const char* value) {
    std::error_code error;
    std::filesystem::create_directories(marker.parent_path(), error);
    if (error) return Result<void>::failure({"Could not save the ScpDPI startup preference", error.value()});
    std::ofstream configured(marker, std::ios::binary | std::ios::trunc);
    if (!configured) return Result<void>::failure({"Could not save the ScpDPI startup preference", 0});
    configured << value << '\n';
    if (!configured) return Result<void>::failure({"Could not save the ScpDPI startup preference", 0});
    return Result<void>::success();
}
#endif

} // namespace

Result<std::filesystem::path> install_and_register_autostart() {
#ifdef _WIN32
    const auto source_exe = current_executable();
    if (!source_exe) return Result<std::filesystem::path>::failure(source_exe.error());
    const auto app_data = local_app_data();
    if (!app_data) return Result<std::filesystem::path>::failure(app_data.error());

    const auto install_dir = app_data.value() / L"Programs" / L"ScpDPI";
    std::error_code error;
    std::filesystem::create_directories(install_dir, error);
    if (error) return Result<std::filesystem::path>::failure({"Could not create the per-user ScpDPI install directory", error.value()});

    auto copied = copy_if_missing(source_exe.value(), install_dir / source_exe.value().filename());
    if (!copied) return Result<std::filesystem::path>::failure(copied.error());
    const auto source_dir = source_exe.value().parent_path();
    const auto helper_name = std::filesystem::path(L"scpdpi_autostart.exe");
    copied = copy_if_missing(source_dir / helper_name, install_dir / helper_name);
    if (!copied) return Result<std::filesystem::path>::failure(copied.error());

    for (std::filesystem::directory_iterator item(source_dir, error), end; !error && item != end; item.increment(error)) {
        if (!item->is_regular_file(error) || error) { error.clear(); continue; }
        std::wstring extension = item->path().extension().wstring();
        if (extension == L".dll" || extension == L".DLL") {
            copied = copy_if_missing(item->path(), install_dir / item->path().filename());
            if (!copied) return Result<std::filesystem::path>::failure(copied.error());
        }
    }
    if (error) return Result<std::filesystem::path>::failure({"Could not enumerate ScpDPI runtime files", error.value()});

    const auto marker = install_dir / L".startup-configured";
    if (!std::filesystem::exists(marker, error)) {
        if (error) return Result<std::filesystem::path>::failure({"Could not check ScpDPI startup preference", error.value()});
        const auto registered = write_run_entry(install_dir / helper_name);
        if (!registered) return Result<std::filesystem::path>::failure(registered.error());
        const auto saved = write_startup_marker(marker, "configured");
        if (!saved) return Result<std::filesystem::path>::failure({"ScpDPI was registered, but its startup preference could not be saved", saved.error().native_code});
    }
    return Result<std::filesystem::path>::success(install_dir);
#else
    return Result<std::filesystem::path>::failure({"Per-user Windows autostart is not available on this platform", 0});
#endif
}

Result<void> enable_autostart() {
#ifdef _WIN32
    const auto app_data = local_app_data();
    if (!app_data) return Result<void>::failure(app_data.error());
    auto helper = app_data.value() / L"Programs" / L"ScpDPI" / L"scpdpi_autostart.exe";
    std::error_code error;
    if (!std::filesystem::is_regular_file(helper, error) || error) {
        const auto installed = install_and_register_autostart();
        if (!installed) return Result<void>::failure(installed.error());
        helper = installed.value() / L"scpdpi_autostart.exe";
    }
    const auto registered = write_run_entry(helper);
    if (!registered) return registered;
    const auto marker = startup_marker_path();
    if (!marker) return Result<void>::failure(marker.error());
    return write_startup_marker(marker.value(), "configured");
#else
    return Result<void>::failure({"Per-user Windows autostart is not available on this platform", 0});
#endif
}

Result<void> remove_autostart() {
#ifdef _WIN32
    HKEY key = nullptr;
    constexpr wchar_t run_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const LSTATUS open_status = RegOpenKeyExW(HKEY_CURRENT_USER, run_key, 0, KEY_SET_VALUE, &key);
    if (open_status != ERROR_SUCCESS && open_status != ERROR_FILE_NOT_FOUND)
        return Result<void>::failure({"Could not open the current user's Windows startup settings", static_cast<int>(open_status)});
    if (open_status == ERROR_SUCCESS) {
        const LSTATUS delete_status = RegDeleteValueW(key, L"ScpDPI");
        RegCloseKey(key);
        if (delete_status != ERROR_SUCCESS && delete_status != ERROR_FILE_NOT_FOUND)
            return Result<void>::failure({"Could not remove ScpDPI from Windows startup", static_cast<int>(delete_status)});
    }
    const auto marker = startup_marker_path();
    if (!marker) return Result<void>::failure(marker.error());
    return write_startup_marker(marker.value(), "disabled");
#else
    return Result<void>::failure({"Per-user Windows autostart is not available on this platform", 0});
#endif
}

} // namespace scpdpi
