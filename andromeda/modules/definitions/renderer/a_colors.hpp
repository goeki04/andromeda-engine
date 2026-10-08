#pragma once

/**
 * @file a_colors.hpp
 * @brief The engine's shared color palette: named RGBA constants, a stable enum to name them by,
 *        and the conversions every consumer would otherwise re-type.
 *
 * @details Colors travel through the engine as @c vec4 in (r, g, b, a) order with components in
 *          [0, 1] - the form the UBOs and shaders expect - so this header deliberately adds no
 *          wrapper type. Editor-specific semantics (accent, selection, panel backgrounds) are not
 *          here; they live in @c Gui::a_Style.hpp, which owns the editor's look.
 */

#include "a_primitives.hpp"
#include <string_view>
#include <cmath>

namespace Andromeda {

    /**
     * @namespace Andromeda::Colors
     * @brief Named RGBA constants of the built-in palette.
     *
     * @note These are display-space (sRGB) literals, which is what you want for clear colors,
     *       debug draws, gizmos and particle tints. Feeding one to the PBR shader as an albedo
     *       means converting it first - see @c srgbToLinear().
     */
    namespace Colors {
        inline constexpr vec4 Transparent { 0.0f,  0.0f,  0.0f,  0.0f };

        inline constexpr vec4 Black       { 0.0f,  0.0f,  0.0f,  1.0f };
        inline constexpr vec4 White       { 1.0f,  1.0f,  1.0f,  1.0f };
        inline constexpr vec4 DarkGrey    { 0.25f, 0.25f, 0.25f, 1.0f };
        inline constexpr vec4 Grey        { 0.5f,  0.5f,  0.5f,  1.0f };
        inline constexpr vec4 LightGrey   { 0.75f, 0.75f, 0.75f, 1.0f };

        inline constexpr vec4 Red         { 1.0f,  0.0f,  0.0f,  1.0f };
        inline constexpr vec4 Green       { 0.0f,  1.0f,  0.0f,  1.0f };
        inline constexpr vec4 Blue        { 0.0f,  0.0f,  1.0f,  1.0f };
        inline constexpr vec4 Yellow      { 1.0f,  1.0f,  0.0f,  1.0f };
        inline constexpr vec4 Cyan        { 0.0f,  1.0f,  1.0f,  1.0f };
        inline constexpr vec4 Magenta     { 1.0f,  0.0f,  1.0f,  1.0f };

        inline constexpr vec4 Orange      { 1.0f,  0.5f,  0.0f,  1.0f };
        inline constexpr vec4 Purple      { 0.5f,  0.0f,  0.5f,  1.0f };
        inline constexpr vec4 Pink        { 1.0f,  0.4f,  0.7f,  1.0f };
        inline constexpr vec4 Brown       { 0.4f,  0.26f, 0.13f, 1.0f };
        inline constexpr vec4 Teal        { 0.0f,  0.5f,  0.5f,  1.0f };
        inline constexpr vec4 Lime        { 0.5f,  1.0f,  0.0f,  1.0f };
        inline constexpr vec4 Navy        { 0.0f,  0.0f,  0.5f,  1.0f };
        inline constexpr vec4 Olive       { 0.5f,  0.5f,  0.0f,  1.0f };
        inline constexpr vec4 Maroon      { 0.5f,  0.0f,  0.0f,  1.0f };
    }

    /**
     * @enum NamedColor
     * @brief Identifies one entry of @c Colors by a stable value.
     *
     * @details Lets a color travel as a single byte instead of four floats: serialized scenes,
     *          editor dropdowns and particle node defaults can store the name and resolve it
     *          through @c toColor(), so a tweaked palette value updates everywhere at once.
     *          Append new entries before @c Count only - the numeric values are persisted.
     */
    enum class NamedColor : u8 {
        Transparent = 0,
        Black, White, DarkGrey, Grey, LightGrey,
        Red, Green, Blue, Yellow, Cyan, Magenta,
        Orange, Purple, Pink, Brown, Teal, Lime, Navy, Olive, Maroon,
        Count ///< Number of palette entries; not a color itself.
    };

    /** @brief Resolves a palette entry to its RGBA value. Returns @c Colors::Magenta for an invalid enum, so a broken value is visible rather than silent. */
    constexpr vec4 toColor(NamedColor color) {
        switch (color) {
            case NamedColor::Transparent: return Colors::Transparent;
            case NamedColor::Black:       return Colors::Black;
            case NamedColor::White:       return Colors::White;
            case NamedColor::DarkGrey:    return Colors::DarkGrey;
            case NamedColor::Grey:        return Colors::Grey;
            case NamedColor::LightGrey:   return Colors::LightGrey;
            case NamedColor::Red:         return Colors::Red;
            case NamedColor::Green:       return Colors::Green;
            case NamedColor::Blue:        return Colors::Blue;
            case NamedColor::Yellow:      return Colors::Yellow;
            case NamedColor::Cyan:        return Colors::Cyan;
            case NamedColor::Magenta:     return Colors::Magenta;
            case NamedColor::Orange:      return Colors::Orange;
            case NamedColor::Purple:      return Colors::Purple;
            case NamedColor::Pink:        return Colors::Pink;
            case NamedColor::Brown:       return Colors::Brown;
            case NamedColor::Teal:        return Colors::Teal;
            case NamedColor::Lime:        return Colors::Lime;
            case NamedColor::Navy:        return Colors::Navy;
            case NamedColor::Olive:       return Colors::Olive;
            case NamedColor::Maroon:      return Colors::Maroon;
            default:                      return Colors::Magenta;
        }
    }

    /** @brief Returns the display name of a palette entry, for editor dropdowns and serialization. */
    constexpr std::string_view toString(NamedColor color) {
        switch (color) {
            case NamedColor::Transparent: return "Transparent";
            case NamedColor::Black:       return "Black";
            case NamedColor::White:       return "White";
            case NamedColor::DarkGrey:    return "Dark Grey";
            case NamedColor::Grey:        return "Grey";
            case NamedColor::LightGrey:   return "Light Grey";
            case NamedColor::Red:         return "Red";
            case NamedColor::Green:       return "Green";
            case NamedColor::Blue:        return "Blue";
            case NamedColor::Yellow:      return "Yellow";
            case NamedColor::Cyan:        return "Cyan";
            case NamedColor::Magenta:     return "Magenta";
            case NamedColor::Orange:      return "Orange";
            case NamedColor::Purple:      return "Purple";
            case NamedColor::Pink:        return "Pink";
            case NamedColor::Brown:       return "Brown";
            case NamedColor::Teal:        return "Teal";
            case NamedColor::Lime:        return "Lime";
            case NamedColor::Navy:        return "Navy";
            case NamedColor::Olive:       return "Olive";
            case NamedColor::Maroon:      return "Maroon";
            default:                      return "Unknown";
        }
    }

    /** @brief Builds a color from 8-bit channel values, the form color pickers and hex codes come in. */
    constexpr vec4 fromRGB8(u8 r, u8 g, u8 b, u8 a = 255) {
        return vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    /** @brief Builds a color from a packed 0xRRGGBB literal, with alpha given separately. */
    constexpr vec4 fromHex(u32 rgb, float alpha = 1.0f) {
        return vec4(((rgb >> 16) & 0xFFu) / 255.0f,
                    ((rgb >> 8)  & 0xFFu) / 255.0f,
                    ( rgb        & 0xFFu) / 255.0f,
                    alpha);
    }

    /** @brief Returns @p color with its alpha replaced by @p alpha, leaving the RGB untouched. */
    constexpr vec4 withAlpha(const vec4& color, float alpha) {
        return vec4(color.x, color.y, color.z, alpha);
    }

    /**
     * @brief Packs a color into the 0xAABBGGRR word ImGui's draw list API expects (@c ImU32).
     * @details Kept here rather than in the gui module so panels that hand-draw geometry can reuse
     *          the engine palette without converting by hand.
     */
    constexpr u32 toPackedABGR(const vec4& color) {
        const auto channel = [](float v) -> u32 {
            const float clamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            return static_cast<u32>(clamped * 255.0f + 0.5f);
        };
        return channel(color.x) | (channel(color.y) << 8) | (channel(color.z) << 16) | (channel(color.w) << 24);
    }

    /**
     * @brief Converts a display-space (sRGB) color into linear space for lighting math.
     * @details Required before using a palette entry as a PBR albedo: the shader works in linear
     *          space, so an unconverted literal renders noticeably too bright. Uses the cheap
     *          gamma 2.2 approximation rather than the exact piecewise sRGB curve, which is what
     *          the rest of the pipeline assumes. Alpha is never gamma-encoded and passes through.
     */
    inline vec4 srgbToLinear(const vec4& color) {
        return vec4(std::pow(color.x, 2.2f), std::pow(color.y, 2.2f), std::pow(color.z, 2.2f), color.w);
    }
}
