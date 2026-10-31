#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::array<wchar_t, 32768> path_buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
    if (length == 0U || length >= path_buffer.size()) return 1;

    std::wstring directory(path_buffer.data(), length);
    const auto separator = directory.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return 1;
    directory.resize(separator + 1U);
    const std::wstring service = directory + L"scpdpi_service.exe";
    std::wstring command = L"\"" + service + L"\" --serve";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(service.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process)) return 2;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
