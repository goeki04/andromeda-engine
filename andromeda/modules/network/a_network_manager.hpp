#pragma once

/**
 * @file a_network_manager.hpp
 * @brief Subsystem owning the networking runtime of the engine.
 */

#include "a_subsystem_manager.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <memory>
#include "a_HomeAssistant.hpp"
#include "a_network_info.hpp"
#include <optional>
#include <thread>
#include <boost/asio/executor_work_guard.hpp>
#include "a_threadSafeQueue.hpp"
#include <string>
namespace Andromeda {
	/**
	 * @class NetworkManager
	 * @brief Subsystem that owns the io_context, TLS context and network thread.
	 *
	 * Creates the shared io_context and TLS context, starts the network services
	 * and runs the io_context on a dedicated network thread. update() drains the
	 * services' message queues on the main thread once per frame, so all game
	 * facing logic stays single-threaded.
	 */
	class NetworkManager : public ISubsystem {
		public:
			/** @brief Sets up TLS and proxy configuration, starts the services and launches the network thread. */
			void start() override;

			/** @brief Stops the io_context and joins the network thread. */
			void destroy() override;

			/** @brief Forwards queued network messages to the services on the main thread. Called once per frame. */
			void update() override;
			static constexpr std::string_view GetStaticName() { return "NetworkManager"; }
			/**
			 * @brief Gets the runtime string identifier of the subsystem.
			 * @return A C-string containing the subsystem's name.
			 */
			const char* getSubsystemName() const override {
				return GetStaticName().data();
			}
	private:
		std::shared_ptr<boost::asio::io_context> m_IoContext;			///< Event loop shared with all network services.
		boost::asio::ssl::context m_SslContext{boost::asio::ssl::context::tls_client};	///< TLS context with certificate verification enabled.
		std::unique_ptr<HomeAssistantService> m_HomeAssistantService;	///< Home Assistant WebSocket service.
		std::optional<ProxySettings> m_ProxySettings;					///< Proxy configuration read from the environment.
		std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_WorkGuard;	///< Keeps io_context::run() alive while no work is pending.
		std::jthread m_NetworkThread;									///< Dedicated thread executing the io_context.
		ThreadSafeQueue<std::string> m_SensorEventQueue;	///< Raw sensor lines, parsed and dispatched in update().
	};
}
