// JSON form of the ECS components. Every component in the generated ComponentDirectory is checked,
// so a new component without (or with broken) JSON bindings fails here instead of in a saved scene.

#include <gtest/gtest.h>
#include <tuple>
#include <typeinfo>
#include "a_component_parser.hpp"
#include "a_components.hpp"
#include "generated_components.hpp"

using namespace Andromeda;
using namespace Andromeda::ECS::Component;

namespace {
    template<typename C>
    void expectRoundTrip() {
        SCOPED_TRACE(typeid(C).name());
        const C original{};
        const nlohmann::json saved = original;
        const C loaded = saved.get<C>();
        EXPECT_EQ(nlohmann::json(loaded), saved);
    }

    template<typename... Components>
    void expectEveryComponentRoundTrips(std::tuple<Components...>* /*directory*/) {
        (expectRoundTrip<Components>(), ...);
    }
}

TEST(ComponentJson, EveryComponentOfTheDirectoryRoundTrips) {
    expectEveryComponentRoundTrips(static_cast<ComponentDirectory*>(nullptr));
}

TEST(ComponentJson, TransformLayoutInSceneFilesIsStable) {
    // Saved scenes depend on this exact shape: vectors as arrays, the rotation as named fields.
    Transform transform;
    transform.position = { 1.0f, 2.0f, 3.0f };
    transform.scale = { 4.0f, 5.0f, 6.0f };
    transform.rotation = quat(0.5f, 0.5f, -0.5f, 0.5f); // w, x, y, z

    const nlohmann::json expected = nlohmann::json::parse(R"({
        "position": [1.0, 2.0, 3.0],
        "scale": [4.0, 5.0, 6.0],
        "rotation": { "w": 0.5, "x": 0.5, "y": -0.5, "z": 0.5 }
    })");
    EXPECT_EQ(nlohmann::json(transform), expected);
}

TEST(ComponentJson, VectorWithTheWrongNumberOfElementsIsRejected) {
    // A hand-edited scene with "position": [1, 2] must fail loudly, not leave z at garbage.
    const nlohmann::json broken = nlohmann::json::parse(R"({
        "position": [1.0, 2.0], "scale": [1.0, 1.0, 1.0], "rotation": { "w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0 }
    })");

    EXPECT_ANY_THROW((void)broken.get<Transform>());
}
