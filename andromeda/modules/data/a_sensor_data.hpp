#pragma once
#include <variant>
#include "a_primitives.hpp"
#include "telemetry/bmv080_telemetry.hpp"
#include <string>
using SensorTelemetry = std::variant<Andromeda::BMV080Telemetry>;

namespace Andromeda {
    inline void to_json(nlohmann::json& j, const SensorTelemetry& telemetry) {
        std::visit([&j](const auto& arg) { j = arg; }, telemetry);
    }

    inline void from_json(const nlohmann::json& j, SensorTelemetry& telemetry) {
        try {
            telemetry = j.get<Andromeda::BMV080Telemetry>();
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error(std::string("Error while parsing SensorTelemetry: ") + e.what());
        }
    }
    struct SensorData {
        std::string sensor_name;
        u64 timestamp;
        std::string source; // direct_tcp, home assistant, etc.
        SensorTelemetry data;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SensorData, sensor_name, timestamp, source, data)
    };
}