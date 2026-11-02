#pragma once

#include <tuple>

namespace Andromeda::ECS::Component {
    struct AABB;
    struct Material;
    struct MeshRenderer;
    struct ParticleSystem;
    struct Tag;
    struct Transform;

    using ComponentDirectory = std::tuple<
        AABB,
        Material,
        MeshRenderer,
        ParticleSystem,
        Tag,
        Transform
    >;
}
