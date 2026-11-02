#include "a_MockHttpProxy.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <array>
#include <memory>

namespace Andromeda::Test {
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using tcp = net::ip::tcp;

    class MockHttpProxy::Coroutines {
    public:
        static net::awaitable<void> acceptLoop(MockHttpProxy& proxy) {
            try {
                for (;;) {
                    tcp::socket socket = co_await proxy.m_Acceptor.async_accept(net::use_awaitable);
                    net::co_spawn(proxy.m_Io, session(proxy, std::make_shared<tcp::socket>(std::move(socket))), net::detached);
                }
            } catch (...) {
                // The acceptor was closed: the proxy is shutting down.
            }
        }

        static net::awaitable<void> session(MockHttpProxy& proxy, std::shared_ptr<tcp::socket> client) {
            try {
                beast::flat_buffer buffer;
                http::request<http::empty_body> request;
                co_await http::async_read(*client, buffer, request, net::use_awaitable);
                const std::string target(request.target());
                {
                    std::lock_guard lock(proxy.m_Mutex);
                    proxy.m_ConnectTargets.push_back(target);
                }

                if (proxy.m_Mode == Mode::Refuse || request.method() != http::verb::connect) {
                    http::response<http::empty_body> refused{ http::status::forbidden, request.version() };
                    refused.prepare_payload();
                    co_await http::async_write(*client, refused, net::use_awaitable);
                    co_return;
                }

                const auto colon = target.rfind(':');
                tcp::resolver resolver(proxy.m_Io);
                const auto endpoints = co_await resolver.async_resolve(target.substr(0, colon), target.substr(colon + 1), net::use_awaitable);
                auto upstream = std::make_shared<tcp::socket>(proxy.m_Io);
                co_await net::async_connect(*upstream, endpoints, net::use_awaitable);

                http::response<http::empty_body> established{ http::status::ok, request.version() };
                established.reason("Connection established");
                co_await http::async_write(*client, established, net::use_awaitable);

                net::co_spawn(proxy.m_Io, relay(client, upstream), net::detached);
                net::co_spawn(proxy.m_Io, relay(upstream, client), net::detached);
            } catch (...) {
                // The client gave up or the target could not be reached.
            }
        }

        /** @brief Copies bytes from @p from to @p to until either side closes, then closes both. */
        static net::awaitable<void> relay(std::shared_ptr<tcp::socket> from, std::shared_ptr<tcp::socket> to) {
            std::array<char, 16 * 1024> chunk{};
            try {
                for (;;) {
                    const std::size_t read = co_await from->async_read_some(net::buffer(chunk), net::use_awaitable);
                    co_await net::async_write(*to, net::buffer(chunk.data(), read), net::use_awaitable);
                }
            } catch (...) {}
            beast::error_code ignored;
            from->close(ignored);
            to->close(ignored);
        }
    };

    MockHttpProxy::MockHttpProxy(const Mode mode)
        : m_Mode(mode) {
        const tcp::endpoint endpoint(net::ip::make_address("127.0.0.1"), 0);
        m_Acceptor.open(endpoint.protocol());
        m_Acceptor.bind(endpoint);
        m_Acceptor.listen();
        m_Port = m_Acceptor.local_endpoint().port();

        net::co_spawn(m_Io, Coroutines::acceptLoop(*this), net::detached);
        m_Thread = std::thread([this] { m_Io.run(); });
    }

    MockHttpProxy::~MockHttpProxy() {
        m_Io.stop();
        m_Thread.join();
    }

    std::vector<std::string> MockHttpProxy::connectTargets() const {
        std::lock_guard lock(m_Mutex);
        return m_ConnectTargets;
    }
}
