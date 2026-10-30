#include "a_ProjectSettingsPanel.hpp"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>
#include "IconsLucide.h"
#include <filesystem>
#include "a_EditorContext.hpp"
#include "a_FolderDialog.hpp"
#include <SDL3/SDL_video.h>
namespace Andromeda::Gui {
    constexpr const char* tabName(ProjectSettingsTab tab) {
        switch (tab) {
        case ProjectSettingsTab::General:
            return "General";
        case ProjectSettingsTab::Rendering:
            return "Rendering";
        case ProjectSettingsTab::Assets:
            return "Assets";
        case ProjectSettingsTab::Plugins:
            return "Plugins";
        case ProjectSettingsTab::Input:
            return "Input";
        case ProjectSettingsTab::Scripting:
            return "Scripting";
        default:
            return "Unknown";
        }
    }

    constexpr const char* tabIcon(ProjectSettingsTab tab) {
        switch (tab) {
        case ProjectSettingsTab::General:
            return ICON_LC_SLIDERS_HORIZONTAL;
        case ProjectSettingsTab::Rendering:
            return ICON_LC_MONITOR;
        case ProjectSettingsTab::Assets:
            return ICON_LC_FOLDER;
        case ProjectSettingsTab::Plugins:
            return ICON_LC_PLUG;
        case ProjectSettingsTab::Input:
            return ICON_LC_KEYBOARD;
        case ProjectSettingsTab::Scripting:
            return ICON_LC_CODE;
        default:
            return "";
        }
    }

    namespace {
        struct GraphicsApiOption {
            const char* name;
            bool implemented;
        };

        constexpr std::array<GraphicsApiOption, 4> GraphicsApis{ {
            {"OpenGL", true},
            {"DirectX 12", false},
            {"Vulkan", false},
            {"Metal", false},
        } };

        struct MsaaOption {
            const char* name;
            i32 samples;
        };
        constexpr std::array<MsaaOption, 4> MsaaOptions{ {
            {"Off", 1},
            {"2x", 2},
            {"4x", 4},
            {"8x", 8},
        } };
    }

    namespace {
        /**
         * @brief Turns a stored asset directory into the full path shown in the list.
         * @details Entries are stored relative to the project root so a saved project still works
         *          when it is moved; the list shows them in full, with backslashes like Explorer.
         *          Entries outside the project are stored absolute already and only get the separators.
         *          Plain string work on purpose: going through std::filesystem::path::string() would
         *          re-encode UTF-8 folder names with the ANSI code page.
         */
        std::string displayPath(const std::string& stored) {
            const bool absolute = (stored.size() >= 2 && stored[1] == ':') ||
                                  (!stored.empty() && (stored[0] == '/' || stored[0] == '\\'));
            // ASSET_PATH is "<project>/assets/" - its parent is the project root.
            static const std::string projectRoot =
                std::filesystem::path(ASSET_PATH).parent_path().parent_path().generic_string();
            std::string full = absolute ? stored : projectRoot + '/' + stored;
            std::replace(full.begin(), full.end(), '/', '\\');
            return full;
        }
    }

    void ProjectSettingsPanel::addDirectory(std::string_view rawPath) {
        namespace fs = std::filesystem;
        const fs::path projectRoot = fs::path(ASSET_PATH).parent_path().parent_path();
        const fs::path relative = fs::path(rawPath).lexically_normal().lexically_relative(projectRoot);

        const bool insideProject = !relative.empty() && *relative.begin() != "..";
        std::string path = (insideProject ? relative : fs::path(rawPath).lexically_normal()).generic_string();
        // One spelling for every entry: no trailing slash, so "assets" and "assets/" do not end up
        // as two directories. A drive root ("C:/") or "/" keeps it - without, it would mean something else.
        while (path.size() > 1 && path.back() == '/' && path[path.size() - 2] != ':') {
            path.pop_back();
        }

        if (path.empty() || std::ranges::find(m_Assets.directories, path) != m_Assets.directories.end()) {
            return;
        }
        m_Assets.directories.push_back(path);
        m_SelectedDirectory = static_cast<i32>(m_Assets.directories.size()) - 1;
    }

    void ProjectSettingsPanel::drawGeneral(EditorContext& ctx) {
        ImGui::SeparatorText("Project");
        ImGui::InputText("Name", m_General.projectName, IM_ARRAYSIZE(m_General.projectName));
        ImGui::InputText("Version", m_General.version, IM_ARRAYSIZE(m_General.version));
        ImGui::InputTextWithHint("Startup Scene", "scenes/main.scene", m_General.startupScene, IM_ARRAYSIZE(m_General.startupScene));

        ImGui::SeparatorText("Editor");
        ImGui::Checkbox("Reopen last scene on startup", &m_General.reopenLastScene);
        ImGui::Checkbox("Autosave", &m_General.autosave);
        ImGui::BeginDisabled(!m_General.autosave);
        ImGui::SliderInt("Autosave interval (min)", &m_General.autosaveMinutes, 1, 30);
        ImGui::EndDisabled();
    }

    void ProjectSettingsPanel::drawRendering(EditorContext& ctx) {
        ImGui::SeparatorText("Backend");

        if (ImGui::BeginCombo("Graphics API", GraphicsApis[m_Rendering.graphicsApi].name)) {
            for (i32 i = 0; i < static_cast<i32>(GraphicsApis.size()); ++i) {
                const GraphicsApiOption& api = GraphicsApis[i];
                const bool selected = m_Rendering.graphicsApi == i;
                ImGui::BeginDisabled(!api.implemented);
                if (ImGui::Selectable(api.name, selected)) {
                    m_Rendering.graphicsApi = i;
                }
                ImGui::EndDisabled();
                if (!api.implemented) {
                    ImGui::SetItemTooltip("No %s backend yet - only OpenGL is implemented.", api.name);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Checkbox("VSync", &m_Rendering.vsync);

        const auto current = std::ranges::find(MsaaOptions, m_Rendering.msaaSamples, &MsaaOption::samples);
        const char* preview = current != MsaaOptions.end() ? current->name : MsaaOptions.front().name;
        if (ImGui::BeginCombo("MSAA", preview)) {
            for (const MsaaOption& option : MsaaOptions) {
                const bool selected = option.samples == m_Rendering.msaaSamples;
                if (ImGui::Selectable(option.name, selected)) {
                    m_Rendering.msaaSamples = option.samples;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void ProjectSettingsPanel::drawAssets(EditorContext& ctx) {
        ImGui::SeparatorText("Asset Directories");

        ImGui::BeginChild("##AssetDirectories", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        if (m_Assets.directories.empty()) {
            ImGui::TextDisabled("No directories. Add one below.");
        }
        for (i32 i = 0; i < static_cast<i32>(m_Assets.directories.size()); ++i) {
            ImGui::PushID(i);
            char label[600];
            std::snprintf(label, sizeof(label), "%s  %s", ICON_LC_FOLDER, displayPath(m_Assets.directories[i]).c_str());
            if (ImGui::Selectable(label, m_SelectedDirectory == i)) {
                m_SelectedDirectory = i;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        const bool submitted = ImGui::InputTextWithHint("##NewDirectory", "assets/...", m_NewDirectory,
                                                        IM_ARRAYSIZE(m_NewDirectory), ImGuiInputTextFlags_EnterReturnsTrue);
        const std::string_view newDirectory(m_NewDirectory);
        const bool canAdd = !newDirectory.empty() &&
                            std::ranges::find(m_Assets.directories, newDirectory) == m_Assets.directories.end();

        ImGui::SameLine();
        ImGui::BeginDisabled(!canAdd);
        const bool addClicked = ImGui::SmallButton(ICON_LC_PLUS);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Add directory");
        if ((addClicked || submitted) && canAdd) {
            addDirectory(newDirectory);
            m_NewDirectory[0] = '\0';
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedDirectory < 0);
        const bool removeClicked = ImGui::SmallButton(ICON_LC_MINUS);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Remove selected directory");
        if (removeClicked && m_SelectedDirectory >= 0) {
            m_Assets.directories.erase(m_Assets.directories.begin() + m_SelectedDirectory);
            const i32 remaining = static_cast<i32>(m_Assets.directories.size());
            m_SelectedDirectory = remaining == 0 ? -1 : std::min(m_SelectedDirectory, remaining - 1);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_LC_FOLDER_OPEN)) {
            void* hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(ctx.windowContext.window),
                                                SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
            for (const std::string& path : OS::pickFolders(hwnd, ASSET_PATH)) {
                addDirectory(path);
            }
        }
        ImGui::SetItemTooltip("Browse for folders");

        ImGui::SeparatorText("Asset Database");
        ImGui::InputText("Database folder", m_Assets.databaseDirectory, IM_ARRAYSIZE(m_Assets.databaseDirectory));
        ImGui::TextDisabled("asset_db.json is stored in this folder.");
    }

    void ProjectSettingsPanel::drawPlugins(EditorContext& ctx) {

    }

    void ProjectSettingsPanel::drawInput(EditorContext& ctx) {

    }

    void ProjectSettingsPanel::drawScripting(EditorContext& ctx) {

    }

    void ProjectSettingsPanel::drawCall(EditorContext& ctx) {

        switch (m_SelectedTab) {
            case ProjectSettingsTab::General:
                drawGeneral(ctx);
                break;
            case ProjectSettingsTab::Rendering:
                drawRendering(ctx);
                break;
            case ProjectSettingsTab::Assets:
                drawAssets(ctx);
                break;
            case ProjectSettingsTab::Plugins:
                drawPlugins(ctx);
                break;
            case ProjectSettingsTab::Input:
                drawInput(ctx);
                break;
            case ProjectSettingsTab::Scripting:
                drawScripting(ctx);
                break;
            default:
                break;
        }
    }

    void ProjectSettingsPanel::initPanel(EditorContext& ctx) {

    }

    void ProjectSettingsPanel::onGuiRender(EditorContext& ctx) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(m_Name, &m_IsOpen, m_WindowFlags)) {
            ImGui::BeginChild("##Categories", ImVec2(180.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX);
            for (i32 i = 0; i < static_cast<i32>(ProjectSettingsTab::Count); ++i) {
                const auto tab = static_cast<ProjectSettingsTab>(i);
                char label[64];
                std::snprintf(label, sizeof(label), "%s  %s", tabIcon(tab), tabName(tab));
                if (ImGui::Selectable(label, m_SelectedTab == tab)) {
                    m_SelectedTab = tab;
                }
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##content");
            drawCall(ctx);
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ProjectSettingsPanel::readSettings(const char* line) {
        constexpr std::string_view prefix = "Category=";
        std::string_view text(line);
        if (!text.starts_with(prefix)) {
            return;
        }
        text.remove_prefix(prefix.size());

        for (int i = 0; i < static_cast<int>(ProjectSettingsTab::Count); ++i) {
            const auto tab = static_cast<ProjectSettingsTab>(i);
            if (text == tabName(tab)) {
                m_SelectedTab = tab;
                return;
            }
        }
    }

    void ProjectSettingsPanel::writeSettings(ImGuiTextBuffer& buf) const {
        buf.appendf("Category=%s\n", tabName(m_SelectedTab));
    }
}