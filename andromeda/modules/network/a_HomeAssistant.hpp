#pragma once

/**
 * @file a_HomeAssistant.hpp
 * @brief Service handling the Home Assistant WebSocket API.
 */

#include "a_primitives.hpp"
#include "a_IWebsocketClient.hpp"
#include <string>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include "a_network_info.hpp"
#include <optional>
#include <mutex>
#include <memory>
#include <vector>
namespace Andromeda {
	/**
	 * @class HomeAssistantService
	 * @brief Connects to a Home Assistant instance via its WebSocket API.
	 *
	 * Owns the WebSocket client, performs the token based authentication
	 * handshake and requests entity states. Incoming messages are buffered in a
	 * mutex-protected queue by the network thread and processed on the main
	 * thread in update(), so all protocol logic runs single-threaded.
	 */
	class HomeAssistantService {
	public:
		/**
		 * @brief Constructs the service without connecting.
		 * @param ioContext Shared io_context passed on to the WebSocket client.
		 * @param sslContext TLS context used for the encrypted connection.
		 * @param proxySettings Optional HTTP proxy configuration.
		 */
		HomeAssistantService(std::shared_ptr<boost::asio::io_context> ioContext,
			boost::asio::ssl::context& sslContext,
			std::optional<ProxySettings> proxySettings)
			: m_IoContext(std::move(ioContext)), m_SslContext(sslContext), m_ProxySettings(std::move(proxySettings)) {}

		/** @brief Creates the WebSocket client, registers the receive queue and starts connecting. */
		void init();

		/** @brief Processes all queued messages. Must be called once per frame on the main thread. */
		void update();

		/**
		 * @brief Reads the Home Assistant access token from secrets.json.
		 * @return The token, or an empty string if it could not be read.
		 */
		std::string readHomeAssistantTokenFromSecretsJson();
	private:
		std::shared_ptr<boost::asio::io_context> m_IoContext;	///< io_context shared with the WebSocket client.
		boost::asio::ssl::context& m_SslContext;				///< TLS context supplied by the NetworkManager.
		std::optional<ProxySettings> m_ProxySettings;			///< Optional HTTP proxy configuration.
		i32 m_MessageID = 1;									///< Incrementing id required by the Home Assistant command protocol.
		bool m_Authenticated = false;							///< True once the authentication handshake succeeded.
		std::shared_ptr<IWebsocketClient> m_WebsocketClient;	///< Underlying WebSocket connection.
		std::mutex m_QueueMutex;								///< Protects m_MessageQueue between network and main thread.
		std::vector<std::string> m_MessageQueue;				///< Messages received on the network thread, drained in update().

		/**
		 * @brief Handles the auth_required/auth_ok/auth_invalid protocol messages.
		 * @param messageType The value of the message's "type" field.
		 */
		void handleAuthMessage(const std::string& messageType);

		/**
		 * @brief Parses and dispatches a single received message. Runs on the main thread.
		 * @param message Raw JSON message received from Home Assistant.
		 */
		void handleMessage(const std::string& message);
	};
}
