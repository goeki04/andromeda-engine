// The EventManager's promise is reentrancy: listeners may add and remove listeners, and dispatch
// further events, from inside a dispatch without breaking the loop that is calling them.

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "a_event_manager.hpp"
#include "a_EventManagerGuard.hpp"

using namespace Andromeda;
using Andromeda::Test::EventManagerGuard;

namespace {
    void dispatchMeshReloaded(u32 meshID = 1) {
        EventManager::getInstance().Dispatch(MeshReloaded::GetStaticType(), MeshReloaded(meshID));
    }
    void dispatchFileChanged() {
        EventManager::getInstance().Dispatch(FileChanged::GetStaticType(), FileChanged(FileAction::Modified, "a.obj"));
    }
}

TEST(EventManager, DispatchReachesOnlyListenersOfThatType) {
    EventManagerGuard events;
    std::vector<u32> meshes;
    int fileEvents = 0;
    events.listen<MeshReloaded>([&](const MeshReloaded& e) { meshes.push_back(e.meshID); });
    events.listen<FileChanged>([&](const FileChanged&) { ++fileEvents; });

    dispatchMeshReloaded(42);

    EXPECT_EQ(meshes, std::vector<u32>{ 42 });
    EXPECT_EQ(fileEvents, 0);
}

TEST(EventManager, ListenersRunInTheOrderTheyWereAdded) {
    EventManagerGuard events;
    std::string order;
    events.listen<MeshReloaded>([&](const MeshReloaded&) { order += "a"; });
    events.listen<MeshReloaded>([&](const MeshReloaded&) { order += "b"; });
    events.listen<MeshReloaded>([&](const MeshReloaded&) { order += "c"; });

    dispatchMeshReloaded();

    EXPECT_EQ(order, "abc");
}

TEST(EventManager, RemovedListenerIsNotCalledAgain) {
    EventManagerGuard events;
    int calls = 0;
    const EventListenerID id = events.listen<MeshReloaded>([&](const MeshReloaded&) { ++calls; });

    EventManager::getInstance().RemoveEventListener(id);
    dispatchMeshReloaded();

    EXPECT_EQ(calls, 0);
}

TEST(EventManager, ListenerCanRemoveItselfWhileBeingCalled) {
    // A one-shot listener. Erasing it right away would pull the element out from under the loop.
    EventManagerGuard events;
    int calls = 0;
    int laterCalls = 0;
    EventListenerID self{};
    self = events.listen<MeshReloaded>([&](const MeshReloaded&) {
        ++calls;
        EventManager::getInstance().RemoveEventListener(self);
    });
    events.listen<MeshReloaded>([&](const MeshReloaded&) { ++laterCalls; });

    dispatchMeshReloaded();
    dispatchMeshReloaded();

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(laterCalls, 2) << "the listener after it must still run in both dispatches";
}

TEST(EventManager, ListenerRemovedByAnEarlierOneIsSkippedInTheSameDispatch) {
    EventManagerGuard events;
    int victimCalls = 0;
    EventListenerID victim{};
    events.listen<MeshReloaded>([&](const MeshReloaded&) { EventManager::getInstance().RemoveEventListener(victim); });
    victim = events.listen<MeshReloaded>([&](const MeshReloaded&) { ++victimCalls; });

    dispatchMeshReloaded();

    EXPECT_EQ(victimCalls, 0);
}

TEST(EventManager, ListenerAddedDuringADispatchHearsOnlyTheNextEvent) {
    // Regression: the new listener was appended to the very vector the dispatch iterated over. When
    // that reallocated, the loop went on in freed memory: the listener after the adding one was
    // called through a moved-from std::function ("bad function call"; AddressSanitizer reports a
    // heap-use-after-free in Dispatch).
    // KeyDown is used by no other test: the singleton keeps a vector's capacity after its listeners
    // are gone, and only a vector that starts empty is sure to reallocate on the third listener.
    EventManagerGuard events;
    int addedCalls = 0;
    int nextCalls = 0;
    bool added = false;
    events.listen<KeyDown>([&](const KeyDown&) {
        if (!added) {
            added = true;
            events.listen<KeyDown>([&](const KeyDown&) { ++addedCalls; });
        }
    });
    events.listen<KeyDown>([&](const KeyDown&) { ++nextCalls; });
    const auto pressKey = [] {
        EventManager::getInstance().Dispatch(KeyDown::GetStaticType(), KeyDown(Keycode::A));
    };

    pressKey();
    EXPECT_EQ(nextCalls, 1) << "the listener after the adding one must still be called, once";
    EXPECT_EQ(addedCalls, 0) << "a listener must not hear the event it was added during";

    pressKey();
    EXPECT_EQ(nextCalls, 2);
    EXPECT_EQ(addedCalls, 1);
}

TEST(EventManager, ListenerAddedAndRemovedWithinOneDispatchNeverRuns) {
    EventManagerGuard events;
    int addedCalls = 0;
    bool done = false;
    events.listen<MeshReloaded>([&](const MeshReloaded&) {
        if (done) return;
        done = true;
        const EventListenerID id = events.listen<MeshReloaded>([&](const MeshReloaded&) { ++addedCalls; });
        EventManager::getInstance().RemoveEventListener(id);
    });

    dispatchMeshReloaded();
    dispatchMeshReloaded();

    EXPECT_EQ(addedCalls, 0);
}

TEST(EventManager, NestedDispatchDoesNotCleanUpUnderTheOuterOne) {
    // A FileChanged listener triggers a MeshReloaded (as the ResourceManager does on a reload), and
    // a MeshReloaded listener removes a FileChanged listener. Removal must wait until the outer
    // FileChanged dispatch has finished, and the removed listener must not run anymore.
    EventManagerGuard events;
    std::string order;
    EventListenerID removed{};
    events.listen<FileChanged>([&](const FileChanged&) {
        order += "reload ";
        dispatchMeshReloaded();
    });
    removed = events.listen<FileChanged>([&](const FileChanged&) { order += "removed "; });
    events.listen<FileChanged>([&](const FileChanged&) { order += "last "; });
    events.listen<MeshReloaded>([&](const MeshReloaded&) {
        order += "mesh ";
        EventManager::getInstance().RemoveEventListener(removed);
    });

    dispatchFileChanged();

    EXPECT_EQ(order, "reload mesh last ");
}
