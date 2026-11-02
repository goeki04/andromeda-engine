#pragma once

/**
 * @file a_undo_buffer.hpp
 * @brief Data-oriented undo history that stores commands as a flat byte stream.
 */

#include <type_traits>
#include <vector>
#include "a_primitives.hpp"
#include "generated_undo_commands.hpp"

namespace Andromeda::ECS {
	class ComponentRegistry;
}
namespace Andromeda::Editor::Undo {

	/**
	 * @class UndoBuffer
	 * @brief A fixed-depth undo stack that serializes commands into a contiguous byte buffer.
	 *
	 * @details Rather than storing polymorphic command objects, each undo step is appended as a
	 *          @c CommandHeader (type + payload size) immediately followed by a POD payload
	 *          (e.g. @c UndoTransformData). @c commandOffsets records where each command begins,
	 *          allowing the most recent command to be located and replayed. This keeps undo data
	 *          cache-friendly and free of per-command heap allocations. The history is capped at
	 *          @c maxUndoSteps; pushing beyond the cap evicts the oldest command.
	 *
	 * @see CommandDispatcher, generated_undo_commands.hpp
	 */
	class UndoBuffer {
	private:
		std::vector<u8> data;              ///< Flat byte stream holding all queued commands (header + payload, back to back).
		std::vector<size_t> commandOffsets; ///< Byte offset into @c data for the start of each command.
	public:
		static constexpr size_t maxUndoSteps = 50; ///< Maximum number of commands retained before the oldest is evicted.

		/**
		 * @brief Appends raw bytes to the end of the command buffer.
		 * @param src Pointer to the source bytes to copy.
		 * @param size Number of bytes to copy.
		 */
		void appendBytes(const void* src, size_t size);

		/**
		 * @brief Removes the oldest command from the buffer to enforce the @c maxUndoSteps cap.
		 */
		void deleteCommand();

		/**
		 * @brief Pops the most recent command and applies its inverse to the registry.
		 * @param registry The component registry the undo operation is applied to.
		 */
		void undo(ECS::ComponentRegistry& registry);

		/** @brief Number of commands that can currently be undone, at most @c maxUndoSteps. */
		[[nodiscard]] size_t size() const { return commandOffsets.size(); }

		/**
		 * @brief Serializes a command (header + payload) onto the undo stack.
		 * @details Evicts the oldest command first if the buffer is full, records the new
		 *          command's offset, then writes a @c CommandHeader followed by @p commandData.
		 * @tparam T The POD payload type describing the change (e.g. @c UndoTransformData).
		 * @param cmdType The command type tag stored in the header, used during dispatch.
		 * @param commandData The payload capturing the data needed to reverse the action.
		 */
		template<typename T>
		void pushCommand(CommandType cmdType, const T& commandData) {
			// The payload is copied byte by byte and later read back through a reinterpret_cast.
			// That is only defined for trivially copyable types: a payload holding a std::string
			// (e.g. UndoTagData, through Tag::name) would copy the string's internal pointer, and the
			// undo would read freed memory once the original string is gone.
			static_assert(std::is_trivially_copyable_v<T>,
				"Undo payloads are stored as raw bytes and must be trivially copyable - "
				"replace std::string and other owning members with fixed-size data.");
			if (commandOffsets.size() >= maxUndoSteps) {
				deleteCommand();
			}

			commandOffsets.push_back(data.size());

			CommandHeader header{ cmdType, (u32)sizeof(T) };
			appendBytes(&header, sizeof(CommandHeader));

			appendBytes(&commandData, sizeof(T));
		}
	};
}