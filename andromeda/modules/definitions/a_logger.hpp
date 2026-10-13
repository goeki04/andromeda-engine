#pragma once

/**
 * @file a_logger.hpp
 * @brief Central logging facility for the Andromeda Engine, built on spdlog.
 *
 * Lives in the header-only `definitions` interface library, so every module that
 * links `definitions` (i.e. all of them) can log without extra CMake wiring.
 *
 * Usage:
 *   Andromeda::Log::init();                 // once, early in main()
 *   A_INFO("Loaded {} meshes", count);
 *   A_WARN("mesh '{}' not found", name);
 *   A_ERROR("VAO {} missing for entity {}", vao, id);
 *
 * Formatting is fmt/std::format style ("{}"), provided by spdlog.
 */

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "a_log_sink.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Andromeda::Log {

    /**
     * @brief Initializes the global logger (colored console + optional file sink).
     * @param logFile Path of the plain-text log file. If it cannot be opened,
     *                logging continues to the console only.
     * @note Safe to call multiple times; only the first call has an effect.
     */
    inline void init(const std::string& logFile = "andromeda.log") {
        static bool initialized = false;
        if (initialized) return;
        initialized = true;

        try {
            std::vector<spdlog::sink_ptr> sinks;

            auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console->set_pattern("%^[%T] [%l]%$ %v");
            sinks.push_back(console);

            try {
                auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
                file->set_pattern("[%Y-%m-%d %T.%e] [%l] %v");
                sinks.push_back(file);
            } catch (const spdlog::spdlog_ex&) {
                // File sink is optional - keep going with the console only.
            }

            // Third sink: feeds the editor's own console. Deliberately after the other two,
            // so a crash inside the editor still leaves a complete stdout and file log.
            auto console_panel = std::make_shared<ConsoleSinkMt>();
            console_panel->set_pattern("[%T] [%l] %v");
            sinks.push_back(console_panel);

            auto logger = std::make_shared<spdlog::logger>("Andromeda", sinks.begin(), sinks.end());
#ifdef NDEBUG
            logger->set_level(spdlog::level::info);
#else
            logger->set_level(spdlog::level::trace);
#endif
            logger->flush_on(spdlog::level::warn);
            spdlog::set_default_logger(logger);
        } catch (...) {
            // Logging setup must never crash the application.
        }
    }

} // namespace Andromeda::Log

// Convenience macros that forward to the global (default) logger.
#define A_TRACE(...)    ::spdlog::trace(__VA_ARGS__)
#define A_DEBUG(...)    ::spdlog::debug(__VA_ARGS__)
#define A_INFO(...)     ::spdlog::info(__VA_ARGS__)
#define A_WARN(...)     ::spdlog::warn(__VA_ARGS__)
#define A_ERROR(...)    ::spdlog::error(__VA_ARGS__)
#define A_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
