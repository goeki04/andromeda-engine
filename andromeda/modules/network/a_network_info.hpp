#pragma once

/**
 * @file a_network_info.hpp
 * @brief Helpers for reading the HTTP proxy configuration from the environment.
 */

#include "a_Primitives.hpp"
#include <charconv>
#include <string>
#include <string_view>
#include <cstdlib>
#include <optional>
#include "a_environment.hpp"
#include "a_logger.hpp"
namespace Andromeda {
	/**
	 * @struct ProxySettings
	 * @brief Host and port of an HTTP proxy server.
	 */
	struct ProxySettings {
		std::string host;	///< Hostname or IP address of the proxy.
		i32 port;			///< Port the proxy listens on.
	};

	/**
	 * @class NetworkInfo
	 * @brief Static helpers for querying the system's proxy configuration.
	 */
	class NetworkInfo {
	public:
		/**
		 * @brief Parses the HTTPS_PROXY/HTTP_PROXY environment variables.
		 * @return The proxy settings, or an empty optional if none are configured.
		 */
        static std::optional<ProxySettings> getProxySettings() {
            auto proxyEnv = getEnvironmentVariable("HTTPS_PROXY");
            if (!proxyEnv) {
                proxyEnv = getEnvironmentVariable("HTTP_PROXY");
            }
            if (!proxyEnv) {
                return {};
            }

            return parseProxyUrl(*proxyEnv);
        }

		/**
		 * @brief Parses a proxy URL as found in HTTPS_PROXY/HTTP_PROXY, e.g. "http://proxy.example:3128/".
		 * @details The scheme, any path and user credentials ("user:pass@") are dropped - the WebSocket
		 *          client cannot authenticate at a proxy, but ignoring the whole proxy would make it try to
		 *          connect directly, which fails behind a corporate firewall without telling why. An IPv6
		 *          host is written in brackets ("[::1]:3128") and returned without them.
		 * @return Host and port, or an empty optional if the host is missing or the port is not a whole
		 *         number from 1 to 65535.
		 */
        static std::optional<ProxySettings> parseProxyUrl(std::string_view url) {
            if (const auto schemeEnd = url.find("://"); schemeEnd != std::string_view::npos) {
                url.remove_prefix(schemeEnd + 3);
            }
            if (const auto pathStart = url.find('/'); pathStart != std::string_view::npos) {
                url = url.substr(0, pathStart);
            }
            if (const auto credentialsEnd = url.rfind('@'); credentialsEnd != std::string_view::npos) {
                url.remove_prefix(credentialsEnd + 1);
            }

            const auto colonPos = url.rfind(':');
            if (colonPos == std::string_view::npos) {
                return {};
            }
            std::string_view host = url.substr(0, colonPos);
            const std::string_view portText = url.substr(colonPos + 1);
            if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
                host = host.substr(1, host.size() - 2);
            }

            // from_chars instead of std::stoi: stoi would accept "3128abc" as 3128.
            i32 port = 0;
            const char* portEnd = portText.data() + portText.size();
            const auto [parsedEnd, ec] = std::from_chars(portText.data(), portEnd, port);
            if (host.empty() || portText.empty() || ec != std::errc{} || parsedEnd != portEnd || port <= 0 || port > 65535) {
                return {};
            }
            return ProxySettings{ std::string(host), port };
        }

		/**
		 * @brief Checks whether the given proxy settings are usable.
		 * @param proxySettings Settings previously returned by getProxySettings().
		 * @return True if a valid proxy is configured.
		 */
        static bool hasProxysettings(const std::optional<ProxySettings> &proxySettings) {
            if (proxySettings.has_value()) {
                if (proxySettings->host != "" && proxySettings->port > 0) {
                    A_INFO("Proxy settings found: {}:{}", proxySettings->host, proxySettings->port);
                    return true;
                }
            }
            A_INFO("No proxy found or invalid configuration. Connecting directly.");
            return false;
        }
	};
}
