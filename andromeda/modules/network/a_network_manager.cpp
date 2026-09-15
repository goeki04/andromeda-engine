#include "a_network_manager.hpp"
#include "a_logger.hpp"
#include "a_BoostWebsocketClient.hpp"
#include <nlohmann/json.hpp>
#include <boost/asio/co_spawn.hpp>
#include <string>
#include "a_event_manager.hpp"
#include <boost/asio/detached.hpp>
#include "a_sensor_events.hpp"
namespace Andromeda {
    net::awaitable<void> readSensorStream(ThreadSafeQueue<std::string>& sensorLineQueue, std::string host, u16 port);
	void NetworkManager::start()
	{
		m_IoContext = std::make_shared<boost::asio::io_context>();
		m_ProxySettings = NetworkInfo::getProxySettings();
		m_SslContext.set_verify_mode(boost::asio::ssl::verify_peer);
		std::string certFile = std::string(SOURCE_DIRECTORY) + "/certs/cacert.pem";
		m_SslContext.load_verify_file(certFile);
		m_SslContext.set_default_verify_paths();

		m_HomeAssistantService = std::make_unique<HomeAssistantService>(m_IoContext, m_SslContext, m_ProxySettings);
        m_HomeAssistantService->init();

        boost::asio::co_spawn(*m_IoContext,readSensorStream(m_SensorEventQueue, "127.0.0.1", 8080),boost::asio::detached);
		m_WorkGuard.emplace(m_IoContext->get_executor());
		m_NetworkThread = std::jthread([ctx = m_IoContext]() {
			ctx->run();
			});
	}

	void NetworkManager::destroy()
	{
		m_WorkGuard.reset();
		m_IoContext->stop();
		if (m_NetworkThread.joinable()) {
			m_NetworkThread.join();
		}
	}

	void NetworkManager::update()
	{
		if (m_HomeAssistantService) {
			m_HomeAssistantService->update();
		}
        // Parsing happens here, on the main thread, so that a malformed line only costs one
        // warning instead of ending the network coroutine's read loop.
        auto lines = m_SensorEventQueue.dequeueAll();
        for (const auto& line : lines) {
            try {
                const SensorData sensorData = nlohmann::json::parse(line).get<SensorData>();
                const OnSensorMessageReceived event{ sensorData.data };
                EventManager::getInstance().Dispatch(EventType::OnSensorMessageReceived, event);
            } catch (const std::exception& e) {
                A_WARN("Sensor line could not be parsed: {}", e.what());
            }
        }
	}
}