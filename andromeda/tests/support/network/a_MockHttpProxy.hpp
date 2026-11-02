#pragma once

/**
 * @file a_MockHttpProxy.hpp
 * @brief An in-process HTTP CONNECT proxy for testing clients behind a proxy.
 */

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Andromeda::Test {

    /**
     * @class MockHttpProxy
     * @brief An HTTP proxy on 127.0.0.1 that answers CONNECT requests, running on its own thread.
     * @details Behind a corporate proxy the WebSocket client first asks the proxy to open a tunnel
     *          ("CONNECT host:port") and then runs TLS through it. This proxy either opens the tunnel
     *          and relays the bytes both ways, or refuses it like a proxy that blocks the target.
     */
    class MockHttpProxy {
    public:
        enum class Mode {
            Tunnel, ///< Answer "200" and relay the connection to the requested target.
            Refuse, ///< Answer "403 Forbidden" and close.
        };

        explicit MockHttpProxy(Mode mode = Mode::Tunnel);
        ~MockHttpProxy();
        MockHttpProxy(const MockHttpProxy&) = delete;
        MockHttpProxy& operator=(const MockHttpProxy&) = delete;

        [[nodiscard]] unsigned short port() const { return m_Port; }

        /** @brief The target of every CONNECT request so far, e.g. "127.0.0.1:50123". */
        [[nodiscard]] std::vector<std::string> connectTargets() const;

    private:
        class Coroutines;

        Mode m_Mode;
        boost::asio::io_context m_Io;
        boost::asio::ip::tcp::acceptor m_Acceptor{ m_Io };
        unsigned short m_Port = 0;
        std::thread m_Thread;

        mutable std::mutex m_Mutex;
        std::vector<std::string> m_ConnectTargets;
    };
}
