// BoostWebsocketClient against a local TLS WebSocket server and a local HTTP proxy: the whole
// connection path - TCP, proxy tunnel, TLS with certificate and host name check, WebSocket upgrade -
// without the internet.

#include <gtest/gtest.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "a_BoostWebsocketClient.hpp"
#include "a_LogCapture.hpp"
#include "a_MockHttpProxy.hpp"
#include "a_MockWebsocketServer.hpp"
#include "a_TestCertificates.hpp"

using namespace Andromeda;
using namespace Andromeda::Test;
using namespace std::chrono_literals;

namespace {
    struct StateChange {
        ConnectionState state;
        std::string reason;
    };

    /** @brief A port nothing listens on: bound once to get a free number, then released. */
    unsigned short closedPort() {
        boost::asio::io_context io;
        boost::asio::ip::tcp::acceptor acceptor(io, { boost::asio::ip::make_address("127.0.0.1"), 0 });
        return acceptor.local_endpoint().port();
    }
}

class WebsocketClientTest : public ::testing::Test {
protected:
    // Keeps the client's log lines out of the test output and lets tests check what it reported.
    LogCapture m_Log;
    TestCertificates m_Certificates;
    // Both the io_context and the client live in a shared_ptr: the client keeps the io_context
    // alive, and its coroutines keep the client alive through shared_from_this().
    std::shared_ptr<boost::asio::io_context> m_Io = std::make_shared<boost::asio::io_context>();
    boost::asio::ssl::context m_TrustingTls{ boost::asio::ssl::context::tls_client };
    std::vector<StateChange> m_States;
    std::vector<std::string> m_Received;

    void SetUp() override {
        m_Certificates.trustIn(m_TrustingTls);
    }

    std::shared_ptr<BoostWebsocketClient> makeClient(unsigned short port,
                                                     std::optional<ProxySettings> proxy = std::nullopt,
                                                     boost::asio::ssl::context* tls = nullptr) {
        WebsocketReq request;
        request.host = "127.0.0.1";
        request.port = port;
        request.path = "/api/websocket";
        auto client = std::make_shared<BoostWebsocketClient>(m_Io, tls ? *tls : m_TrustingTls, request, proxy);
        // The callbacks run on the thread that runs m_Io - the test thread, inside runUntil().
        client->onConnectionStateChanged([this](ConnectionState state, const std::string& reason) {
            m_States.push_back({ state, reason });
        });
        client->onMessageReceived([this](const std::string& message) { m_Received.push_back(message); });
        return client;
    }

    /** @brief Runs the client's io_context until @p done holds or @p timeout passes. */
    bool runUntil(const std::function<bool()>& done, std::chrono::milliseconds timeout = 5s) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!done()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            m_Io->run_for(10ms);
            if (m_Io->stopped()) {
                m_Io->restart();
            }
        }
        return true;
    }

    bool runUntilStates(std::size_t count) {
        return runUntil([&] { return m_States.size() >= count; });
    }

    bool connect(BoostWebsocketClient& client) {
        client.connect();
        return runUntilStates(1) && m_States.front().state == ConnectionState::Connected;
    }
};

TEST_F(WebsocketClientTest, ConnectsOverTlsAndReportsConnected) {
    MockWebsocketServer server(m_Certificates);
    auto client = makeClient(server.port());

    client->connect();
    ASSERT_TRUE(runUntilStates(1));

    EXPECT_EQ(m_States.front().state, ConnectionState::Connected) << m_States.front().reason;
    EXPECT_TRUE(client->isConnected());
    EXPECT_EQ(server.acceptedConnections(), 1);
}

TEST_F(WebsocketClientTest, SentMessageReachesTheServerAndItsReplyTheCallback) {
    MockWebsocketServer server(m_Certificates);
    auto client = makeClient(server.port());
    ASSERT_TRUE(connect(*client));

    client->send(R"({"type":"ping","id":1})");

    ASSERT_TRUE(runUntil([&] { return !m_Received.empty(); }));
    EXPECT_EQ(server.messages(), std::vector<std::string>{ R"({"type":"ping","id":1})" });
    EXPECT_EQ(m_Received, std::vector<std::string>{ R"({"type":"ping","id":1})" });
}

TEST_F(WebsocketClientTest, MessagesPushedByTheServerArriveInOrder) {
    // Home Assistant sends state changes unasked; they must arrive complete and in order.
    MockWebsocketServer server(m_Certificates);
    auto client = makeClient(server.port());
    ASSERT_TRUE(connect(*client));

    server.push("first");
    server.push(std::string(100 * 1024, 'x')); // larger than one TLS record
    server.push("third");

    ASSERT_TRUE(runUntil([&] { return m_Received.size() == 3; }));
    EXPECT_EQ(m_Received[0], "first");
    EXPECT_EQ(m_Received[1].size(), 100u * 1024);
    EXPECT_EQ(m_Received[2], "third");
}

TEST_F(WebsocketClientTest, DisconnectClosesCleanlyAndIsReportedOnce) {
    MockWebsocketServer server(m_Certificates);
    auto client = makeClient(server.port());
    ASSERT_TRUE(connect(*client));

    client->disconnect();

    ASSERT_TRUE(runUntilStates(2));
    EXPECT_TRUE(server.waitForClientClose(2s)) << "the server should see a proper close frame";
    runUntil([] { return false; }, 200ms); // give a second, wrong notification the chance to show up
    ASSERT_EQ(m_States.size(), 2u);
    EXPECT_EQ(m_States[1].state, ConnectionState::Disconnected);
    EXPECT_FALSE(client->isConnected());
}

TEST_F(WebsocketClientTest, ServerClosingTheConnectionIsReportedAsDisconnected) {
    MockWebsocketServer server(m_Certificates);
    auto client = makeClient(server.port());
    ASSERT_TRUE(connect(*client));

    server.closeConnection();

    ASSERT_TRUE(runUntilStates(2));
    EXPECT_EQ(m_States[1].state, ConnectionState::Disconnected);
    EXPECT_FALSE(client->isConnected());
}

TEST_F(WebsocketClientTest, SendingWhileDisconnectedIsRefused) {
    MockWebsocketServer server(m_Certificates);
    auto client = makeClient(server.port());

    client->send("lost");
    runUntil([] { return false; }, 100ms);

    EXPECT_TRUE(server.messages().empty());
    EXPECT_TRUE(m_Log.contains("not Connected")) << m_Log.text();
}

TEST_F(WebsocketClientTest, ServerWithAnUntrustedCertificateIsRejected) {
    MockWebsocketServer server(m_Certificates);
    boost::asio::ssl::context noTrust{ boost::asio::ssl::context::tls_client };
    noTrust.set_verify_mode(boost::asio::ssl::verify_peer);
    auto client = makeClient(server.port(), std::nullopt, &noTrust);

    client->connect();
    ASSERT_TRUE(runUntilStates(1));

    EXPECT_EQ(m_States.front().state, ConnectionState::Failed);
    EXPECT_FALSE(client->isConnected());
    EXPECT_EQ(server.acceptedConnections(), 0);
}

TEST_F(WebsocketClientTest, CertificateIssuedForAnotherHostIsRejected) {
    // Trusted CA, valid certificate - but for a different server. Without the host name check a
    // man in the middle with any certificate from a trusted CA could read the traffic.
    TestCertificates otherHost("DNS:other.invalid");
    boost::asio::ssl::context trustsOtherHostsCa{ boost::asio::ssl::context::tls_client };
    otherHost.trustIn(trustsOtherHostsCa);
    MockWebsocketServer server(otherHost);
    auto client = makeClient(server.port(), std::nullopt, &trustsOtherHostsCa);

    client->connect();
    ASSERT_TRUE(runUntilStates(1));

    EXPECT_EQ(m_States.front().state, ConnectionState::Failed);
    EXPECT_EQ(server.acceptedConnections(), 0);
}

TEST_F(WebsocketClientTest, RefusedConnectionFailsWithAReason) {
    auto client = makeClient(closedPort());

    client->connect();
    ASSERT_TRUE(runUntilStates(1));

    EXPECT_EQ(m_States.front().state, ConnectionState::Failed);
    EXPECT_FALSE(m_States.front().reason.empty());
}

TEST_F(WebsocketClientTest, RejectedUpgradeIsReportedAsFailed) {
    // The server is reachable and TLS works, but it refuses the WebSocket upgrade (e.g. a wrong path).
    MockWebsocketServer server(m_Certificates, { .rejectUpgrade = true });
    auto client = makeClient(server.port());

    client->connect();
    ASSERT_TRUE(runUntilStates(1));

    EXPECT_EQ(m_States.front().state, ConnectionState::Failed);
    EXPECT_FALSE(client->isConnected());
}

TEST_F(WebsocketClientTest, ConnectsThroughAnHttpProxyTunnel) {
    MockWebsocketServer server(m_Certificates);
    MockHttpProxy proxy;
    auto client = makeClient(server.port(), ProxySettings{ "127.0.0.1", proxy.port() });
    ASSERT_TRUE(connect(*client)) << (m_States.empty() ? "" : m_States.front().reason);

    client->send("through the tunnel");

    ASSERT_TRUE(runUntil([&] { return !m_Received.empty(); }));
    EXPECT_EQ(m_Received.front(), "through the tunnel");
    EXPECT_EQ(proxy.connectTargets(), std::vector<std::string>{ "127.0.0.1:" + std::to_string(server.port()) });
}

TEST_F(WebsocketClientTest, ProxyRefusingTheTunnelIsReportedAsFailed) {
    MockWebsocketServer server(m_Certificates);
    MockHttpProxy proxy(MockHttpProxy::Mode::Refuse);
    auto client = makeClient(server.port(), ProxySettings{ "127.0.0.1", proxy.port() });

    client->connect();
    ASSERT_TRUE(runUntilStates(1));

    EXPECT_EQ(m_States.front().state, ConnectionState::Failed);
    EXPECT_NE(m_States.front().reason.find("Proxy tunnel failed"), std::string::npos) << m_States.front().reason;
    EXPECT_EQ(server.acceptedConnections(), 0);
}
