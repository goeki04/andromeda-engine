#pragma once

/**
 * @file a_WeatherService.hpp
 * @brief Service fetching current weather data from the Open-Meteo API.
 */

#include <httplib.h>
#include "a_primitives.hpp"
#include <optional>
namespace Andromeda {
	/**
	 * @struct GeoCoordinates
	 * @brief Geographic position in decimal degrees.
	 */
	struct GeoCoordinates {
		float latitude;		///< Latitude in decimal degrees.
		float longitude;	///< Longitude in decimal degrees.
	};

	/**
	 * @struct WeatherData
	 * @brief Current weather values returned by the forecast API.
	 */
	struct WeatherData {
		float windSpeed;	///< Wind speed in km/h.
		i32 windDirection;	///< Wind direction in degrees.
	};

	/**
	 * @class WeatherService
	 * @brief Fetches live weather data via the Open-Meteo HTTP API.
	 *
	 * Performs blocking HTTPS requests (geocoding and forecast). Proxy settings
	 * from the environment are applied automatically.
	 */
	class WeatherService {
	public:
		WeatherService() {};

		/**
		 * @brief Fetches the current weather for the configured location.
		 * @return The current weather data, or default values on failure.
		 */
		WeatherData getLiveWeatherData();

		/**
		 * @brief Resolves a location name to geographic coordinates.
		 * @param location Name of the town or city to look up.
		 * @return The coordinates, or an empty optional if the lookup failed.
		 */
		std::optional<GeoCoordinates> fetchGeoCoordinates(const std::string& location);

		/**
		 * @brief Fetches the current weather for the given coordinates.
		 * @param coords Geographic position to query.
		 * @return The weather data, or an empty optional if the request failed.
		 */
		std::optional<WeatherData> fetchWeatherData(const GeoCoordinates& coords);
	};
}
