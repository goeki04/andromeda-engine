
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <boost/asio/use_awaitable.hpp>
#include "a_event_manager.hpp"
#include "a_EventTypes.hpp"
#include <boost/asio/awaitable.hpp>
#include <a_logger.hpp>
#include "a_primitives.hpp"
#include "a_threadSafeQueue.hpp"
namespace net = boost::asio;
using boost::asio::ip::tcp;
/// <summary>
/// <TODO: Make it to a service
/// </summary>
namespace Andromeda {
    void readSensorData(boost::asio::io_context& io_ctx, const std::string& ip, unsigned short port) {
        tcp::socket socket(io_ctx);
        tcp::resolver resolver(io_ctx);

        boost::asio::connect(socket, resolver.resolve(ip, std::to_string(port)));
        std::cout << "Direkt mit Sensor verbunden!\n";

        boost::asio::streambuf buffer;

        while (true) {
            boost::system::error_code ec;
            boost::asio::read_until(socket, buffer, '\n', ec);

            if (ec) {
                std::cerr << "Sensor-Verbindung getrennt: " << ec.message() << "\n";
                break;
            }

            std::istream is(&buffer);
            std::string line;
            std::getline(is, line);

            std::cout << "Sensor Data: " << line << "\n";
        }
    }

    net::awaitable<void> readSensorStream(ThreadSafeQueue<OnSensorMessageReceived>& sensorEventQueue, std::string host, u16 port) {
        auto executor = co_await net::this_coro::executor;
        tcp::resolver resolver(executor);
        tcp::socket socket(executor);

        try {
            auto endpoints = co_await resolver.async_resolve(host, std::to_string(port), net::use_awaitable);
            co_await net::async_connect(socket, endpoints, net::use_awaitable);
            A_INFO("Mit Sensor verbunden!");
            std::string handshake = "hello_sensor\n";
            co_await net::async_write(socket, net::buffer(handshake), net::use_awaitable);

            net::streambuf buffer;
            while (socket.is_open()) {
                co_await net::async_read_until(socket, buffer, '\n', net::use_awaitable);

                std::istream is(&buffer);
                std::string line;
                std::getline(is, line); // std::getline will remove the newline character and stop at the end of the line
                OnSensorMessageReceived event{ line };
                sensorEventQueue.push(std::move(event));
            }
        } catch (const std::exception& e) {
            A_ERROR("Server not online");
        }
    }
}