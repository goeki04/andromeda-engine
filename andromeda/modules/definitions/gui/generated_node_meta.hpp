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

    /** @brief Every struct this header carries metadata for. */
    using ReflectedNodes = std::tuple<
        ::Andromeda::Gui::Node::AddNode
    >;

    /** @brief The same types as ReflectedNodes, as display names in declaration order. */
    inline constexpr std::array<std::string_view, 1> ReflectedNodesNames = {"AddNode"};

} // namespace Andromeda::Meta
