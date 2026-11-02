#pragma once

/**
 * @file a_LogCapture.hpp
 * @brief Records what the engine logs while a test runs.
 */

#include <spdlog/spdlog.h>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace Andromeda::Test {

    /**
     * @class LogCapture
     * @brief Collects every message logged through A_INFO, A_WARN, ... while it exists.
     * @details The A_* macros write to spdlog's default logger. LogCapture puts a logger in its place
     *          that keeps the messages in memory and restores the previous one when it is destroyed.
     *          That lets a test check that a problem was reported, not only that nothing crashed -
     *          e.g. that a scene entity whose mesh is gone produced a warning.
     */
    class LogCapture {
    public:
        LogCapture();
        ~LogCapture();
        LogCapture(const LogCapture&) = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        /** @brief All messages so far, one per line, prefixed with their level (e.g. "[warning] ..."). */
        [[nodiscard]] std::string text() const;

        /** @brief True if any message contains @p fragment. */
        [[nodiscard]] bool contains(std::string_view fragment) const;

        /** @brief Number of messages logged at exactly @p level. */
        [[nodiscard]] std::size_t count(spdlog::level::level_enum level) const;

    private:
        class Sink;
        std::shared_ptr<Sink> m_Sink;
        std::shared_ptr<spdlog::logger> m_Previous;
    };
}
