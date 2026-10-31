#include "scpdpi/common/logger.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace scpdpi {
Result<void> Logger::initialize(const std::filesystem::path& log_file, const std::string& level, bool file_enabled) {
    try {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        if (file_enabled) {
            if (!log_file.parent_path().empty()) std::filesystem::create_directories(log_file.parent_path());
            sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file.string(), true));
        }
        auto logger = std::make_shared<spdlog::logger>("scpdpi", sinks.begin(), sinks.end());
        if (level == "trace") logger->set_level(spdlog::level::trace);
        else if (level == "debug") logger->set_level(spdlog::level::debug);
        else if (level == "info") logger->set_level(spdlog::level::info);
        else if (level == "warn") logger->set_level(spdlog::level::warn);
        else if (level == "error") logger->set_level(spdlog::level::err);
        else return Result<void>::failure({"Unsupported logging level: " + level, 0});
        spdlog::set_default_logger(std::move(logger));
        return Result<void>::success();
    } catch (const spdlog::spdlog_ex& ex) {
        return Result<void>::failure({ex.what(), 0});
    } catch (const std::filesystem::filesystem_error& ex) {
        return Result<void>::failure({ex.what(), ex.code().value()});
    }
}
} // namespace scpdpi
