#include "a_ConsolePanel.hpp"
#include "a_EditorContext.hpp"
#include "a_log_sink.hpp"
namespace Andromeda::Gui {
    /** @brief Maps a log level onto the colour its line is drawn in, 0 for the default. */
    static ImU32 colorForLevel(const spdlog::level::level_enum level) {
        switch (level) {
            case spdlog::level::critical: return IM_COL32(255, 110, 110, 255);
            case spdlog::level::err:      return IM_COL32(255, 100, 100, 255);
            case spdlog::level::warn:     return IM_COL32(240, 200,  90, 255);
            case spdlog::level::debug:
            case spdlog::level::trace:    return IM_COL32(150, 150, 160, 255);
            default:                      return 0;
        }
    }

    void ConsolePanel::onGuiRender(EditorContext& ctx) {
        // Drained every frame regardless of whether the panel is open, otherwise the queue
        // grows unbounded while the console is hidden - which is its default state.
        for (auto& line : Log::pendingLines().dequeueAll()) {
            m_Console.addLine(std::move(line.text), colorForLevel(line.level));
        }

        
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(m_Name, &m_IsOpen))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Console"))
                m_IsOpen = false;
            ImGui::EndPopup();
        }

        ImGui::TextWrapped("Enter 'help' for help.");

        if (ImGui::SmallButton("Clear")) { m_Console.clearLog(); }
        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::SmallButton("Copy");

        ImGui::Separator();

        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &m_Console.autoScroll);
            ImGui::EndPopup();
        }

        if (ImGui::Button("Options")) ImGui::OpenPopup("Options");
        ImGui::SameLine();
        m_Console.filter.Draw("Filter", 180);
        ImGui::Separator();

        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::Selectable("Clear")) m_Console.clearLog();
                ImGui::EndPopup();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
            if (copy_to_clipboard) ImGui::LogToClipboard();

            for (const auto& item : m_Console.items)
            {
                if (!m_Console.filter.PassFilter(item.text.c_str()))
                    continue;

                // Lines from the log sink carry their colour; the command handlers still mark
                // their own output with the "[E]" and "# " prefixes.
                ImU32 color = item.color;
                if (color == 0)
                {
                    if (item.text.find("[E]") != std::string::npos)
                        color = IM_COL32(255, 100, 100, 255);
                    else if (item.text.starts_with("# "))
                        color = IM_COL32(255, 205, 150, 255);
                }

                if (color != 0)
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(item.text.c_str());
                if (color != 0)
                    ImGui::PopStyleColor();
            }
            if (copy_to_clipboard) ImGui::LogFinish();

            if (m_Console.scrollToBottom || (m_Console.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);

            m_Console.scrollToBottom = false;
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::Separator();

        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("Input", m_Console.inputBuf.data(), 256, input_text_flags, &Console::AppConsole::textEditCallbackStub, (void*)this))
        {
            const std::string_view inputView = m_Console.inputBuf;
            const size_t start = inputView.find_first_not_of(" \t\r\n");
            const size_t end = inputView.find_last_not_of(" \t\r\n");
            if (start != std::string_view::npos)
            {
                const std::string_view trimmedCmd = inputView.substr(start, end - start + 1);

                const Console::CommandLine cl = m_Console.parseInput(trimmedCmd);
                m_Console.execCommand(cl);
            }
            m_Console.inputBuf[0] = '\0';
            reclaim_focus = true;
        }

        ImGui::SetItemDefaultFocus();
        if (reclaim_focus) ImGui::SetKeyboardFocusHere(-1);

        ImGui::End();
    }
}