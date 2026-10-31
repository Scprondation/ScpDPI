#include "autostart.hpp"

#include <array>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <taskschd.h>
#include <oleauto.h>

namespace scpdpi {
namespace {
constexpr wchar_t task_name[] = L"ScpDPI DPI Background";

template <typename T>
class ComOwner {
public:
    ~ComOwner() { reset(); }
    ComOwner() = default;
    ComOwner(const ComOwner&) = delete;
    ComOwner& operator=(const ComOwner&) = delete;
    [[nodiscard]] T** put() { reset(); return &value_; }
    [[nodiscard]] T* get() const noexcept { return value_; }
    [[nodiscard]] T* operator->() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return value_ != nullptr; }
    void reset() noexcept { if (value_ != nullptr) { value_->Release(); value_ = nullptr; } }
private:
    T* value_{};
};

class BstrOwner {
public:
    explicit BstrOwner(const wchar_t* value) : value_(SysAllocString(value)) {}
    ~BstrOwner() { SysFreeString(value_); }
    BstrOwner(const BstrOwner&) = delete;
    BstrOwner& operator=(const BstrOwner&) = delete;
    [[nodiscard]] BSTR get() const noexcept { return value_; }
private:
    BSTR value_{};
};

class VariantOwner {
public:
    VariantOwner() { VariantInit(&value_); }
    ~VariantOwner() { VariantClear(&value_); }
    VariantOwner(const VariantOwner&) = delete;
    VariantOwner& operator=(const VariantOwner&) = delete;
    [[nodiscard]] VARIANT* get() noexcept { return &value_; }
    [[nodiscard]] VARIANT value() const noexcept { return value_; }
    void set_bstr(BSTR value) { value_.vt = VT_BSTR; value_.bstrVal = SysAllocString(value); }
private:
    VARIANT value_{};
};

class ComApartment {
public:
    ComApartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), initialized_(SUCCEEDED(result_)) {}
    ~ComApartment() { if (initialized_) CoUninitialize(); }
    [[nodiscard]] HRESULT result() const noexcept { return result_; }
private:
    HRESULT result_;
    bool initialized_;
};

Result<std::wstring> current_user_sid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return Result<std::wstring>::failure({"Could not inspect the current Windows user", static_cast<int>(GetLastError())});
    struct TokenGuard { HANDLE value; ~TokenGuard() { CloseHandle(value); } } token_guard{token};
    DWORD size = 0;
    (void)GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> buffer(size);
    if (size == 0U || !GetTokenInformation(token, TokenUser, buffer.data(), size, &size))
        return Result<std::wstring>::failure({"Could not read the current Windows user", static_cast<int>(GetLastError())});
    LPWSTR text = nullptr;
    const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
    if (!ConvertSidToStringSidW(user->User.Sid, &text))
        return Result<std::wstring>::failure({"Could not identify the current Windows user", static_cast<int>(GetLastError())});
    std::wstring sid(text);
    LocalFree(text);
    return Result<std::wstring>::success(std::move(sid));
}

Result<void> root_folder(ComOwner<ITaskService>& service, ComOwner<ITaskFolder>& folder) {
    const HRESULT created = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                              IID_ITaskService, reinterpret_cast<void**>(service.put()));
    if (FAILED(created)) return Result<void>::failure({"Could not connect to Windows Task Scheduler", static_cast<int>(created)});
    VariantOwner empty;
    const HRESULT connected = service->Connect(empty.value(), empty.value(), empty.value(), empty.value());
    if (FAILED(connected)) return Result<void>::failure({"Could not connect to Windows Task Scheduler", static_cast<int>(connected)});
    BstrOwner root(L"\\");
    const HRESULT opened = service->GetFolder(root.get(), folder.put());
    if (FAILED(opened)) return Result<void>::failure({"Could not open the Windows scheduled tasks folder", static_cast<int>(opened)});
    return Result<void>::success();
}

bool is_task_not_found(HRESULT result) {
    return result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
}

Result<bool> current_account_is_admin() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return Result<bool>::failure({"Could not inspect the current Windows account", static_cast<int>(GetLastError())});
    struct TokenGuard { HANDLE value; ~TokenGuard() { CloseHandle(value); } } token_guard{token};
    DWORD size = 0;
    (void)GetTokenInformation(token, TokenGroups, nullptr, 0, &size);
    std::vector<unsigned char> buffer(size);
    if (size == 0U || !GetTokenInformation(token, TokenGroups, buffer.data(), size, &size))
        return Result<bool>::failure({"Could not read the current Windows account groups", static_cast<int>(GetLastError())});
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &administrators))
        return Result<bool>::failure({"Could not check administrator membership", static_cast<int>(GetLastError())});
    const auto* groups = reinterpret_cast<const TOKEN_GROUPS*>(buffer.data());
    bool is_admin = false;
    for (DWORD index = 0; index < groups->GroupCount; ++index) {
        if (EqualSid(groups->Groups[index].Sid, administrators)) { is_admin = true; break; }
    }
    FreeSid(administrators);
    return Result<bool>::success(is_admin);
}

Result<std::wstring> registered_task_action_path() {
    ComApartment apartment;
    if (FAILED(apartment.result()) && apartment.result() != RPC_E_CHANGED_MODE)
        return Result<std::wstring>::failure({"Could not initialize Windows Task Scheduler", static_cast<int>(apartment.result())});
    ComOwner<ITaskService> service;
    ComOwner<ITaskFolder> folder;
    auto folder_result = root_folder(service, folder);
    if (!folder_result) return Result<std::wstring>::failure(folder_result.error());
    BstrOwner name(task_name);
    ComOwner<IRegisteredTask> task;
    const HRESULT found = folder->GetTask(name.get(), task.put());
    if (is_task_not_found(found)) return Result<std::wstring>::success({});
    if (FAILED(found)) return Result<std::wstring>::failure({"Could not inspect the ScpDPI background task", static_cast<int>(found)});
    ComOwner<ITaskDefinition> definition;
    HRESULT result = task->get_Definition(definition.put());
    if (FAILED(result)) return Result<std::wstring>::failure({"Could not inspect the ScpDPI background action", static_cast<int>(result)});
    ComOwner<IActionCollection> actions;
    result = definition->get_Actions(actions.put());
    if (FAILED(result)) return Result<std::wstring>::failure({"Could not inspect the ScpDPI background action", static_cast<int>(result)});
    ComOwner<IAction> generic_action;
    result = actions->get_Item(1, generic_action.put());
    if (FAILED(result)) return Result<std::wstring>::failure({"Could not inspect the ScpDPI background action", static_cast<int>(result)});
    ComOwner<IExecAction> action;
    result = generic_action->QueryInterface(IID_IExecAction, reinterpret_cast<void**>(action.put()));
    if (FAILED(result)) return Result<std::wstring>::failure({"The ScpDPI background task has an unsupported action", static_cast<int>(result)});
    BSTR path = nullptr;
    result = action->get_Path(&path);
    if (FAILED(result)) return Result<std::wstring>::failure({"Could not read the ScpDPI background action path", static_cast<int>(result)});
    std::wstring value = path == nullptr ? L"" : path;
    SysFreeString(path);
    return Result<std::wstring>::success(std::move(value));
}

Result<void> run_registered_task() {
    ComApartment apartment;
    if (FAILED(apartment.result()) && apartment.result() != RPC_E_CHANGED_MODE)
        return Result<void>::failure({"Could not initialize Windows Task Scheduler", static_cast<int>(apartment.result())});
    ComOwner<ITaskService> service;
    ComOwner<ITaskFolder> folder;
    auto folder_result = root_folder(service, folder);
    if (!folder_result) return Result<void>::failure(folder_result.error());
    BstrOwner name(task_name);
    ComOwner<IRegisteredTask> task;
    const HRESULT found = folder->GetTask(name.get(), task.put());
    if (FAILED(found)) return Result<void>::failure({"ScpDPI's elevated background task is not installed", static_cast<int>(found)});
    VariantOwner parameters;
    ComOwner<IRunningTask> running;
    const HRESULT started = task->Run(parameters.value(), running.put());
    if (FAILED(started)) return Result<void>::failure({"Windows could not start the ScpDPI background task", static_cast<int>(started)});
    return Result<void>::success();
}

Result<void> restart_registered_task() {
    ComApartment apartment;
    if (FAILED(apartment.result()) && apartment.result() != RPC_E_CHANGED_MODE)
        return Result<void>::failure({"Could not initialize Windows Task Scheduler", static_cast<int>(apartment.result())});
    ComOwner<ITaskService> service;
    ComOwner<ITaskFolder> folder;
    auto folder_result = root_folder(service, folder);
    if (!folder_result) return folder_result;
    BstrOwner name(task_name);
    ComOwner<IRegisteredTask> task;
    const HRESULT found = folder->GetTask(name.get(), task.put());
    if (FAILED(found)) return Result<void>::failure({"ScpDPI's elevated background task is not installed", static_cast<int>(found)});

    TASK_STATE state = TASK_STATE_UNKNOWN;
    HRESULT result = task->get_State(&state);
    if (FAILED(result)) return Result<void>::failure({"Could not inspect the ScpDPI background task state", static_cast<int>(result)});
    if (state == TASK_STATE_RUNNING || state == TASK_STATE_QUEUED) {
        result = task->Stop(0);
        if (FAILED(result)) return Result<void>::failure({"Could not stop the running ScpDPI background task", static_cast<int>(result)});
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            result = task->get_State(&state);
            if (FAILED(result)) return Result<void>::failure({"Could not wait for the ScpDPI background task to stop", static_cast<int>(result)});
            if (state != TASK_STATE_RUNNING && state != TASK_STATE_QUEUED) break;
            Sleep(100U);
        } while (std::chrono::steady_clock::now() < deadline);
        if (state == TASK_STATE_RUNNING || state == TASK_STATE_QUEUED)
            return Result<void>::failure({"ScpDPI background task did not stop in time", ERROR_TIMEOUT});
    }

    VariantOwner parameters;
    ComOwner<IRunningTask> running;
    result = task->Run(parameters.value(), running.put());
    if (FAILED(result)) return Result<void>::failure({"Windows could not restart the ScpDPI background task", static_cast<int>(result)});
    return Result<void>::success();
}

} // namespace

Result<void> install_elevated_background_task() {
    BOOL is_admin = FALSE;
    PSID administrators = nullptr;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &administrators))
        return Result<void>::failure({"Could not check administrator access", static_cast<int>(GetLastError())});
    const BOOL checked = CheckTokenMembership(nullptr, administrators, &is_admin);
    FreeSid(administrators);
    if (!checked || !is_admin)
        return Result<void>::failure({"Administrator permission is required to install the DPI packet driver task", static_cast<int>(GetLastError())});

    const auto protected_runtime = install_protected_runtime();
    if (!protected_runtime) return Result<void>::failure(protected_runtime.error());
    const auto install_dir = protected_runtime.value();
    const auto helper_path = install_dir / L"scpdpi_autostart.exe";
    std::error_code file_error;
    if (!std::filesystem::is_regular_file(helper_path, file_error) || file_error)
        return Result<void>::failure({"The ScpDPI background launcher is missing", file_error.value()});

    const auto user_sid = current_user_sid();
    if (!user_sid) return Result<void>::failure(user_sid.error());
    ComApartment apartment;
    if (FAILED(apartment.result()) && apartment.result() != RPC_E_CHANGED_MODE)
        return Result<void>::failure({"Could not initialize Windows Task Scheduler", static_cast<int>(apartment.result())});

    ComOwner<ITaskService> service;
    const HRESULT created = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                              IID_ITaskService, reinterpret_cast<void**>(service.put()));
    if (FAILED(created)) return Result<void>::failure({"Could not connect to Windows Task Scheduler", static_cast<int>(created)});
    VariantOwner empty;
    const HRESULT connected = service->Connect(empty.value(), empty.value(), empty.value(), empty.value());
    if (FAILED(connected)) return Result<void>::failure({"Could not connect to Windows Task Scheduler", static_cast<int>(connected)});
    BstrOwner root(L"\\");
    ComOwner<ITaskFolder> folder;
    const HRESULT opened = service->GetFolder(root.get(), folder.put());
    if (FAILED(opened)) return Result<void>::failure({"Could not open the Windows scheduled tasks folder", static_cast<int>(opened)});

    ComOwner<ITaskDefinition> definition;
    const HRESULT new_task = service->NewTask(0U, definition.put());
    if (FAILED(new_task)) return Result<void>::failure({"Could not create the ScpDPI background task", static_cast<int>(new_task)});

    ComOwner<IRegistrationInfo> registration;
    HRESULT result = definition->get_RegistrationInfo(registration.put());
    if (FAILED(result)) return Result<void>::failure({"Could not configure the ScpDPI background task", static_cast<int>(result)});
    BstrOwner author(L"ScpDPI");
    result = registration->put_Author(author.get());
    if (FAILED(result)) return Result<void>::failure({"Could not configure the ScpDPI background task", static_cast<int>(result)});

    ComOwner<IPrincipal> principal;
    result = definition->get_Principal(principal.put());
    if (FAILED(result)) return Result<void>::failure({"Could not configure the ScpDPI background task", static_cast<int>(result)});
    BstrOwner sid_for_principal(user_sid.value().c_str());
    result = principal->put_UserId(sid_for_principal.get());
    if (SUCCEEDED(result)) result = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    if (SUCCEEDED(result)) result = principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
    if (FAILED(result)) return Result<void>::failure({"Could not configure the ScpDPI background task to run with administrator privileges", static_cast<int>(result)});

    ComOwner<ITriggerCollection> triggers;
    result = definition->get_Triggers(triggers.put());
    if (FAILED(result)) return Result<void>::failure({"Could not configure ScpDPI's Windows startup trigger", static_cast<int>(result)});
    ComOwner<ITrigger> generic_trigger;
    result = triggers->Create(TASK_TRIGGER_LOGON, generic_trigger.put());
    if (FAILED(result)) return Result<void>::failure({"Could not configure ScpDPI's Windows startup trigger", static_cast<int>(result)});
    ComOwner<ILogonTrigger> logon_trigger;
    result = generic_trigger->QueryInterface(IID_ILogonTrigger, reinterpret_cast<void**>(logon_trigger.put()));
    if (SUCCEEDED(result)) {
        BstrOwner sid_for_trigger(user_sid.value().c_str());
        result = logon_trigger->put_UserId(sid_for_trigger.get());
    }
    if (SUCCEEDED(result)) {
        BstrOwner delay(L"PT5S");
        result = logon_trigger->put_Delay(delay.get());
    }
    if (FAILED(result)) return Result<void>::failure({"Could not configure ScpDPI's Windows startup trigger", static_cast<int>(result)});

    ComOwner<IActionCollection> actions;
    result = definition->get_Actions(actions.put());
    if (FAILED(result)) return Result<void>::failure({"Could not configure the ScpDPI background action", static_cast<int>(result)});
    ComOwner<IAction> generic_action;
    result = actions->Create(TASK_ACTION_EXEC, generic_action.put());
    if (FAILED(result)) return Result<void>::failure({"Could not configure the ScpDPI background action", static_cast<int>(result)});
    ComOwner<IExecAction> action;
    result = generic_action->QueryInterface(IID_IExecAction, reinterpret_cast<void**>(action.put()));
    if (SUCCEEDED(result)) {
        BstrOwner helper(helper_path.c_str());
        result = action->put_Path(helper.get());
    }
    if (SUCCEEDED(result)) {
        BstrOwner arguments(L"--serve");
        result = action->put_Arguments(arguments.get());
    }
    if (SUCCEEDED(result)) {
        BstrOwner working_directory(install_dir.c_str());
        result = action->put_WorkingDirectory(working_directory.get());
    }
    if (FAILED(result)) return Result<void>::failure({"Could not configure the ScpDPI background action", static_cast<int>(result)});

    ComOwner<ITaskSettings> settings;
    result = definition->get_Settings(settings.put());
    if (SUCCEEDED(result)) result = settings->put_StartWhenAvailable(VARIANT_TRUE);
    if (SUCCEEDED(result)) result = settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
    if (SUCCEEDED(result)) {
        BstrOwner limit(L"PT0S");
        result = settings->put_ExecutionTimeLimit(limit.get());
    }
    if (FAILED(result)) return Result<void>::failure({"Could not configure ScpDPI's background task lifetime", static_cast<int>(result)});

    BstrOwner name(task_name);
    VariantOwner user;
    user.set_bstr(sid_for_principal.get());
    VariantOwner password;
    VariantOwner sddl;
    ComOwner<IRegisteredTask> registered;
    result = folder->RegisterTaskDefinition(name.get(), definition.get(), TASK_CREATE_OR_UPDATE,
        user.value(), password.value(), TASK_LOGON_INTERACTIVE_TOKEN, sddl.value(), registered.put());
    if (FAILED(result)) return Result<void>::failure({"Could not save ScpDPI's elevated Windows startup task", static_cast<int>(result)});
    VariantOwner parameters;
    ComOwner<IRunningTask> running;
    result = registered->Run(parameters.value(), running.put());
    if (FAILED(result)) return Result<void>::failure({"ScpDPI was installed but Windows could not start its background task", static_cast<int>(result)});
    return Result<void>::success();
}

Result<void> ensure_elevated_background_task(const std::filesystem::path& install_dir, bool force_reinstall) {
    const auto expected = expected_protected_runtime_path(install_dir);
    if (!expected) return Result<void>::failure(expected.error());
    const auto registered = registered_task_action_path();
    if (!registered) return Result<void>::failure(registered.error());
    if (!force_reinstall && !registered.value().empty() && CompareStringOrdinal(registered.value().c_str(), -1,
            expected.value().c_str(), -1, TRUE) == CSTR_EQUAL) return run_registered_task();

    const auto admin_account = current_account_is_admin();
    if (!admin_account) return Result<void>::failure(admin_account.error());
    if (!admin_account.value())
        return Result<void>::failure({"The Windows account running ScpDPI must be an administrator to install its persistent DPI processor", ERROR_ACCESS_DENIED});

    const auto service_executable = install_dir / L"scpdpi_service.exe";
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = service_executable.c_str();
    info.lpParameters = L"--install-background-task";
    info.lpDirectory = install_dir.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        const DWORD code = GetLastError();
        const std::string message = code == ERROR_CANCELLED
            ? "ScpDPI needs one-time administrator approval to install the Windows packet processor"
            : "Could not request administrator approval for the Windows packet processor";
        return Result<void>::failure({message, static_cast<int>(code)});
    }
    if (info.hProcess == nullptr) return Result<void>::failure({"Windows did not start the ScpDPI elevated installer", 0});
    const DWORD wait = WaitForSingleObject(info.hProcess, 60000U);
    DWORD exit_code = 1U;
    if (wait == WAIT_OBJECT_0) (void)GetExitCodeProcess(info.hProcess, &exit_code);
    CloseHandle(info.hProcess);
    if (wait != WAIT_OBJECT_0) return Result<void>::failure({"Timed out while installing ScpDPI's elevated startup task", static_cast<int>(wait)});
    if (exit_code != 0U) return Result<void>::failure({"Could not install ScpDPI's elevated startup task (exit code " + std::to_string(exit_code) + ")", static_cast<int>(exit_code)});
    return Result<void>::success();
}

Result<void> restart_elevated_background_task() {
    return restart_registered_task();
}

Result<void> set_background_task_autostart(bool enabled) {
    ComApartment apartment;
    if (FAILED(apartment.result()) && apartment.result() != RPC_E_CHANGED_MODE)
        return Result<void>::failure({"Could not initialize Windows Task Scheduler", static_cast<int>(apartment.result())});
    ComOwner<ITaskService> service;
    ComOwner<ITaskFolder> folder;
    auto folder_result = root_folder(service, folder);
    if (!folder_result) return Result<void>::failure(folder_result.error());
    BstrOwner name(task_name);
    ComOwner<IRegisteredTask> task;
    HRESULT result = folder->GetTask(name.get(), task.put());
    if (FAILED(result)) return Result<void>::failure({"ScpDPI's elevated background task is not installed", static_cast<int>(result)});
    result = task->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
    if (FAILED(result)) return Result<void>::failure({"Could not update ScpDPI's Windows startup setting", static_cast<int>(result)});
    if (enabled) return run_registered_task();
    return Result<void>::success();
}

} // namespace scpdpi

#else
namespace scpdpi {
Result<void> install_elevated_background_task() {
    return Result<void>::failure({"Elevated Windows background tasks are not available on this platform", 0});
}
Result<void> ensure_elevated_background_task(const std::filesystem::path&, bool) {
    return Result<void>::failure({"Elevated Windows background tasks are not available on this platform", 0});
}
Result<void> restart_elevated_background_task() {
    return Result<void>::failure({"Elevated Windows background tasks are not available on this platform", 0});
}
Result<void> set_background_task_autostart(bool) {
    return Result<void>::failure({"Elevated Windows background tasks are not available on this platform", 0});
}
} // namespace scpdpi
#endif
