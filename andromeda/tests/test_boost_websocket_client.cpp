#include <gtest/gtest.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ostream_sink.h>
#include <chrono>
#include <memory>
#include <sstream>
#include "a_BoostWebsocketClient.hpp"
#include "a_logger.hpp"

using namespace Andromeda;

namespace {

// BoostWebsocketClient only reports connect() success/failure via A_INFO/A_ERROR
// log lines (no public getter, no callback). To assert on the outcome without
// changing the class under test, we temporarily swap spdlog's default logger
// for one that writes into an in-memory buffer, then restore it afterwards.
class LogCapture {
public:
    LogCapture() {
        m_Sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(m_Buffer);
        m_Logger = std::make_shared<spdlog::logger>("test", m_Sink);
        m_Logger->set_level(spdlog::level::trace);
        m_Previous = spdlog::default_logger();
        spdlog::set_default_logger(m_Logger);
    }

    ~LogCapture() {
        spdlog::set_default_logger(m_Previous);
    }

    std::string text() const { return m_Buffer.str(); }

private:
    std::ostringstream m_Buffer;
    std::shared_ptr<spdlog::sinks::ostream_sink_mt> m_Sink;
    std::shared_ptr<spdlog::logger> m_Logger;
    std::shared_ptr<spdlog::logger> m_Previous;
};

} // namespace

class BoostWebsocketClientTest : public ::testing::Test {
protected:
    // Both the io_context and the client itself must live in a shared_ptr: the client
    // takes a shared io_context, and connect() spawns coroutines that keep the object
    // alive via shared_from_this(). A stack instance compiles in some setups but throws
    // std::bad_weak_ptr the moment connect() is called.
    std::shared_ptr<boost::asio::io_context> m_IoContext = std::make_shared<boost::asio::io_context>();
    boost::asio::ssl::context m_SslContext{ boost::asio::ssl::context::tlsv12_client };

    void SetUp() override {
        m_SslContext.set_default_verify_paths();
    }
};

// NOTE: These first two tests need real internet/DNS access (they talk to a
// public echo server). They are integration tests riding along in the unit
// test binary, not true isolated unit tests - skip/ignore them if this runs
// in a sandboxed CI without network access.

TEST_F(BoostWebsocketClientTest, ConnectSucceedsAgainstPublicEchoServer) {
    WebsocketReq req;
    req.host = "ws.postman-echo.com";
    req.port = 443;
    req.path = "/raw";

    LogCapture capture;
    auto client = std::make_shared<BoostWebsocketClient>(m_IoContext, m_SslContext, req, std::nullopt);

    ASSERT_NO_THROW(client->connect());
    m_IoContext->run_for(std::chrono::seconds(10));

    EXPECT_NE(capture.text().find("Successfully connected"), std::string::npos)
        << "Expected a successful connection log line. Full log:\n" << capture.text();
}

TEST_F(BoostWebsocketClientTest, ConnectFailsGracefullyForUnresolvableHost) {
    WebsocketReq req;
    req.host = "this-host-does-not-exist.invalid";
    req.port = 443;
    req.path = "/";

    LogCapture capture;
    auto client = std::make_shared<BoostWebsocketClient>(m_IoContext, m_SslContext, req, std::nullopt);

    ASSERT_NO_THROW(client->connect());
    m_IoContext->run_for(std::chrono::seconds(10));

    EXPECT_NE(capture.text().find("Connection failed"), std::string::npos)
        << "DNS resolution should fail fast and be caught, not crash or hang. Full log:\n" << capture.text();
}

// Uses a non-routable address (TEST-NET reserved range) so no DNS lookup is
// needed - this only exercises the connect-timeout path via expires_after(),
// and should still complete well before the 35s bound regardless of network
// access being available in the sandbox.
TEST_F(BoostWebsocketClientTest, ConnectDoesNotHangOnUnreachableHost) {
    WebsocketReq req;
    req.host = "10.255.255.1";
    req.port = 443;
    req.path = "/";

    LogCapture capture;
    auto client = std::make_shared<BoostWebsocketClient>(m_IoContext, m_SslContext, req, std::nullopt);

    ASSERT_NO_THROW(client->connect());

    const auto start = std::chrono::steady_clock::now();
    m_IoContext->run_for(std::chrono::seconds(35)); // internal timeout is 30s
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::seconds(35))
        << "connect() did not return within the expected timeout window - "
           "the expires_after() watchdog may not be firing.";
    EXPECT_NE(capture.text().find("Connection failed"), std::string::npos)
        << "Full log:\n" << capture.text();
}
