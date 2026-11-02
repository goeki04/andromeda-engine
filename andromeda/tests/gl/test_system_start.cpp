#include <gtest/gtest.h>
#include <iostream>
#include "a_subsystem_manager.hpp"
#include "window_manager.hpp"
#include "editor.hpp"
#include "resource_manager.h"
#include "renderer.h"
#include "scene.hpp"
using namespace Andromeda;

class SubsystemStartTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "[ INFO ] Starting Test Setup..." << std::endl;
        auto& manager = SystemManager::getInstance();
        manager.m_Subsystems.clear();
    }
};

TEST_F(SubsystemStartTest, CheckWindowManager) {
    std::cout << "[ STEP ] Initializing WindowManager..." << std::endl;
    Window::WindowManager windowManager;

    ASSERT_NO_THROW({
        windowManager.start();
        }) << "Error in WindowManager::start()!";

    std::cout << "[ OK ] WindowManager started successfully." << std::endl;

    EXPECT_GT(Window::g_WindowWidth, 0)
        << "Error: WindowManager started, but g_WindowWidth is 0. "
        << "This will cause a division by zero in the Renderer later!";

    EXPECT_GT(Window::g_WindowHeight, 0)
        << "Error: WindowManager started, but g_WindowHeight is 0!";
}

TEST_F(SubsystemStartTest, CheckResourceManager) {
    std::cout << "[ STEP ] Initializing ResourceManager..." << std::endl;
    Andromeda::ResourceManager resourceManager;

    ASSERT_NO_THROW({
        resourceManager.start();
        }) << "Error in ResourceManager::start()!";

    std::cout << "[ OK ] ResourceManager started successfully." << std::endl;
}

TEST_F(SubsystemStartTest, CheckRenderer) {
    std::cout << "[ STEP ] Initializing Renderer..." << std::endl;

    auto& manager = Andromeda::SystemManager::getInstance();

    ResourceManager rm;
    Andromeda::SceneManager sm;
    Renderer renderer;

    manager.addSubsystem(&rm);
    manager.addSubsystem(&sm);
    manager.addSubsystem(&renderer);
    rm.start();
    sm.start();
    ASSERT_NO_THROW({
        renderer.start();
        }) << "Error in Renderer::start() - possible null pointer or missing dependency.";

    std::cout << "[ OK ] Renderer started successfully." << std::endl;
}

TEST_F(SubsystemStartTest, CheckEditor) {
    std::cout << "[ STEP ] Initializing Editor..." << std::endl;
    auto& manager = SystemManager::getInstance();
    Editor::Editor editor;
    Renderer renderer;
    ResourceManager rm;
    Andromeda::SceneManager sm;
    Window::WindowManager wm;
    manager.addSubsystem(&wm);
    manager.addSubsystem(&rm);
    manager.addSubsystem(&sm);
    manager.addSubsystem(&renderer);
    manager.addSubsystem(&editor);
    wm.start();
    ASSERT_NO_THROW({
        rm.start();
        }) << "RM ERROR";
    ASSERT_NO_THROW({
    renderer.start();
        }) << "RENDERER ERROR";
    ASSERT_NO_THROW({
        editor.start();
        }) << "Error in Editor::start()";

    std::cout << "[ OK ] Editor started successfully." << std::endl;
}