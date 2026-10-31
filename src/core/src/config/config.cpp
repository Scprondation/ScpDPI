#include "scpdpi/config/config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <system_error>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace scpdpi {
using nlohmann::json;

AppConfig ConfigStore::defaults() { return AppConfig{}; }

Result<AppConfig> ConfigStore::load(const std::filesystem::path& path) const {
    std::ifstream input(path, std::ios::binary);
    if (!input) return Result<AppConfig>::failure({"Cannot open configuration: " + path.string(), 0});
    try {
        const auto doc = json::parse(input);
        if (!doc.is_object()) return Result<AppConfig>::failure({"Configuration root must be an object", 0});
        AppConfig cfg = defaults();
        cfg.version = doc.value("version", cfg.version);
        if (cfg.version != "0.1.0") return Result<AppConfig>::failure({"Unsupported configuration version: " + cfg.version, 0});
        if (const auto it = doc.find("engine"); it != doc.end()) {
            if (!it->is_object()) return Result<AppConfig>::failure({"engine must be an object", 0});
            cfg.engine_enabled = it->value("enabled", cfg.engine_enabled);
            const auto interval = it->value("scan_interval_ms", cfg.scan_interval_ms);
            if (interval < 250U || interval > 60000U) return Result<AppConfig>::failure({"scan_interval_ms must be between 250 and 60000", 0});
            cfg.scan_interval_ms = interval;
        }
        if (const auto it = doc.find("classification"); it != doc.end()) {
            if (!it->is_object()) return Result<AppConfig>::failure({"classification must be an object", 0});
            cfg.unknown_action = it->value("unknown_action", cfg.unknown_action);
            if (cfg.unknown_action != "bypass") return Result<AppConfig>::failure({"unknown_action must be bypass in version 0.1.0", 0});
        }
        cfg.profile = doc.value("profile", cfg.profile);
        if (cfg.profile != "default" && cfg.profile != "vpn-safe" && cfg.profile != "diagnostic" && cfg.profile != "custom")
            return Result<AppConfig>::failure({"Unknown profile: " + cfg.profile, 0});
        if (const auto it = doc.find("logging"); it != doc.end()) {
            if (!it->is_object()) return Result<AppConfig>::failure({"logging must be an object", 0});
            cfg.log_level = it->value("level", cfg.log_level);
            cfg.file_logging = it->value("file_enabled", cfg.file_logging);
            if (cfg.log_level != "trace" && cfg.log_level != "debug" && cfg.log_level != "info" &&
                cfg.log_level != "warn" && cfg.log_level != "error")
                return Result<AppConfig>::failure({"Unsupported logging level: " + cfg.log_level, 0});
        }
        if (const auto it = doc.find("dpi"); it != doc.end()) {
            if (!it->is_object()) return Result<AppConfig>::failure({"dpi must be an object", 0});
            cfg.dpi_enabled = it->value("enabled", cfg.dpi_enabled);
        }
        return Result<AppConfig>::success(std::move(cfg));
    } catch (const json::exception& ex) {
        return Result<AppConfig>::failure({std::string("Invalid configuration JSON: ") + ex.what(), 0});
    } catch (const std::exception& ex) {
        return Result<AppConfig>::failure({std::string("Could not read configuration: ") + ex.what(), 0});
    }
}

Result<void> ConfigStore::save(const AppConfig& cfg, const std::filesystem::path& path) const {
    if (cfg.version != "0.1.0" || cfg.scan_interval_ms < 250U || cfg.scan_interval_ms > 60000U || cfg.unknown_action != "bypass")
        return Result<void>::failure({"Refusing to save an invalid configuration", 0});
    if (cfg.profile != "default" && cfg.profile != "vpn-safe" && cfg.profile != "diagnostic" && cfg.profile != "custom")
        return Result<void>::failure({"Unknown profile: " + cfg.profile, 0});
    if (cfg.log_level != "trace" && cfg.log_level != "debug" && cfg.log_level != "info" &&
        cfg.log_level != "warn" && cfg.log_level != "error")
        return Result<void>::failure({"Unsupported logging level: " + cfg.log_level, 0});
    try {
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        auto temp = path;
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
        const auto process_id = static_cast<unsigned long>(GetCurrentProcessId());
#else
        const auto process_id = static_cast<unsigned long>(getpid());
#endif
        temp += ".tmp." + std::to_string(process_id) + "." + std::to_string(nonce);
        const json doc = {
            {"version", cfg.version},
            {"engine", {{"enabled", cfg.engine_enabled}, {"scan_interval_ms", cfg.scan_interval_ms}}},
            {"classification", {{"unknown_action", cfg.unknown_action}}},
            {"profile", cfg.profile},
            {"dpi", {{"enabled", cfg.dpi_enabled}}},
            {"logging", {{"level", cfg.log_level}, {"file_enabled", cfg.file_logging}}}
        };
        {
            std::ofstream output(temp, std::ios::binary | std::ios::trunc);
            if (!output) return Result<void>::failure({"Cannot create temporary configuration: " + temp.string(), 0});
            output << doc.dump(2) << '\n';
            output.flush();
            if (!output) return Result<void>::failure({"Failed while writing temporary configuration", 0});
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const auto code = static_cast<int>(GetLastError());
            return Result<void>::failure({"Could not replace configuration file", code});
        }
#else
        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
        if (ec) return Result<void>::failure({"Could not replace configuration file: " + ec.message(), ec.value()});
#endif
        return Result<void>::success();
    } catch (const std::filesystem::filesystem_error& ex) {
        return Result<void>::failure({ex.what(), ex.code().value()});
    }
}
} // namespace scpdpi
