#pragma once

/**
 * @file a_MockWebsocketServer.hpp
 * @brief An in-process TLS WebSocket server for testing the WebSocket client.
 */

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "a_TestCertificates.hpp"

namespace Andromeda::Test {

    /**
     * @class MockWebsocketServer
     * @brief A real TLS WebSocket server on 127.0.0.1 with a free port, running on its own thread.
     * @details Echoes every text message back and records it, so a test can check both directions.
     *          It speaks the same protocol a production server does - TCP, TLS, the HTTP upgrade - so
     *          the client is tested end to end without the internet: fast, offline and deterministic.
     *          One client connection at a time is enough for the tests.
     */
    class MockWebsocketServer {
    public:
        struct Options {
            bool rejectUpgrade = false; ///< Answer the WebSocket upgrade with 403 Forbidden instead of accepting it.
        };

        explicit MockWebsocketServer(const TestCertificates& certificates, Options options = {});
        ~MockWebsocketServer();
        MockWebsocketServer(const MockWebsocketServer&) = delete;
        MockWebsocketServer& operator=(const MockWebsocketServer&) = delete;

        /** @brief The port the server listens on, chosen by the operating system. */
        [[nodiscard]] unsigned short port() const { return m_Port; }

        /** @brief Sends @p text to the connected client, after anything already queued. */
        void push(const std::string& text);

        /** @brief Closes the connection from the server side with a normal close frame. Call it once nothing is being sent. */
        void closeConnection();

        /** @brief Waits until @p count messages arrived; false on timeout. */
        [[nodiscard]] bool waitForMessages(std::size_t count, std::chrono::milliseconds timeout) const;

        /** @brief Waits until the client sent a close frame; false on timeout. */
        [[nodiscard]] bool waitForClientClose(std::chrono::milliseconds timeout) const;

        /** @brief Every text message received so far, in order. */
        [[nodiscard]] std::vector<std::string> messages() const;

        /** @brief Number of WebSocket upgrades the server accepted. */
        [[nodiscard]] int acceptedConnections() const;

    private:
        using Stream = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;
        class Coroutines;

        Options m_Options;
        boost::asio::io_context m_Io;
        boost::asio::ssl::context m_Tls{ boost::asio::ssl::context::tls_server };
        boost::asio::ip::tcp::acceptor m_Acceptor{ m_Io };
        unsigned short m_Port = 0;
        std::shared_ptr<Stream> m_Stream; ///< The open connection; only touched on the server thread.
        // Beast allows one write at a time per stream, so echoes and pushes queue up here and a single
        // writer coroutine sends them in order. Server thread only, like m_Stream.
        std::deque<std::string> m_Outbox;
        bool m_Writing = false;
        std::thread m_Thread;

        mutable std::mutex m_Mutex;
        mutable std::condition_variable m_Changed;
        std::vector<std::string> m_Messages;
        bool m_ClientClosed = false;
        int m_AcceptedConnections = 0;
    };
}
