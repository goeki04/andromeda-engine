#include "a_ProjectSettingsPanel.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>
#include "IconsLucide.h"
#include <filesystem>
#include "a_EditorContext.hpp"
#include "a_FolderDialog.hpp"
#include "a_ProjectSettings.hpp"
#include "resource_manager.h"
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

        /**
         * @brief The reverse of displayPath(): turns a typed or picked path into its stored form.
         * @details Relative to the project root when it lies inside the project, absolute otherwise,
         *          always with '/' and without a trailing slash - so "assets" and "assets/" do not
         *          end up as two entries. A drive root ("C:/") or "/" keeps its slash, without it
         *          it would mean something else.
         */
        std::string toStoredPath(const std::string_view rawPath) {
            namespace fs = std::filesystem;
            const fs::path projectRoot = fs::path(ASSET_PATH).parent_path().parent_path();
            const fs::path relative = fs::path(rawPath).lexically_normal().lexically_relative(projectRoot);

            const bool insideProject = !relative.empty() && *relative.begin() != "..";
            std::string path = (insideProject ? relative : fs::path(rawPath).lexically_normal()).generic_string();
            while (path.size() > 1 && path.back() == '/' && path[path.size() - 2] != ':') {
                path.pop_back();
            }
            return path;
        }
    }

    void ProjectSettingsPanel::addDirectory(EditorContext& ctx, std::string_view rawPath) {
        const std::string path = toStoredPath(rawPath);

        std::vector<std::string>& directories = ctx.projectSettings->assets.directories;
        if (path.empty() || std::ranges::find(directories, path) != directories.end()) {
            return;
        }
        directories.push_back(path);
        m_SelectedDirectory = static_cast<i32>(directories.size()) - 1;
        m_Dirty = true;
        // Scanned right away, so the folder's assets are in the registry without a restart.
        ctx.resourceManager->scanAssetDirectory(path);
    }

    void ProjectSettingsPanel::drawGeneral(EditorContext& ctx) {
        ProjectSettings::General& general = ctx.projectSettings->general;

        // Text fields and sliders report every keystroke or drag step. Saving only once the field is
        // left after a change keeps project.json from being rewritten per letter.
        ImGui::SeparatorText("Project");
        ImGui::InputText("Name", &general.projectName);
        m_Dirty |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::InputText("Version", &general.version);
        m_Dirty |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::InputTextWithHint("Startup Scene", "scenes/main.scene", &general.startupScene);
        m_Dirty |= ImGui::IsItemDeactivatedAfterEdit();

        ImGui::SeparatorText("Editor");
        m_Dirty |= ImGui::Checkbox("Reopen last scene on startup", &general.reopenLastScene);
        m_Dirty |= ImGui::Checkbox("Autosave", &general.autosave);
        ImGui::BeginDisabled(!general.autosave);
        ImGui::SliderInt("Autosave interval (min)", &general.autosaveMinutes, 1, 30);
        m_Dirty |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::EndDisabled();
    }

    void ProjectSettingsPanel::drawRendering(EditorContext& ctx) {
        ProjectSettings::Rendering& rendering = ctx.projectSettings->rendering;
        ImGui::SeparatorText("Backend");

        // project.json can be edited by hand - keep a value outside the list from indexing past it.
        const i32 apiIndex = std::clamp(rendering.graphicsApi, 0, static_cast<i32>(GraphicsApis.size()) - 1);
        if (ImGui::BeginCombo("Graphics API", GraphicsApis[apiIndex].name)) {
            for (i32 i = 0; i < static_cast<i32>(GraphicsApis.size()); ++i) {
                const GraphicsApiOption& api = GraphicsApis[i];
                const bool selected = apiIndex == i;
                ImGui::BeginDisabled(!api.implemented);
                if (ImGui::Selectable(api.name, selected)) {
                    rendering.graphicsApi = i;
                    m_Dirty = true;
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

        m_Dirty |= ImGui::Checkbox("VSync", &rendering.vsync);

        const auto current = std::ranges::find(MsaaOptions, rendering.msaaSamples, &MsaaOption::samples);
        const char* preview = current != MsaaOptions.end() ? current->name : MsaaOptions.front().name;
        if (ImGui::BeginCombo("MSAA", preview)) {
            for (const MsaaOption& option : MsaaOptions) {
                const bool selected = option.samples == rendering.msaaSamples;
                if (ImGui::Selectable(option.name, selected)) {
                    rendering.msaaSamples = option.samples;
                    m_Dirty = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void ProjectSettingsPanel::drawAssets(EditorContext& ctx) {
        ProjectSettings::Assets& assets = ctx.projectSettings->assets;
        if (m_SelectedDirectory >= static_cast<i32>(assets.directories.size())) {
            m_SelectedDirectory = -1;
        }

        ImGui::SeparatorText("Asset Directories");

        ImGui::BeginChild("##AssetDirectories", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        if (assets.directories.empty()) {
            ImGui::TextDisabled("No directories. Add one below.");
        }
        for (i32 i = 0; i < static_cast<i32>(assets.directories.size()); ++i) {
            ImGui::PushID(i);
            char label[600];
            std::snprintf(label, sizeof(label), "%s  %s", ICON_LC_FOLDER, displayPath(assets.directories[i]).c_str());
            if (ImGui::Selectable(label, m_SelectedDirectory == i)) {
                m_SelectedDirectory = i;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        const bool submitted = ImGui::InputTextWithHint("##NewDirectory", "assets/...", &m_NewDirectory,
                                                        ImGuiInputTextFlags_EnterReturnsTrue);
        const bool canAdd = !m_NewDirectory.empty() &&
                            std::ranges::find(assets.directories, m_NewDirectory) == assets.directories.end();

        ImGui::SameLine();
        ImGui::BeginDisabled(!canAdd);
        const bool addClicked = ImGui::SmallButton(ICON_LC_PLUS);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Add directory");
        if ((addClicked || submitted) && canAdd) {
            addDirectory(ctx, m_NewDirectory);
            m_NewDirectory.clear();
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedDirectory < 0);
        const bool removeClicked = ImGui::SmallButton(ICON_LC_MINUS);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Remove selected directory");
        if (removeClicked && m_SelectedDirectory >= 0) {
            ctx.resourceManager->forgetAssetDirectory(assets.directories[m_SelectedDirectory]);
            assets.directories.erase(assets.directories.begin() + m_SelectedDirectory);
            const i32 remaining = static_cast<i32>(assets.directories.size());
            m_SelectedDirectory = remaining == 0 ? -1 : std::min(m_SelectedDirectory, remaining - 1);
            m_Dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_LC_FOLDER_OPEN)) {
            void* hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(ctx.windowContext.window),
                                                SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
            for (const std::string& path : OS::pickFolders(hwnd, ASSET_PATH)) {
                addDirectory(ctx, path);
            }
        }
        ImGui::SetItemTooltip("Browse for folders");

        ImGui::SeparatorText("Asset Database");
        // Shown in full like the directory list, stored relative. The text lives in a member,
        // not in a temporary rebuilt every frame: on the frame the field is left, ImGui reports
        // the finished edit but writes nothing more into the buffer, so a temporary would
        // already hold the old path again and the edit would be lost.
        if (m_DatabaseInput.empty()) {
            m_DatabaseInput = displayPath(assets.databaseDirectory);
        }
        ImGui::InputText("Database folder", &m_DatabaseInput);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            const std::string stored = toStoredPath(m_DatabaseInput);
            if (!stored.empty()) {
                assets.databaseDirectory = stored;
                m_Dirty = true;
            }
        }
        // Refreshed only while nobody types in it, so an edit in progress is not overwritten.
        if (!ImGui::IsItemActive()) {
            m_DatabaseInput = displayPath(assets.databaseDirectory);
        }
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
            if (ctx.projectSettings == nullptr || ctx.resourceManager == nullptr) {
                ImGui::TextDisabled("No project loaded.");
            } else {
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
        }
        ImGui::End();

        // Saved here once per frame at most, however many fields were finished in it.
        if (m_Dirty && ctx.resourceManager != nullptr) {
            ctx.resourceManager->saveProjectSettings();
            m_Dirty = false;
        }
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