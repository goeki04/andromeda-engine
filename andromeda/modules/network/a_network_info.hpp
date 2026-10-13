#pragma once

/**
 * @file a_network_info.hpp
 * @brief Helpers for reading the HTTP proxy configuration from the environment.
 */

#include "a_Primitives.hpp"
#include <string>
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

            std::string proxyUrl = *proxyEnv;

            auto schemeEnd = proxyUrl.find("://");
            std::string hostPort = (schemeEnd != std::string::npos) ? proxyUrl.substr(schemeEnd + 3) : proxyUrl;

            if (!hostPort.empty() && hostPort.back() == '/') {
                hostPort.pop_back();
            }

            auto colonPos = hostPort.find(':');
            if (colonPos == std::string::npos) {
                return {};
            }

            std::string host = hostPort.substr(0, colonPos);
            std::string portStr = hostPort.substr(colonPos + 1);

            if (host.empty() || portStr.empty()) {
                return {};
            }

            try {
                i32 port = std::stoi(portStr);
                if (port <= 0 || port > 65535) {
                    return {};
                }
                return ProxySettings{ host, port };
            }
            catch (const std::exception&) {
                return {};
            }
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
