// The undo history stores commands as raw bytes back to back; the risky part is the offset
// bookkeeping when the oldest command is evicted.

#include <gtest/gtest.h>
#include "a_undo_buffer.hpp"
#include "a_registry.hpp"

using namespace Andromeda;
using namespace Andromeda::Editor::Undo;
using ECS::Component::Transform;

class UndoBufferTest : public ::testing::Test {
protected:
    ECS::ComponentRegistry m_Scene;
    ECS::Entity m_Entity = m_Scene.createEntity();
    UndoBuffer m_History;

    void SetUp() override {
        m_Scene.addComponent(m_Entity, Transform{});
    }

    Transform& transform() {
        return m_Scene.getPool<Transform>().get(m_Entity);
    }

    /** @brief Moves the entity to x = @p x and records the step, as the gizmo does. */
    void moveTo(float x) {
        const Transform before = transform();
        transform().position.x = x;
        m_History.pushCommand(CommandType::UpdateTransform, UndoTransformData{ m_Entity, before, transform() });
    }
};

TEST_F(UndoBufferTest, UndoRestoresThePreviousState) {
    transform().position = { 1.0f, 2.0f, 3.0f };
    moveTo(10.0f);

    m_History.undo(m_Scene);

    EXPECT_EQ(transform().position, vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(UndoBufferTest, UndoWalksBackOneStepAtATime) {
    moveTo(1.0f);
    moveTo(2.0f);
    moveTo(3.0f);

    m_History.undo(m_Scene);
    EXPECT_EQ(transform().position.x, 2.0f);
    m_History.undo(m_Scene);
    EXPECT_EQ(transform().position.x, 1.0f);
    m_History.undo(m_Scene);
    EXPECT_EQ(transform().position.x, 0.0f);
}

TEST_F(UndoBufferTest, HistoryKeepsTheNewestStepsAndEvictsTheOldest) {
    // After evicting, every remaining offset is shifted; a mistake there makes the undo read a
    // payload from the middle of another command.
    const int steps = static_cast<int>(UndoBuffer::maxUndoSteps) + 10;
    for (int i = 1; i <= steps; ++i) {
        moveTo(static_cast<float>(i));
    }
    ASSERT_EQ(m_History.size(), UndoBuffer::maxUndoSteps);

    for (int expected = steps - 1; m_History.size() > 0; --expected) {
        m_History.undo(m_Scene);
        ASSERT_EQ(transform().position.x, static_cast<float>(expected)) << "after undoing back to step " << expected;
    }
    // The ten oldest steps are gone: the history ends at the state before step 11.
    EXPECT_EQ(transform().position.x, 10.0f);
}

TEST_F(UndoBufferTest, UndoWithAnEmptyHistoryChangesNothing) {
    transform().position.x = 5.0f;

    m_History.undo(m_Scene);

    EXPECT_EQ(transform().position.x, 5.0f);
    EXPECT_EQ(m_History.size(), 0u);
}
