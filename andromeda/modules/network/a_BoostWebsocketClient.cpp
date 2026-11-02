#include <boost/asio/co_spawn.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/asio/detached.hpp>
#include <chrono>
#include "a_logger.hpp"
#include "a_BoostWebsocketClient.hpp"
#include "a_network_info.hpp"
#include <memory>
#include <boost/asio/buffer.hpp>
#include "a_Primitives.hpp"
namespace Andromeda
{
	void BoostWebsocketClient::connect() {
		net::co_spawn(m_Stream.get_executor(),doConnect(shared_from_this()), net::detached);
	}

	net::awaitable<void> BoostWebsocketClient::doConnect(std::shared_ptr<BoostWebsocketClient> self) {
		try {
			auto executor = co_await net::this_coro::executor;
			auto resolver = net::ip::tcp::resolver(executor);
			bool hasProxy = NetworkInfo::hasProxysettings(self->m_ProxySettings);
			beast::get_lowest_layer(self->m_Stream).expires_after(std::chrono::seconds(30));

			std::string host = hasProxy ? self->m_ProxySettings->host : self->m_Req.host;
			std::string port = std::to_string(hasProxy ? self->m_ProxySettings->port : self->m_Req.port);

			auto const results = co_await resolver.async_resolve(host, port, net::use_awaitable);
			co_await beast::get_lowest_layer(self->m_Stream).async_connect(results, net::use_awaitable); //implicit TCP handshake

			if (hasProxy) {
				http::request<http::empty_body> req{ http::verb::connect, self->m_Req.host + ":" + std::to_string(self->m_Req.port), 11 };
				req.set(http::field::host, self->m_Req.host);
				req.set(http::field::proxy_connection, "Keep-Alive");
				req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coo");
				beast::get_lowest_layer(self->m_Stream).expires_after(std::chrono::seconds(30));
				co_await http::async_write(beast::get_lowest_layer(self->m_Stream), req, net::use_awaitable);

				beast::flat_buffer buffer;
				http::response_parser<http::empty_body> parser;
				parser.skip(true);
				beast::get_lowest_layer(self->m_Stream).expires_after(std::chrono::seconds(30));
				co_await http::async_read(beast::get_lowest_layer(self->m_Stream), buffer, parser, net::use_awaitable);

				auto const& res = parser.get();
				if (res.result() != http::status::ok) {
					throw std::runtime_error("Proxy tunnel failed: " + std::string(res.reason()));
				}
			}

			if (!SSL_set_tlsext_host_name(self->m_Stream.next_layer().native_handle(), self->m_Req.host.c_str())) {
				throw beast::system_error{ beast::error_code{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()} };
			}
			if (!SSL_set1_host(self->m_Stream.next_layer().native_handle(), self->m_Req.host.c_str())) {
				throw std::runtime_error("Failed to set expected hostname for certificate verification");
			}
			beast::get_lowest_layer(self->m_Stream).expires_after(std::chrono::seconds(30));
			co_await self->m_Stream.next_layer().async_handshake(net::ssl::stream_base::client, net::use_awaitable);	//SSL handshake
			std::string hostHeader = self->m_Req.host + ":" + std::to_string(self->m_Req.port);
			std::string path = self->m_Req.path.empty() ? "/" : self->m_Req.path;
			beast::get_lowest_layer(self->m_Stream).expires_never();
			self->m_Stream.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
			self->m_Stream.set_option(websocket::stream_base::decorator(
				[](websocket::request_type& req) {
					req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coo");
				}
			));
			co_await self->m_Stream.async_handshake(hostHeader, path, net::use_awaitable); //WebSocket handshake
			self->m_IsConnected.store(true, std::memory_order_release);
			self->notifyState(ConnectionState::Connected, {});
			self->startReadLoop();

			A_INFO("Successfully connected to WebSocket: {}", self->m_Req.host);
		}
		catch (const std::exception& e) {
			//A_ERROR("Connection failed: {}", e.what());
			self->notifyState(ConnectionState::Failed, e.what());
		}
	}

	void BoostWebsocketClient::disconnect() {
		net::co_spawn(m_Stream.get_executor(),doDisconnect(shared_from_this()), net::detached);
	}

	net::awaitable<void> BoostWebsocketClient::doDisconnect(std::shared_ptr<BoostWebsocketClient> self) {
		try {
			co_await self->m_Stream.async_close(websocket::close_code::normal, net::use_awaitable);
			A_INFO("Successfully disconnected cleanly: {}", self->m_Req.host);
			self->markDisconnected("closed by this side");
		}
		catch (const std::exception& e) {
			A_ERROR("WebSocket closed: ({})", e.what());
			self->markDisconnected(e.what());
		}
	}

	void BoostWebsocketClient::send(const std::string& payload) {
		if (!m_IsConnected) {
			A_WARN("Cannot send message, Websocket is not Connected");
			return;
		}
		net::co_spawn(m_Stream.get_executor(),doSend(shared_from_this(), payload), net::detached);
	}

	net::awaitable<void> BoostWebsocketClient::doSend(std::shared_ptr<BoostWebsocketClient> self, std::string payload) {
		try {
			co_await self->m_Stream.async_write(net::buffer(payload), net::use_awaitable);
		}
		catch (const std::exception& e) {
			A_ERROR("Send failed: {}", e.what());
			self->markDisconnected(e.what());
		}
	}

	void BoostWebsocketClient::onMessageReceived(std::function<void(const std::string&)> callback) {
		m_MessageCallback = callback;
	}

	void BoostWebsocketClient::onConnectionStateChanged(std::function<void(ConnectionState, const std::string&)> callback) {
		m_StateCallback = std::move(callback);
	}

	void BoostWebsocketClient::notifyState(const ConnectionState state, const std::string& reason) const {
		if (m_StateCallback) {
			m_StateCallback(state, reason);
		}
	}

	void BoostWebsocketClient::markDisconnected(const std::string& reason) {
		if (m_IsConnected.exchange(false, std::memory_order_acq_rel)) {
			notifyState(ConnectionState::Disconnected, reason);
		}
	}

	void BoostWebsocketClient::startReadLoop()
	{
		net::co_spawn(m_Stream.get_executor(),doReadLoop(shared_from_this()), net::detached);
	}

	net::awaitable<void> BoostWebsocketClient::doReadLoop(std::shared_ptr<BoostWebsocketClient> self) {
		try {
			beast::flat_buffer buffer;
			while (self->m_IsConnected.load(std::memory_order_acquire)) {
				co_await self->m_Stream.async_read(buffer, net::use_awaitable);
				std::string message = beast::buffers_to_string(buffer.data());
				buffer.consume(buffer.size());
				if (self->m_MessageCallback) {
					self->m_MessageCallback(message);
				}
			}
		}
		catch (const std::exception& e) {
			A_ERROR("Read loop error: {}", e.what());
			self->markDisconnected(e.what());
		}
	}
}
