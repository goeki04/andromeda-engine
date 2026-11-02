#include "a_MockWebsocketServer.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace Andromeda::Test {
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    using tcp = net::ip::tcp;

    /** @brief The server's coroutines; a nested class so they may touch the private state. */
    class MockWebsocketServer::Coroutines {
    public:
        static net::awaitable<void> acceptLoop(MockWebsocketServer& server) {
            try {
                for (;;) {
                    tcp::socket socket = co_await server.m_Acceptor.async_accept(net::use_awaitable);
                    net::co_spawn(server.m_Io, session(server, std::move(socket)), net::detached);
                }
            } catch (...) {
                // The acceptor was closed: the server is shutting down.
            }
        }

        static net::awaitable<void> session(MockWebsocketServer& server, tcp::socket socket) {
            auto stream = std::make_shared<Stream>(std::move(socket), server.m_Tls);
            try {
                co_await stream->next_layer().async_handshake(net::ssl::stream_base::server, net::use_awaitable);

                if (server.m_Options.rejectUpgrade) {
                    beast::flat_buffer buffer;
                    http::request<http::string_body> request;
                    co_await http::async_read(stream->next_layer(), buffer, request, net::use_awaitable);
                    http::response<http::string_body> response{ http::status::forbidden, request.version() };
                    response.prepare_payload();
                    co_await http::async_write(stream->next_layer(), response, net::use_awaitable);
                    co_return;
                }

                co_await stream->async_accept(net::use_awaitable);
                server.m_Stream = stream;
                {
                    std::lock_guard lock(server.m_Mutex);
                    ++server.m_AcceptedConnections;
                }
                server.m_Changed.notify_all();

                beast::flat_buffer buffer;
                for (;;) {
                    co_await stream->async_read(buffer, net::use_awaitable);
                    std::string text = beast::buffers_to_string(buffer.data());
                    buffer.consume(buffer.size());
                    {
                        std::lock_guard lock(server.m_Mutex);
                        server.m_Messages.push_back(text);
                    }
                    server.m_Changed.notify_all();
                    send(server, std::move(text));
                }
            } catch (const beast::system_error& error) {
                if (error.code() == websocket::error::closed) {
                    {
                        std::lock_guard lock(server.m_Mutex);
                        server.m_ClientClosed = true;
                    }
                    server.m_Changed.notify_all();
                }
            } catch (...) {
                // A failed TLS handshake or a dropped connection - exactly what some tests provoke.
            }
            if (server.m_Stream == stream) {
                server.m_Stream.reset();
            }
        }

        /** @brief Queues @p text and starts the writer if it is idle. Server thread only. */
        static void send(MockWebsocketServer& server, std::string text) {
            server.m_Outbox.push_back(std::move(text));
            if (!server.m_Writing && server.m_Stream) {
                server.m_Writing = true;
                net::co_spawn(server.m_Io, drainOutbox(server, server.m_Stream), net::detached);
            }
        }

        static net::awaitable<void> drainOutbox(MockWebsocketServer& server, std::shared_ptr<Stream> stream) {
            try {
                while (!server.m_Outbox.empty()) {
                    const std::string text = std::move(server.m_Outbox.front());
                    server.m_Outbox.pop_front();
                    co_await stream->async_write(net::buffer(text), net::use_awaitable);
                }
            } catch (...) {
                server.m_Outbox.clear();
            }
            server.m_Writing = false;
        }

        static net::awaitable<void> close(std::shared_ptr<Stream> stream) {
            try {
                co_await stream->async_close(websocket::close_code::normal, net::use_awaitable);
            } catch (...) {}
        }
    };

    MockWebsocketServer::MockWebsocketServer(const TestCertificates& certificates, const Options options)
        : m_Options(options) {
        certificates.useAsServer(m_Tls);
        const tcp::endpoint endpoint(net::ip::make_address("127.0.0.1"), 0);
        m_Acceptor.open(endpoint.protocol());
        m_Acceptor.bind(endpoint);
        m_Acceptor.listen();
        m_Port = m_Acceptor.local_endpoint().port();

        net::co_spawn(m_Io, Coroutines::acceptLoop(*this), net::detached);
        m_Thread = std::thread([this] { m_Io.run(); });
    }

    MockWebsocketServer::~MockWebsocketServer() {
        m_Io.stop();
        m_Thread.join();
        m_Stream.reset();
    }

    void MockWebsocketServer::push(const std::string& text) {
        net::post(m_Io, [this, text] { Coroutines::send(*this, text); });
    }

    void MockWebsocketServer::closeConnection() {
        net::post(m_Io, [this] {
            if (m_Stream) {
                net::co_spawn(m_Io, Coroutines::close(m_Stream), net::detached);
            }
        });
    }

    bool MockWebsocketServer::waitForMessages(const std::size_t count, const std::chrono::milliseconds timeout) const {
        std::unique_lock lock(m_Mutex);
        return m_Changed.wait_for(lock, timeout, [&] { return m_Messages.size() >= count; });
    }

    bool MockWebsocketServer::waitForClientClose(const std::chrono::milliseconds timeout) const {
        std::unique_lock lock(m_Mutex);
        return m_Changed.wait_for(lock, timeout, [&] { return m_ClientClosed; });
    }

    std::vector<std::string> MockWebsocketServer::messages() const {
        std::lock_guard lock(m_Mutex);
        return m_Messages;
    }

    int MockWebsocketServer::acceptedConnections() const {
        std::lock_guard lock(m_Mutex);
        return m_AcceptedConnections;
    }
}
