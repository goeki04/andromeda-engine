#pragma once

/**
 * @file a_GroupsOverlay.hpp
 * @brief The particle groups overlay: picking the group whose graph is edited, and managing the groups.
 */

#include "a_components.hpp"
#include "a_Primitives.hpp"

namespace Andromeda::Gui::Overlay {

    /** @brief What the groups list has to remember between frames: the row currently being renamed. */
    struct GroupsOverlayState {
        u32 renamingGroupId = 0;      ///< Group being renamed; 0 = none.
        char renameBuffer[64]{};      ///< Edited name while renamingGroupId is set.
        bool renameFocusPending = false; ///< Puts the keyboard into the field in its first frame.
    };

    /**
     * @brief One row per group, with a context menu to rename, duplicate or delete, and one on free space
     *        to add a group.
     * @param shownGroupId The group actually drawn on the canvas; it gets the highlight.
     * @param selectedGroupId Set when a row is clicked, or to the copy after duplicating.
     */
    void drawParticleGroupList(ECS::Component::ParticleSystem& system, u32 shownGroupId, u32& selectedGroupId,
                               GroupsOverlayState& state);
}
