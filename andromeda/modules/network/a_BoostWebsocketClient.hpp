#pragma once

/**
 * @file a_BoostWebsocketClient.hpp
 * @brief Boost.Beast based implementation of the IWebsocketClient interface.
 */

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <atomic>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <string>
#include "a_network_info.hpp"
#include "a_IWebsocketClient.hpp"
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace Andromeda {

	/**
	 * @class BoostWebsocketClient
	 * @brief WebSocket client built on Boost.Beast with TLS and HTTP proxy support.
	 *
	 * Establishes a TLS-secured WebSocket connection, optionally tunneled through
	 * an HTTP proxy via CONNECT. All asynchronous work runs as C++20 coroutines on
	 * the stream's strand, making the public methods safe to call from any thread.
	 * Instances must be owned by a std::shared_ptr, because the pending coroutines
	 * keep the object alive through shared_from_this().
	 */
	class BoostWebsocketClient : public IWebsocketClient, public std::enable_shared_from_this<BoostWebsocketClient> {
	public:
		/**
		 * @brief Constructs the client without opening a connection.
		 * @param io_context Shared io_context executing all asynchronous operations.
		 * @param ssl_context TLS context used for encryption and certificate verification.
		 * @param req Host, port and path used for the WebSocket handshake.
		 * @param proxy_settings Optional HTTP proxy; when present the connection is tunneled via CONNECT.
		 */
		BoostWebsocketClient(std::shared_ptr<boost::asio::io_context> io_context, boost::asio::ssl::context& ssl_context, WebsocketReq req,const std::optional<ProxySettings>& proxy_settings)
			: m_IoContext(io_context), m_SslContext(ssl_context), m_ProxySettings(proxy_settings), m_Req(req), m_Stream(net::make_strand(*m_IoContext), m_SslContext) {}

		/** @brief Starts the asynchronous connect sequence (TCP, optional proxy tunnel, TLS and WebSocket handshake). Returns immediately. */
		void connect() override;

		/** @brief Asynchronously closes the connection with a normal WebSocket close frame. */
		void disconnect() override;

		/**
		 * @brief Queues a text payload for asynchronous transmission.
		 * @param payload The message content to transmit.
		 */
		void send(const std::string& payload) override;

		/** @brief Returns whether the WebSocket connection is currently established. */
		bool isConnected() const override { return m_IsConnected.load(std::memory_order_acquire); }

		/**
		 * @brief Registers a callback invoked for every received message.
		 * @param callback Function called with the message content. Runs on the network thread.
		 */
		void onMessageReceived(std::function<void(const std::string&)> callback) override;

		/** @copydoc IWebsocketClient::onConnectionStateChanged */
		void onConnectionStateChanged(std::function<void(ConnectionState, const std::string&)> callback) override;
	private:
		std::shared_ptr<boost::asio::io_context> m_IoContext;	///< Keeps the io_context alive for the lifetime of the client.
		boost::asio::ssl::context& m_SslContext;				///< TLS context supplied by the owner.
		std::atomic<bool> m_IsConnected{ false };				///< Connection state, shared between threads.
		std::optional<ProxySettings> m_ProxySettings;			///< Optional HTTP proxy configuration.
		WebsocketReq m_Req;										///< Target host, port and handshake path.
		websocket::stream<beast::ssl_stream<beast::tcp_stream>> m_Stream;	///< TLS WebSocket stream running on its own strand.

		std::function<void(const std::string&)> m_MessageCallback;	///< Callback invoked by the read loop for each message.
		std::function<void(ConnectionState, const std::string&)> m_StateCallback; ///< Callback for connection state changes.

		/** @brief Forwards a state change to the state callback, if one is set. */
		void notifyState(ConnectionState state, const std::string& reason) const;

		/**
		 * @brief Marks an established connection as ended and reports Disconnected - once.
		 * @details Both a disconnect() and the read loop seeing the closed stream end up here; only the
		 *          first caller finds the flag still set, so the callback fires a single time.
		 */
		void markDisconnected(const std::string& reason);

		/** @brief Spawns the read loop coroutine on the stream's strand. */
		void startReadLoop();

		/** @brief Coroutine performing resolve, TCP connect, optional proxy CONNECT tunnel, TLS and WebSocket handshake. */
		static net::awaitable<void> doConnect(std::shared_ptr<BoostWebsocketClient> self);

		/** @brief Coroutine closing the WebSocket connection. */
		static net::awaitable<void> doDisconnect(std::shared_ptr<BoostWebsocketClient> self);

		/** @brief Coroutine writing a single payload to the stream. */
		static net::awaitable<void> doSend(std::shared_ptr<BoostWebsocketClient> self, std::string payload);

		/** @brief Coroutine reading messages in a loop and forwarding them to the message callback. */
		static net::awaitable<void> doReadLoop(std::shared_ptr<BoostWebsocketClient> self);
	};
}
