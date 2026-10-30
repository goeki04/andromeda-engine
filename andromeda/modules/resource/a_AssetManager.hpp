#pragma once

namespace Andromeda::AssetManagement {
    /**
     * @class AssetManager
     * @brief Maps scenes to the assets they reference.
     * @details Holds, for every scene, the Guids of the assets placed in it. This answers
     *          questions like "which assets does this scene need?" and "which scenes use this
     *          asset?" - for example before deleting an asset, or to load only what a scene needs.
     *
     *          The manager stores Guids only and never loads files itself. Resolving a Guid to
     *          its data or path is the job of AssetRegistry. A Guid that the registry no longer
     *          knows marks a missing asset in the scene, not an error in the manager.
     * @see AssetRegistry
     */
    class AssetManager {

    };
}