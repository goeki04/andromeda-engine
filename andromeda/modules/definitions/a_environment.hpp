#pragma once

/**
 * @file a_environment.hpp
 * @brief Reading environment variables without the MSVC deprecation warning.
 */

#include <cstdlib>
#include <optional>
#include <string>

namespace Andromeda {

    /**
     * @brief Reads an environment variable.
     *
     * @details An unset variable and an empty one both come back as an empty optional -
     *          every caller here treated them the same anyway, and on Windows an empty
     *          value is how a variable is deleted.
     *
     *          MSVC deprecates std::getenv because the pointer it returns is invalidated
     *          by the next putenv/setenv in any thread. _dupenv_s hands out a copy
     *          instead, which this function turns into a std::string and frees right away,
     *          so the caller cannot be caught by that.
     *
     * @param name Name of the variable, e.g. "APPDATA".
     * @return Its value, or an empty optional when it is unset or empty.
     */
    inline std::optional<std::string> getEnvironmentVariable(const char* name) {
#ifdef _MSC_VER
        char* value = nullptr;
        size_t length = 0;

        // Returns 0 for "no error", which includes "variable does not exist" - in that
        // case value stays null, so the pointer is what actually has to be checked.
        if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
            return std::nullopt;
        }

        std::string result(value);
        std::free(value);

        if (result.empty()) {
            return std::nullopt;
        }
        return result;
#else
        const char* value = std::getenv(name);
        if (value == nullptr || value[0] == '\0') {
            return std::nullopt;
        }
        return std::string(value);
#endif
    }
}
