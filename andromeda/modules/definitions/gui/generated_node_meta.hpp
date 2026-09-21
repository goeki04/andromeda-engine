#pragma once

// ============================================================================
//  AUTO-GENERATED FILE - DO NOT EDIT BY HAND
//  Produced by metaData/gen_struct_meta.py
//  Sources:
//      modules/definitions/gui/a_Nodes.hpp
//  Changes to the source headers are picked up on the next build
//  (target: generate_ecs_metadata).
// ============================================================================

// FieldInfo, StructInfo and forEachField live in the hand-written core header, so that
// several generated files can coexist in one translation unit without redefining them.
#include "a_meta_core.hpp"

#include "a_Nodes.hpp"

namespace Andromeda::Meta {


    // ------------------------------------------------------------------------
    // Andromeda::Gui::Node::AddNode (3 fields) from modules/definitions/gui/a_Nodes.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::Gui::Node::AddNode> {
        using type = ::Andromeda::Gui::Node::AddNode;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "AddNode";
        static constexpr std::string_view qualifiedName = "Andromeda::Gui::Node::AddNode";
        static constexpr std::string_view header = "modules/definitions/gui/a_Nodes.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("a", "Input<float>", "", "", &type::a),
            makeField("b", "Input<float>", "", "", &type::b),
            makeField("result", "Output<float>", "", "", &type::result)
        );

        static constexpr std::array<std::string_view, 3> fieldNames = {"a", "b", "result"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    // ------------------------------------------------------------------------
    // Andromeda::Gui::Node::IntVariable (2 fields) from modules/definitions/gui/a_Nodes.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::Gui::Node::IntVariable> {
        using type = ::Andromeda::Gui::Node::IntVariable;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "IntVariable";
        static constexpr std::string_view qualifiedName = "Andromeda::Gui::Node::IntVariable";
        static constexpr std::string_view header = "modules/definitions/gui/a_Nodes.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("value", "Param<i32>", "", "", &type::value),
            makeField("out", "Output<i32>", "", "", &type::out)
        );

        static constexpr std::array<std::string_view, 2> fieldNames = {"value", "out"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    // ------------------------------------------------------------------------
    // Andromeda::Gui::Node::FloatVariable (2 fields) from modules/definitions/gui/a_Nodes.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::Gui::Node::FloatVariable> {
        using type = ::Andromeda::Gui::Node::FloatVariable;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "FloatVariable";
        static constexpr std::string_view qualifiedName = "Andromeda::Gui::Node::FloatVariable";
        static constexpr std::string_view header = "modules/definitions/gui/a_Nodes.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("value", "Param<float>", "", "", &type::value),
            makeField("out", "Output<float>", "", "", &type::out)
        );

        static constexpr std::array<std::string_view, 2> fieldNames = {"value", "out"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    // ------------------------------------------------------------------------
    // Andromeda::Gui::Node::BoolVariable (2 fields) from modules/definitions/gui/a_Nodes.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::Gui::Node::BoolVariable> {
        using type = ::Andromeda::Gui::Node::BoolVariable;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "BoolVariable";
        static constexpr std::string_view qualifiedName = "Andromeda::Gui::Node::BoolVariable";
        static constexpr std::string_view header = "modules/definitions/gui/a_Nodes.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("value", "Param<bool>", "", "", &type::value),
            makeField("out", "Output<bool>", "", "", &type::out)
        );

        static constexpr std::array<std::string_view, 2> fieldNames = {"value", "out"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    /** @brief Every struct this header carries metadata for. */
    using ReflectedNodes = std::tuple<
        ::Andromeda::Gui::Node::AddNode,
        ::Andromeda::Gui::Node::IntVariable,
        ::Andromeda::Gui::Node::FloatVariable,
        ::Andromeda::Gui::Node::BoolVariable
    >;

    /** @brief The same types as ReflectedNodes, as display names in declaration order. */
    inline constexpr std::array<std::string_view, 4> ReflectedNodesNames = {"AddNode", "IntVariable", "FloatVariable", "BoolVariable"};

} // namespace Andromeda::Meta
