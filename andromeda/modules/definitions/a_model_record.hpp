#pragma once

/**
 * @file a_model_record.hpp
 * @brief Describes the static catalogue of built-in 3D models offered to the editor (e.g. in the asset browser).
 */

#include "a_primitives.hpp"
#include <string>
namespace Andromeda {
	struct Mesh;

	/**
	 * @enum deviceType
	 * @brief Classifies the kind of hardware device a model/entity represents.
	 */
	enum class deviceType {
		DEFAULT = -1,  ///< No specific device classification.
		SENSOR,        ///< A measurement device (e.g. the ESP32 particulate sensor).
		CONTROLLER,    ///< A microcontroller/board.
		CABLE,         ///< A connecting cable/wire.
		BREADBOARD,    ///< A prototyping breadboard.
	};

	/**
	 * @brief Static metadata describing a single built-in model entry.
	 */
	struct ModelRecord {
		u32 meshID;        ///< Identifier of the associated mesh resource.
		std::string name;  ///< Human-readable display name (e.g. shown in the asset browser).
		deviceType type;   ///< Device category this model represents.
	};

	/**
	 * @class IModelProvider
	 * @brief Interface for components that expose a fixed catalogue of built-in models to the editor.
	 */
	class IModelProvider {
	public:
		virtual ~IModelProvider() = default;

		/** @brief Returns the total number of available model records. */
		virtual u32 getModelCount() const = 0;

		/**
		 * @brief Returns the rendered preview texture for a model, or 0 if none was baked.
		 * @param meshID The model's mesh identifier.
		 */
		virtual u32 getPreviewTextureID(u32 meshID) const = 0;

		/**
		 * @brief Retrieves the metadata for a specific model.
		 * @param index Zero-based index into the model catalogue, must be less than @c getModelCount().
		 * @return Reference to the requested model's metadata.
		 */
		virtual const ModelRecord& getModelData(u32 index) const = 0;

		/**
		 * @brief Resolves the icon texture/resource ID used to represent a device type in the UI.
		 * @param type The device category to look up an icon for.
		 * @return Identifier of the icon resource.
		 */
		virtual u32 getDeviceIconID(deviceType type) const = 0;
	};
}