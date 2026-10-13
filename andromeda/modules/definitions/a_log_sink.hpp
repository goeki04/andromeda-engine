#pragma once

/**
 * @file a_log_sink.hpp
 * @brief spdlog sink that hands log lines to the in-engine console.
 */

#include <mutex>
#include <string>

#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>

#include "a_threadSafeQueue.hpp"

namespace Andromeda::Log {

    /** @brief One formatted log line, with the level kept separately so the UI can colour it. */
    struct LogLine {
        spdlog::level::level_enum level = spdlog::level::info;
        std::string text;
    };

    /**
     * @brief The queue every log line passes through on its way to the editor console.
     *
     * @details This is the thread boundary. Logging happens on whatever thread produced the
     *          message - the file watcher, the TCP client, Boost.Asio's io_context - while the
     *          console is ImGui and therefore main-thread only. The sink below pushes here, the
     *          console panel drains it once per frame, and nothing else is shared.
     *
     *          A function-local static rather than a global object: it is then built on first
     *          use, which matters because Log::init() runs before main() has done anything.
     */
    inline ThreadSafeQueue<LogLine>& pendingLines() {
        static ThreadSafeQueue<LogLine> queue;
        return queue;
    }

    /**
     * @class ConsoleSink
     * @brief Formats a log message and queues it for the editor console.
     *
     * @details Registered alongside the stdout and file sinks in @c Log::init(), so a message
     *          reaches all three. Lines produced before the editor exists are not lost - they
     *          sit in the queue until the console panel first drains it.
     */
    template<typename Mutex>
    class ConsoleSink final : public spdlog::sinks::base_sink<Mutex> {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            this->formatter_->format(msg, formatted);

            // Built from the buffer's bytes rather than through fmt::to_string, so this header
            // does not care whether spdlog was built against bundled or external fmt.
            std::string text(formatted.data(), formatted.size());

            // The pattern ends every line with a newline, which ImGui would render as a blank
            // row under each entry.
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
                text.pop_back();
            }

            pendingLines().push(LogLine{msg.level, std::move(text)});
        }

        void flush_() override {}
    };

    using ConsoleSinkMt = ConsoleSink<std::mutex>;
}
