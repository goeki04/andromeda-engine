#pragma once

/**
 * @file a_IWebsocketClient.hpp
 * @brief Interface definition for WebSocket client implementations.
 */

#include <string>
#include <functional>
#include "a_Primitives.hpp"
namespace Andromeda {
	/**
	 * @struct WebsocketReq
	 * @brief Describes the parameters needed to establish a WebSocket connection.
	 */
	struct WebsocketReq {
		i32 port;                          ///< Target port number of the WebSocket server.
		std::string host;                ///< Host or IP address of the WebSocket server.
		std::string path;                  ///< Request path/endpoint used for the WebSocket handshake.
	};

	/** @brief What happened to a connection, as reported to IWebsocketClient::onConnectionStateChanged(). */
	enum class ConnectionState {
		Connected,    ///< connect() went through every step; messages can be sent.
		Failed,       ///< connect() gave up; reason says at which step and why.
		Disconnected, ///< An established connection ended, by disconnect() or by the other side.
	};

	/**
	 * @class IWebsocketClient
	 * @brief Abstract interface for a WebSocket client.
	 *
	 * Defines the minimal contract a WebSocket client implementation must fulfill:
	 * connecting to and disconnecting from a server, sending payloads, and
	 * notifying callers of incoming messages via a callback.
	 */
	class IWebsocketClient {
	public:
		/** @brief Virtual destructor to allow safe destruction through the interface pointer. */
		virtual ~IWebsocketClient() = default;
		/** @brief Establishes the WebSocket connection to the configured server. */
		virtual void connect() = 0;

		/** @brief Closes the active WebSocket connection. */
		virtual void disconnect() = 0;

		/**
		 * @brief Sends a text payload over the WebSocket connection.
		 * @param payload The message content to transmit.
		 */
		virtual void send(const std::string& payload) = 0;

		/**
		 * @brief Registers a callback invoked whenever a message is received.
		 * @param callback Function called with the received message content.
		 */
		virtual void onMessageReceived(std::function<void(const std::string&)> callback) = 0;

		/**
		 * @brief Registers a callback invoked whenever the connection state changes.
		 * @details Every connect() ends in exactly one Connected or Failed. A Disconnected follows only a
		 *          Connected, once, however the connection ends. The reason is a human-readable message,
		 *          empty for Connected. Set it before calling connect().
		 * @param callback Function called with the new state and the reason. Runs on the network thread.
		 */
		virtual void onConnectionStateChanged(std::function<void(ConnectionState, const std::string&)> callback) = 0;

		/** @brief Returns whether the WebSocket connection is currently established. */
		virtual bool isConnected() const = 0;
	};
}