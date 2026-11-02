// Reading the proxy from HTTPS_PROXY / HTTP_PROXY. Behind a corporate proxy a value the parser
// rejects means the client tries to connect directly - and fails without saying why.

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "a_network_info.hpp"

using Andromeda::NetworkInfo;

namespace {
    void expectProxy(const std::string& url, const std::string& host, int port) {
        SCOPED_TRACE(url);
        const auto proxy = NetworkInfo::parseProxyUrl(url);
        ASSERT_TRUE(proxy.has_value());
        EXPECT_EQ(proxy->host, host);
        EXPECT_EQ(proxy->port, port);
    }
}

TEST(ProxySettings, AcceptsTheUsualSpellings) {
    expectProxy("http://proxy.example.com:3128", "proxy.example.com", 3128);
    expectProxy("http://proxy.example.com:3128/", "proxy.example.com", 3128);
    expectProxy("proxy.example.com:8080", "proxy.example.com", 8080);
    expectProxy("http://127.0.0.1:3128", "127.0.0.1", 3128);
}

TEST(ProxySettings, DropsCredentials) {
    // The client cannot authenticate at a proxy, but ignoring the proxy entirely would hide the
    // reason the connection fails.
    expectProxy("http://user:secret@proxy.example.com:3128", "proxy.example.com", 3128);
}

TEST(ProxySettings, AcceptsBracketedIpv6Hosts) {
    expectProxy("http://[::1]:3128", "::1", 3128);
}

TEST(ProxySettings, RejectsUnusableValues) {
    const std::vector<std::string> unusable = {
        "",
        "http://proxy.example.com",        // no port
        "http://:3128",                    // no host
        "http://proxy.example.com:0",
        "http://proxy.example.com:65536",
        "http://proxy.example.com:-1",
        "http://proxy.example.com:3128abc", // std::stoi used to accept this as 3128
        "http://proxy.example.com:abc",
    };

    for (const std::string& url : unusable) {
        SCOPED_TRACE("'" + url + "'");
        EXPECT_FALSE(NetworkInfo::parseProxyUrl(url).has_value());
    }
}
