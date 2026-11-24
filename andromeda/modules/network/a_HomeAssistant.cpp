#include "a_HomeAssistant.hpp"
#include "a_logger.hpp"
#include <nlohmann/json.hpp>
#include "a_filesystem.hpp"
#include "a_BoostWebsocketClient.hpp"
namespace Andromeda {

    void HomeAssistantService::init()
    {
		WebsocketReq req;
		req.host = "pi.andromeda-cloud.net";
		req.port = 443;
		req.path = "/api/websocket";
		m_WebsocketClient = std::make_shared<BoostWebsocketClient>(m_IoContext, m_SslContext, req, m_ProxySettings);
		m_WebsocketClient->onMessageReceived([this](const std::string& message) {
			std::lock_guard lock(m_QueueMutex);
			m_MessageQueue.push_back(message);
		});
		m_WebsocketClient->connect();
    }

	void HomeAssistantService::update()
	{
		std::vector<std::string> local; {
			std::lock_guard lock(m_QueueMutex);
			std::swap(local, m_MessageQueue);
		}

		for (const auto& msg : local) {
			handleMessage(msg);
		}
	}

	std::string HomeAssistantService::readHomeAssistantTokenFromSecretsJson()
	{
		std::string source = SOURCE_DIRECTORY;
		std::string rawTokenFile = Filesystem::readFile(source + "/secrets.json");
		nlohmann::json tokenJson = nlohmann::json::parse(rawTokenFile);

		std::string ha_token = "";

		if (tokenJson.contains("home_assistant") && tokenJson["home_assistant"].is_object()) {
			ha_token = tokenJson["home_assistant"].value("token", "");
		}

		if (ha_token.empty()) {
			A_ERROR("Home Assistant Token konnte nicht aus der secrets.json gelesen werden!");
			return "";
		}
		return ha_token;
	}
	void HomeAssistantService::handleAuthMessage(const std::string& messageType)
	{
		if (messageType == "auth_required") {
			std::string token = readHomeAssistantTokenFromSecretsJson();

			if (token.empty()) {
				A_ERROR("Cannot authenticate. Token is emtpy!.");
				return;
			}

			nlohmann::json authMessage;
			authMessage["type"] = "auth";
			authMessage["access_token"] = token;

			m_WebsocketClient->send(authMessage.dump());
		}
		else if (messageType == "auth_ok") {
			A_INFO("Successfully authenticated with Home Assistant WebSocket API.");
			m_Authenticated = true;

			nlohmann::json getStatesMessage;
			getStatesMessage["id"] = m_MessageID++;
			getStatesMessage["type"] = "get_states";
			m_WebsocketClient->send(getStatesMessage.dump());
		}
		else if (messageType == "auth_invalid") {
			A_ERROR("Authentication with Home Assistant WebSocket API failed. Invalid token.");
			m_Authenticated = false;
		}
	}
	void HomeAssistantService::handleMessage(const std::string& message)
	{
		//A_INFO("Received WebSocket message: {}", message);
		nlohmann::json jsonMessage;
		try {
			jsonMessage = nlohmann::json::parse(message);
		}
		catch (const nlohmann::json::parse_error& e) {
			A_ERROR("Failed to parse WebSocket message as JSON: {}", e.what());
			return;
		}

		if (jsonMessage.contains("type")) {
			std::string messageType = jsonMessage["type"];

			handleAuthMessage(messageType);

			if (!jsonMessage.contains("result") || !jsonMessage["result"].is_array() || messageType != "result") {
				return;
			}

			for (const auto& entity : jsonMessage["result"]) {
				if (entity.value("entity_id", "") == "sensor.bmv080_bmv080_pm1") {
					A_INFO("Sensor pm1 state: {}", entity.value("state", ""));
				}
				if (entity.value("entity_id", "") == "sensor.bmv080_bmv080_pm2_5") {
					A_INFO("Sensor pm2.5 state: {}", entity.value("state", ""));
				}
				if (entity.value("entity_id", "") == "sensor.bmv080_bmv080_pm10") {
					A_INFO("Sensor pm10 state: {}", entity.value("state", ""));
				}
			}
		}
	}
}