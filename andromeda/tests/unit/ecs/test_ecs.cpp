#include <gtest/gtest.h>
#include <algorithm>

#include "a_registry.hpp"

using namespace Andromeda::ECS;
using Andromeda::ECS::Component::Transform;
using Andromeda::ECS::Component::Tag;

// ---------------------------------------------------------------------------
// ComponentPool (sparse set)
// ---------------------------------------------------------------------------

TEST(ComponentPool, AddHasGet) {
    ComponentPool<Transform> pool;
    Transform t;
    t.position = { 1.0f, 2.0f, 3.0f };
    pool.add(5, t);

    EXPECT_TRUE(pool.has(5));
    EXPECT_FALSE(pool.has(0));
    EXPECT_FALSE(pool.has(99));
    EXPECT_FLOAT_EQ(pool.get(5).position.x, 1.0f);
    EXPECT_FLOAT_EQ(pool.get(5).position.z, 3.0f);
}

TEST(ComponentPool, RemoveUsesSwapAndPopButKeepsDataConsistent) {
    ComponentPool<Tag> pool;
    pool.add(1, Tag{ "a" });
    pool.add(2, Tag{ "b" });
    pool.add(3, Tag{ "c" });

    pool.removeEntity(2); // middle element -> last ("c") is swapped into its slot

    EXPECT_FALSE(pool.has(2));
    EXPECT_TRUE(pool.has(1));
    EXPECT_TRUE(pool.has(3));
    EXPECT_EQ(pool.getEntities().size(), 2u);
    // The surviving entities must still map to the correct components.
    EXPECT_EQ(pool.get(1).name, "a");
    EXPECT_EQ(pool.get(3).name, "c");
}

TEST(ComponentPool, RemoveNonExistentIsSafe) {
    ComponentPool<Tag> pool;
    pool.add(1, Tag{ "a" });
    EXPECT_NO_THROW(pool.removeEntity(999));
    EXPECT_TRUE(pool.has(1));
    EXPECT_EQ(pool.getEntities().size(), 1u);
}

TEST(ComponentPool, SerializeDeserializeRoundTrip) {
    ComponentPool<Transform> pool;
    Transform a; a.position = { 1.0f, 0.0f, 0.0f };
    Transform b; b.position = { 0.0f, 2.0f, 0.0f };
    pool.add(0, a);
    pool.add(4, b);

    const auto json = pool.serializePool();

    ComponentPool<Transform> restored;
    restored.deserializePool(json);

    // Entities and data restored...
    EXPECT_TRUE(restored.has(0));
    EXPECT_TRUE(restored.has(4));
    EXPECT_FLOAT_EQ(restored.get(4).position.y, 2.0f);
    // ...and the sparse index rebuilt correctly (gaps report false).
    EXPECT_FALSE(restored.has(2));
}

// ---------------------------------------------------------------------------
// ComponentRegistry
// ---------------------------------------------------------------------------

TEST(ComponentRegistry, CreateEntityAssignsSequentialIdsAndTracksThem) {
    ComponentRegistry reg;
    const Entity e0 = reg.createEntity();
    const Entity e1 = reg.createEntity();

    EXPECT_EQ(e0, 0u);
    EXPECT_EQ(e1, 1u);
    EXPECT_EQ(reg.m_NextID, 2u);
    EXPECT_EQ(reg.getAllEntities().size(), 2u);
}

TEST(ComponentRegistry, AddAndHasComponent) {
    ComponentRegistry reg;
    const Entity e0 = reg.createEntity();
    const Entity e1 = reg.createEntity();

    reg.addComponent<Tag>(e0, Tag{ "hello" });

    EXPECT_TRUE(reg.hasComponent<Tag>(e0));
    EXPECT_FALSE(reg.hasComponent<Tag>(e1));
    EXPECT_FALSE(reg.hasComponent<Transform>(e0)); // pool doesn't even exist yet
}

TEST(ComponentRegistry, DestroyEntityRemovesFromPoolsAndActiveList) {
    ComponentRegistry reg;
    const Entity e0 = reg.createEntity();
    const Entity e1 = reg.createEntity();
    reg.addComponent<Tag>(e0, Tag{ "x" });
    reg.addComponent<Tag>(e1, Tag{ "y" });

    reg.destroyEntity(e0);

    EXPECT_FALSE(reg.hasComponent<Tag>(e0));
    EXPECT_TRUE(reg.hasComponent<Tag>(e1));

    const auto& active = reg.getAllEntities();
    EXPECT_EQ(std::find(active.begin(), active.end(), e0), active.end());
}

TEST(ComponentRegistry, ClearRegistryWipesEverything) {
    ComponentRegistry reg;
    reg.createEntity();
    reg.addComponent<Tag>(0, Tag{ "x" });

    reg.clearRegistry();

    EXPECT_EQ(reg.m_NextID, 0u);
    EXPECT_TRUE(reg.getAllEntities().empty());
    EXPECT_TRUE(reg.m_Pools.empty());
}

TEST(ComponentRegistry, RebuildActiveEntitiesDeduplicatesAcrossPools) {
    ComponentRegistry reg;
    // Entity 10 lives in two pools; entity 20 in one. Pools filled directly,
    // bypassing createEntity (as deserialization does).
    reg.getPool<Transform>().add(10, Transform{});
    reg.getPool<Tag>().add(10, Tag{ "a" });
    reg.getPool<Tag>().add(20, Tag{ "b" });

    reg.m_ActiveEntities.clear();
    reg.rebuildActiveEntities();

    const auto& active = reg.getAllEntities();
    EXPECT_EQ(active.size(), 2u); // 10 appears once despite being in two pools
    EXPECT_NE(std::find(active.begin(), active.end(), 10u), active.end());
    EXPECT_NE(std::find(active.begin(), active.end(), 20u), active.end());
}
