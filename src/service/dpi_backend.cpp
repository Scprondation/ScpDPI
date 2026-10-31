#include "dpi_backend.hpp"

#include <array>
#include <filesystem>
#include <string>

#ifdef _WIN32
namespace scpdpi {
namespace {

std::string error_text(const char* message, DWORD code) {
    return std::string(message) + " (Windows error " + std::to_string(code) + ")";
}

std::filesystem::path runtime_directory() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) return {};
    return std::filesystem::path(buffer.data()).parent_path() / L"scpdpi-dpi";
}

std::wstring quote(const std::filesystem::path& path) {
    return L"\"" + path.wstring() + L"\"";
}

} // namespace

DpiBackend::~DpiBackend() { (void)stop(); }

bool DpiBackend::running() {
    if (process_ == nullptr) return false;
    const DWORD wait = WaitForSingleObject(process_, 0U);
    if (wait == WAIT_TIMEOUT) return true;
    if (wait == WAIT_FAILED) {
        last_error_ = error_text("Could not inspect the DPI backend process", GetLastError());
        return true;
    }

    DWORD exit_code = 0;
    (void)GetExitCodeProcess(process_, &exit_code);
    if (exit_code != 0U) last_error_ = "The DPI backend stopped with exit code " + std::to_string(exit_code);
    CloseHandle(process_);
    process_ = nullptr;
    if (job_ != nullptr) {
        CloseHandle(job_);
        job_ = nullptr;
    }
    return false;
}

Result<void> DpiBackend::start() {
    if (running()) return Result<void>::success();
    last_error_.clear();
    const auto runtime = runtime_directory();
    if (runtime.empty()) {
        last_error_ = "Could not locate the ScpDPI installation directory";
        return Result<void>::failure({last_error_, static_cast<int>(GetLastError())});
    }

    const auto executable = runtime / L"goodbyedpi.exe";
    const auto required_files = {executable, runtime / L"WinDivert.dll", runtime / L"WinDivert64.sys",
                                 runtime / L"russia-blacklist.txt", runtime / L"russia-youtube.txt",
                                 runtime / L"scpdpi-discord.txt"};
    for (const auto& file : required_files) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(file, error) || error) {
            last_error_ = "A DPI runtime file is missing: " + file.filename().string();
            return Result<void>::failure({last_error_, error.value()});
        }
    }

    constexpr wchar_t fake_tls[] = L"160301FFFF01FFFFFF0303594F5552204144564552544953454D454E542048455245202D202431302F6D6F000000000009000000050003000000";
    std::wstring command = quote(executable) + L" -9 --fake-gen 5 --fake-from-hex " + fake_tls +
        L" --blacklist " + quote(runtime / L"russia-blacklist.txt") +
        L" --blacklist " + quote(runtime / L"russia-youtube.txt") +
        L" --blacklist " + quote(runtime / L"scpdpi-discord.txt");

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            CloseHandle(job);
            job = nullptr;
        }
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, runtime.c_str(), &startup, &process)) {
        const DWORD code = GetLastError();
        if (job != nullptr) CloseHandle(job);
        last_error_ = error_text("Could not start the DPI packet processor; administrator permission is required", code);
        return Result<void>::failure({last_error_, static_cast<int>(code)});
    }

    if (job != nullptr && AssignProcessToJobObject(job, process.hProcess)) {
        job_ = job;
    } else if (job != nullptr) {
        CloseHandle(job);
    }
    process_ = process.hProcess;
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1)) {
        const DWORD code = GetLastError();
        CloseHandle(process.hThread);
        (void)TerminateProcess(process_, code);
        CloseHandle(process_);
        process_ = nullptr;
        if (job_ != nullptr) { CloseHandle(job_); job_ = nullptr; }
        last_error_ = error_text("Could not start the DPI packet processor thread", code);
        return Result<void>::failure({last_error_, static_cast<int>(code)});
    }
    CloseHandle(process.hThread);

    const DWORD wait = WaitForSingleObject(process_, 500U);
    if (wait == WAIT_OBJECT_0) {
        DWORD exit_code = 0;
        (void)GetExitCodeProcess(process_, &exit_code);
        CloseHandle(process_);
        process_ = nullptr;
        if (job_ != nullptr) { CloseHandle(job_); job_ = nullptr; }
        last_error_ = "The DPI packet processor exited during startup (exit code " + std::to_string(exit_code) + ")";
        return Result<void>::failure({last_error_, static_cast<int>(exit_code)});
    }
    if (wait == WAIT_FAILED) {
        const DWORD code = GetLastError();
        last_error_ = error_text("Could not confirm that the DPI packet processor started", code);
        return Result<void>::failure({last_error_, static_cast<int>(code)});
    }
    return Result<void>::success();
}

Result<void> DpiBackend::stop() {
    if (process_ == nullptr) return Result<void>::success();
    if (WaitForSingleObject(process_, 0U) == WAIT_TIMEOUT && !TerminateProcess(process_, 0U)) {
        const DWORD code = GetLastError();
        if (code != ERROR_ACCESS_DENIED) {
            last_error_ = error_text("Could not stop the DPI packet processor", code);
            return Result<void>::failure({last_error_, static_cast<int>(code)});
        }
    }
    (void)WaitForSingleObject(process_, 3000U);
    CloseHandle(process_);
    process_ = nullptr;
    if (job_ != nullptr) {
        CloseHandle(job_);
        job_ = nullptr;
    }
    last_error_.clear();
    return Result<void>::success();
}

} // namespace scpdpi

#else
namespace scpdpi {
DpiBackend::~DpiBackend() = default;
bool DpiBackend::running() { return false; }
Result<void> DpiBackend::start() {
    last_error_ = "DPI packet processing is currently available only on Windows";
    return Result<void>::failure({last_error_, 0});
}
Result<void> DpiBackend::stop() { return Result<void>::success(); }
} // namespace scpdpi
#endif
