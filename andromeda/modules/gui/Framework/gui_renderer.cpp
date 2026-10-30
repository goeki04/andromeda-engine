#include "gui_renderer.h"
#include <cstdio>
#include <stdexcept>
#include "imgui_internal.h"
#include "window_manager.hpp"
#include <filesystem>
#include "a_MainMenuBar.hpp"
#include "a_HierarchyPanel.hpp"
#include "a_ConsolePanel.hpp"
#include "a_DetailsPanel.hpp"
#include "a_BrowserPanel.hpp"
#include "a_Chart.hpp"
#include "a_ViewportPanel.hpp"
#include "a_Style.hpp"
#include <implot.h>
#include "a_ParticleEditor.hpp"
#include "a_subsystem_manager.hpp"
#include "a_ProjectSettingsPanel.hpp"
#include "IconsLucide.h"
namespace {
    /// Section name this handler owns inside imgui.ini.
    constexpr const char* kPanelSettingsType = "AndromedaPanels";

    /**
     * @brief Resolves a "[AndromedaPanels][<name>]" section to the panel it refers to.
     * @details Returning nullptr makes ImGui skip the section's lines, which is exactly what
     *          should happen for a panel that no longer exists - an old ini then ages out
     *          quietly instead of needing migration.
     */
    void* panelSettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name) {
        auto* renderer = static_cast<Andromeda::Gui::GuiRenderer*>(handler->UserData);
        for (auto& panel : renderer->m_Panels) {
            if (panel->getName() && std::string_view(panel->getName()) == name) {
                return panel.get();
            }
        }
        return nullptr;
    }

    /** @brief Reads one "Open=<0|1>" line into the panel that panelSettingsReadOpen resolved. */
    void panelSettingsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
        auto* panel = static_cast<Andromeda::Gui::EditorPanel*>(entry);
        int open = 0;
        if (std::sscanf(line, "Open=%d", &open) == 1) {
            panel->m_IsOpen = open != 0;
        } else {
            panel->readSettings(line);
        }
    }

    /** @brief Appends every panel's visibility to the ini buffer ImGui is about to write. */
    void panelSettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
        auto* renderer = static_cast<Andromeda::Gui::GuiRenderer*>(handler->UserData);
        for (const auto& panel : renderer->m_Panels) {
            if (!panel->getName()) {
                continue;
            }
            buf->appendf("[%s][%s]\n", kPanelSettingsType, panel->getName());
            buf->appendf("Open=%d\n", panel->m_IsOpen ? 1 : 0);
            panel->writeSettings(*buf);
            buf->appendf("\n");
        }
    }

    void drawToolBar() {
		ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float height = ImGui::GetFrameHeight();
        const ImGuiWindowFlags wf = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

        if (ImGui::BeginViewportSideBar("##MainToolBar", viewport, ImGuiDir_Up, height, wf))
        {
            if (ImGui::BeginMenuBar())
            {
                const char* label = Andromeda::SystemManager::s_paused ? ICON_LC_PLAY : ICON_LC_PAUSE;
                const float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ImGui::SetCursorPosX((ImGui::GetWindowSize().x - w) * 0.5f);
                if (ImGui::Button(label)) {
                    Andromeda::SystemManager::s_paused = !Andromeda::SystemManager::s_paused;
                }
                ImGui::EndMenuBar();
            }
        }
        ImGui::End();
    }
}

namespace Andromeda::Gui {

    void Gui::GuiRenderer::init(EditorContext& editorContext) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui_ImplSDL3_InitForOpenGL(editorContext.windowContext.window, editorContext.windowContext.glContext);
        ImGui_ImplOpenGL3_Init(editorContext.windowContext.glslVersion);
        ImGui::StyleColorsDark();
        setFlags();
        setStyle();
        loadFont();

        m_Panels.push_back(std::make_unique<HierarchyPanel>("Hierarchy"));
        m_Panels.push_back(std::make_unique<ConsolePanel>("Console"));
        m_Panels.push_back(std::make_unique<DetailsPanel>("Details"));
        m_Panels.push_back(std::make_unique<BrowserPanel>("Browser"));
        m_Panels.push_back(std::make_unique<ViewportPanel>("Viewport"));
        m_Panels.push_back(std::make_unique<ParticleEditor>("Particle Editor"));
        m_Panels.push_back(std::make_unique<ProjectSettingsPanel>("Project Settings"));
        m_Panels.push_back(std::make_unique<Chart>("Chart"));
        
        for (auto& panel : m_Panels) {
            panel->initPanel(editorContext);
        }

        registerPanelSettings();
    }

    void GuiRenderer::registerPanelSettings() {
        // imgui.ini stores a window's position, size, collapsed state and docking, but not
        // whether it is open: p_open points at application memory, so ImGui cannot own it. A
        // settings handler adds our own section to that same file, which keeps panel
        // visibility next to the dock layout it belongs with.
        //
        // Registered here because ImGui loads the ini lazily on the first NewFrame - any later
        // and the saved values would be read before this handler exists to claim them. The
        // panels are already constructed above, so ReadOpen can resolve names right away.
        ImGuiSettingsHandler handler;
        handler.TypeName = kPanelSettingsType;
        handler.TypeHash = ImHashStr(kPanelSettingsType);
        handler.ReadOpenFn = panelSettingsReadOpen;
        handler.ReadLineFn = panelSettingsReadLine;
        handler.WriteAllFn = panelSettingsWriteAll;
        handler.UserData = this;

        // AddSettingsHandler copies the struct, so the local going out of scope is fine.
        ImGui::AddSettingsHandler(&handler);
    }

    void GuiRenderer::update(EditorContext& editorContext) {

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        MainMenuBar::drawMainMenuBar(editorContext);
        drawToolBar();

        constexpr ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiID dockspaceID = ImGui::DockSpaceOverViewport(ImGui::GetID("MyDockSpace"), ImGui::GetMainViewport());
        if (ImGui::DockBuilderGetNode(dockspaceID) == nullptr || ImGui::DockBuilderGetNode(dockspaceID)->ChildNodes[0] == 0) {
            DockBuilder::setDefaultLayout(dockspaceID);
        }

        for (auto& panel : m_Panels)
        {
            if (panel->m_IsOpen)
            {
                panel->onGuiRender(editorContext);
            }
        }
        EndFrame(Window::g_Window);
    }

    void GuiRenderer::EndFrame(SDL_Window* window) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        const ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            const SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();

            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();

            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }
        SDL_GL_SwapWindow(window);
    }

    bool GuiRenderer::isPanelOpen(std::string_view name) const {
        for (const auto& panel : m_Panels) {
            if (panel->getName() && std::string_view(panel->getName()) == name) {
                return panel->m_IsOpen;
            }
        }
        return false;
    }
    void GuiRenderer::setPanelOpen(std::string_view name, bool open) {
        for (auto& panel : m_Panels) {
            if (panel->getName() && std::string_view(panel->getName()) == name) {
                panel->m_IsOpen = open;
                // Without this ImGui has no reason to rewrite the ini, and a toggle made late
                // in a session would be lost.
                ImGui::MarkIniSettingsDirty();
                return;
            }
        }
    }
    std::vector<std::string_view> GuiRenderer::getPanelNames() const{
        std::vector<std::string_view> names;
        names.reserve(m_Panels.size());
        for (const auto& panel : m_Panels) {
            if (panel->getName()) {
                names.emplace_back(panel->getName());
            }
        }
        return names;
    }
    void GuiRenderer::loadFont()
    {
        ImGuiIO& io = ImGui::GetIO();(void)io;
        ImFont* font = io.Fonts->AddFontFromFileTTF(ASSET_PATH "fonts/Roboto-Regular.ttf", 18.0f);
        if (font == nullptr) {
            throw std::runtime_error("failed loading Roboto font");
        }

        static const ImWchar iconRanges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };
        ImFontConfig iconConfig;
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.GlyphMinAdvanceX = 18.0f;
        // Lucide's glyphs sit higher in the line than Roboto's letters: measured at 18 px, the icon centers
        // lie around y = 7 while lowercase text centers at 10.5. Shifting every icon down by 3 px puts
        // icons next to text (and inside buttons) on the text's middle. ImGui scales this for other sizes.
        iconConfig.GlyphOffset = ImVec2(0.0f, 3.0f);
        io.Fonts->AddFontFromFileTTF(ASSET_PATH "fonts/lucide.ttf", 18.0f, &iconConfig, iconRanges);

        io.FontDefault = font;
    }

    void GuiRenderer::OpenFolder()
    {
        const auto targetDirectory = std::filesystem::current_path().parent_path() / "Licenses";
#if defined(_WIN32)
        const std::string cmd = "explorer \"" + targetDirectory.string() + "\"";
#elif defined(__APPLE__)
        const std::string cmd = "open \"" + targetDirectory.string() + "\"";
#else
        const std::string cmd = "xdg-open \"" + targetDirectory.string() + "\"";
#endif
        system(cmd.c_str());
    }

    void GuiRenderer::OpenURL(const std::string& url)
    {
        SDL_OpenURL(url.c_str());
    }

    void GuiRenderer::destroy() {
        ImGui::DestroyPlatformWindows();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        if (ImGui::GetCurrentContext()) {
            ImGui::DestroyContext();
        }
    }
    void GuiRenderer::setFlags()
    {
        ImGuiIO& io = ImGui::GetIO();(void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        // imgui 1.92 DPI handling: re-rasterize fonts and rescale viewports when the window
        // moves to a display with a different scale, instead of stretching a 1x bitmap.
        io.ConfigDpiScaleFonts = true;
        io.ConfigDpiScaleViewports = true;
    }
}