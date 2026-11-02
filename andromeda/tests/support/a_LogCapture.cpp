#include "a_LogCapture.hpp"
#include <spdlog/sinks/base_sink.h>
#include <algorithm>
#include <mutex>
#include <vector>

namespace Andromeda::Test {

    /** @brief Keeps level and text of every message; the base class serializes access with its mutex. */
    class LogCapture::Sink final : public spdlog::sinks::base_sink<std::mutex> {
    public:
        struct Entry {
            spdlog::level::level_enum level;
            std::string text;
        };

        std::vector<Entry> entries() {
            std::lock_guard lock(mutex_);
            return m_Entries;
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            m_Entries.push_back({ msg.level, std::string(msg.payload.data(), msg.payload.size()) });
        }
        void flush_() override {}

    private:
        std::vector<Entry> m_Entries;
    };

    LogCapture::LogCapture()
        : m_Sink(std::make_shared<Sink>()), m_Previous(spdlog::default_logger()) {
        auto logger = std::make_shared<spdlog::logger>("test", m_Sink);
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(std::move(logger));
    }

    LogCapture::~LogCapture() {
        spdlog::set_default_logger(m_Previous);
    }

    std::string LogCapture::text() const {
        std::string all;
        for (const Sink::Entry& entry : m_Sink->entries()) {
            const auto level = spdlog::level::to_string_view(entry.level);
            all.append("[").append(level.data(), level.size()).append("] ").append(entry.text).append("\n");
        }
        return all;
    }

    bool LogCapture::contains(const std::string_view fragment) const {
        const std::vector<Sink::Entry> entries = m_Sink->entries();
        return std::ranges::any_of(entries, [fragment](const Sink::Entry& entry) {
            return entry.text.find(fragment) != std::string::npos;
        });
    }

    std::size_t LogCapture::count(const spdlog::level::level_enum level) const {
        const std::vector<Sink::Entry> entries = m_Sink->entries();
        return static_cast<std::size_t>(std::ranges::count_if(entries, [level](const Sink::Entry& entry) {
            return entry.level == level;
        }));
    }
}
