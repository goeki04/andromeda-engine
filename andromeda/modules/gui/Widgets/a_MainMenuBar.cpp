#include "a_MainMenuBar.hpp"
#include "a_EditorContext.hpp"
#include "gui_renderer.h"
#include "sceneSerializer.hpp"
#include "resource_manager.h"
#include "a_Docking.hpp"
#include "a_logger.hpp"
#include <iostream>
#include <filesystem>
extern std::string g_ProjectPath;

namespace Andromeda::Gui {
    void MainMenuBar::drawMainMenuBar(EditorContext& ctx)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FramePadding.y = 9.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground;
        if (ImGui::BeginMainMenuBar())
        {
            drawFileMenu(ctx);
            drawWindowMenu(ctx);
            drawDebugMenu();
            drawInfoMenu();
            ImGui::EndMainMenuBar();
            ImGui::PopStyleVar();
        }
    }
    void MainMenuBar::drawFileMenu(EditorContext& ctx)
    {
        if (ImGui::BeginMenu("File"))
        {
            // Failures are logged, not thrown: a failed save or a missing scene file must not end the editor.
            const std::string scenePath = (std::filesystem::path(g_ProjectPath) / "scene.json").string();
            if (ImGui::MenuItem("Save")) {
                if (SceneSerializer::save(scenePath, *ctx.registry, *ctx.resourceManager))
                    A_INFO("Scene saved to '{}'", scenePath);
                else
                    A_ERROR("Saving the scene to '{}' failed", scenePath);
            }
            if (ImGui::MenuItem("Load")) {
                if (SceneSerializer::load(scenePath, *ctx.registry, *ctx.resourceManager))
                    A_INFO("Scene loaded from '{}'", scenePath);
                else
                    A_ERROR("Loading the scene from '{}' failed, current scene kept", scenePath);
            }
            if (ImGui::MenuItem("Exit")) {}
            ImGui::EndMenu();
        }
    }
    void MainMenuBar::drawDebugMenu()
    {
        if (ImGui::BeginMenu("Debug")) {
            const float fps = ImGui::GetIO().Framerate;
            const float ms = 1000.0f / fps;
            
            ImGui::Text("Performance: %.1f FPS (%.2f ms/frame)", fps, ms);
            ImGui::Separator();

            ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
            ImGui::Text("Renderer: %s", glGetString(GL_RENDERER));
            ImGui::Text("OpenGL Version: %s", glGetString(GL_VERSION));
            ImGui::Text("GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
            ImGui::EndMenu();
        }
    }
    void MainMenuBar::drawInfoMenu()
    {
        if (ImGui::BeginMenu("Info"))
        {
            if (ImGui::MenuItem("Licenses")) {
                GuiRenderer::OpenFolder();
            }
            if (ImGui::MenuItem("SDK")) {
                GuiRenderer::OpenURL("https://www.bosch-sensortec.com/software-tools/software/previous-sdk-bmv-080-versions/");
            }
            if (ImGui::MenuItem("Github")) {
                GuiRenderer::OpenURL("https://github.com/goeki04/andromeda-engine");
            }
            ImGui::EndMenu();
        }
    }
  
    void MainMenuBar::drawWindowMenu(EditorContext& ctx)
    {
        if (ImGui::BeginMenu("Window")) {
            if (ctx.panelController) {
                for (const auto& name : ctx.panelController->getPanelNames()) {
                    bool isOpen = ctx.panelController->isPanelOpen(name);
                    if (ImGui::MenuItem(name.data(), nullptr, &isOpen)) {
                        ctx.panelController->setPanelOpen(name, isOpen);
                    }
                }
            }
            else {
                ImGui::TextDisabled("No Panel Controller found!");
            }
            ImGui::EndMenu();
        }
    }
}