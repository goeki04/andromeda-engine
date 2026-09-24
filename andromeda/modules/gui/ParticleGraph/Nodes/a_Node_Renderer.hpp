#pragma once

/**
 * @file a_Node_Renderer.hpp
 * @brief Everything needed to draw and edit a particle node graph, in one include.
 *
 * @details The parts live in their own headers and can also be included directly:
 *          - a_Node_Fields.hpp   value widgets, labels, the pin dot
 *          - a_Node_PinQuery.hpp what is behind a pin: node, role, value type
 *          - a_Node_Links.hpp    drawing links, connecting, deleting
 *          - a_Node_Menu.hpp     the "Add Node" popup
 *          - a_Node_Builder.hpp  the layout of a single node
 */

#include "a_Node_Builder.hpp"
#include "a_Node_Fields.hpp"
#include "a_Node_Links.hpp"
#include "a_Node_Menu.hpp"
#include "a_Node_PinQuery.hpp"
#include "a_bindable_fields.hpp"
#include "a_particle_group.hpp"
#include "generated_particle_group_meta.hpp"

// The ParticleGroup fields an event can be bound to, and how many channels each of them has.
inline constexpr auto g_BindableFieldNames = Andromeda::makeBindableFieldNames<Andromeda::ParticleGroup>();
inline constexpr auto g_BindableFieldChannels = Andromeda::makeBindableFieldChannels<Andromeda::ParticleGroup>();
