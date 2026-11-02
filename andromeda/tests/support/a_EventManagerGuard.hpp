#pragma once

/**
 * @file a_EventManagerGuard.hpp
 * @brief Listener registration at the global EventManager that cleans up after itself.
 */

#include <utility>
#include <vector>
#include "a_event_manager.hpp"

namespace Andromeda::Test {

    /**
     * @class EventManagerGuard
     * @brief Registers listeners at EventManager::getInstance() and removes them at scope end.
     * @details The EventManager is a process-wide singleton. A listener a test forgets to remove keeps
     *          firing in every later test of the same executable and makes failures depend on the
     *          order the tests run in. Registering through the guard rules that out.
     */
    class EventManagerGuard {
    public:
        EventManagerGuard() = default;
        EventManagerGuard(const EventManagerGuard&) = delete;
        EventManagerGuard& operator=(const EventManagerGuard&) = delete;

        ~EventManagerGuard() {
            for (const EventListenerID& id : m_Listeners) {
                EventManager::getInstance().RemoveEventListener(id);
            }
        }

        /** @brief Same as EventManager::AddEventListener<T>(), plus removal when the guard ends. */
        template<typename T, typename F>
        EventListenerID listen(F&& callback) {
            const EventListenerID id = EventManager::getInstance().AddEventListener<T>(std::forward<F>(callback));
            m_Listeners.push_back(id);
            return id;
        }

    private:
        std::vector<EventListenerID> m_Listeners;
    };
}
