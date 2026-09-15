#pragma once

/**
 * @file a_particle_group.hpp
 * @brief The particle group and the event bindings that drive its fields at runtime.
 *
 * @details A ParticleGroup is one emitter configuration inside an ECS::Component::ParticleSystem.
 *          Its fields are normally edited by hand in the editor; any field whose type has channels
 *          (see @c ChannelTraits) can additionally be driven by an engine event through an
 *          @c EventBinding.
 *
 *          An EventBinding is pure authoring data: it records *which* event and *which* field are
 *          connected, never a pointer to a live event instance - those exist only for the duration
 *          of a dispatch. A particle group therefore never subscribes to anything. It "listens" to
 *          an event purely by carrying a matching binding, and ParticleBindingSystem walks those
 *          entries once per frame to apply the payload. That is what lets groups and bindings be
 *          created and deleted at runtime without touching any subscription.
 *
 *          Only fields with @c ChannelTraits<T>::count > 0 can be a binding target, so
 *          @c groupName and @c eventBindings are filtered out automatically and never appear in
 *          the field dropdown. Because of that filtering, EventBinding::fieldIndex counts only the
 *          bindable fields and is @b not the declaration index of the member.
 *
 *          See a_particle_binding_system.hpp for the full walkthrough of how a binding is created
 *          and what it takes to make a new event bindable.
 *
 * @note Both structs are scanned by metaData/gen_struct_meta.py. The comment directly above a
 *       struct and the trailing ///< comments on its members are copied verbatim into the
 *       generated StructInfo<> specialization, so keep those short - this file comment is not
 *       picked up and is the right place for longer prose.
 */

#include "a_primitives.hpp"
#include <string>
#include "a_EventTypes.hpp"
#include <variant>
#include <vector>
#include <functional>
namespace Andromeda {

    /** @brief One "event drives field" connection of a particle group, as authored in the editor. */
    struct EventBinding {
        i32 eventChannel = -1;
        i32 eventIndex = -1; ///< Index into Meta::ReflectedEventsNames, -1 while no event is selected.
        i32 fieldIndex = -1; ///< Index into the bindable fields of ParticleGroup, -1 while unselected.
    };

    /** @brief One emitter configuration: how many particles there are, how they look and how they move. */
    struct ParticleGroup {
        std::string groupName = "ParticleGroup_1";  ///< Name of the particle group for identification.
        i32 particleCount = 1000;                ///< Maximum number of particles allowed in this group.
        float size = 1.0f;                       ///< Size of each particle in this group.
        vec3 velocity = {1.0f, 1.0f, 1.0f};      ///< Initial velocity of particles in this group.
        vec3 particleColor = {1.0f, 1.0f, 1.0f}; ///< Color of the particles (RGB).
        float minLifeTime = 0.0f;                ///< Minimum lifetime of the particles in seconds.
        std::vector<EventBinding> eventBindings; ///< List of event bindings for this particle group.
    };
} // namespace Andromeda
