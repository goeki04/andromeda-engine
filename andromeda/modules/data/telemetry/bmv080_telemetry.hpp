#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

namespace Andromeda {
    struct BMV080Telemetry {
        int pm1_0{0};
        int pm2_5{0};
        int pm10_0{0};
        bool is_obstructed{false};
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BMV080Telemetry, pm1_0, pm2_5, pm10_0, is_obstructed)
}