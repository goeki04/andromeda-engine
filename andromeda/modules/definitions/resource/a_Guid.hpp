#pragma once
#include <array>
#include <cstring>
#include <cstddef>
#include <functional>
#include "a_primitives.hpp"
#include <string>
#include <string_view>
#include <optional>
#include <charconv>
namespace Andromeda {

	struct Guid {
        std::array<u8, 16> bytes{};
        bool operator==(const Guid& other) const = default;

		[[nodiscard]] bool isValid() const { 
			return bytes != std::array<u8, 16>{};
        }

        /**
         * @brief Formats the Guid as 32 lowercase hex characters, without dashes.
         * @details Byte order is the storage order of @c bytes, so fromString(toString()) returns
         *          the same Guid.
         */
        [[nodiscard]] std::string toString() const {
            constexpr char digits[] = "0123456789abcdef";
            std::string out(32, '0');
            for (std::size_t i = 0; i < bytes.size(); ++i) {
                out[i * 2]     = digits[bytes[i] >> 4];
                out[i * 2 + 1] = digits[bytes[i] & 0x0F];
            }
            return out;
        }

        /**
         * @brief Parses the format written by toString(): exactly 32 hex characters.
         * @details Upper- and lowercase are both accepted. Anything else - wrong length, dashes,
         *          whitespace, a sign - is rejected instead of being partially read.
         * @return The Guid, or std::nullopt if @p text is not a valid Guid string.
         */
        [[nodiscard]] static std::optional<Guid> fromString(const std::string_view text) {
            if (text.size() != 32) {
                return std::nullopt;
            }
            Guid guid;
            for (std::size_t i = 0; i < guid.bytes.size(); ++i) {
                const char* first = text.data() + i * 2;
                const char* last = first + 2;
                const auto [ptr, ec] = std::from_chars(first, last, guid.bytes[i], 16);
                // ptr != last catches a pair where only the first character was a hex digit.
                if (ec != std::errc{} || ptr != last) {
                    return std::nullopt;
                }
            }
            return guid;
        }
    };
}

template<>
struct std::hash<Andromeda::Guid> {
    std::size_t operator()(const Andromeda::Guid& guid) const noexcept { 
		std::size_t value = 0;

		std::memcpy(&value, guid.bytes.data(), sizeof(value));
		return value;
	}
};