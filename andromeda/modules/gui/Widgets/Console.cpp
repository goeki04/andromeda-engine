#include "console.h"
#include <filesystem>
#include <ranges>

#include "a_primitives.hpp"
#include <sstream>

namespace Andromeda::Gui::Console {

    CommandLine AppConsole::parseInput(const std::string_view input) {
        CommandLine cl;

        if (const size_t space_pos = input.find_first_of(" \t"); space_pos != std::string_view::npos) {
            cl.command = input.substr(0, space_pos);

            std::istringstream iss{ std::string(input.substr(space_pos)) };
            std::string token;
            while (iss >> token) {
                cl.flags.push_back(token);
            }
        }
        else {
            cl.command = input;
        }

        return cl;
    }

    AppConsole::AppConsole()
    {
        initRegistry();
        clearLog();
        historyPos = -1;
        inputBuf.resize(256,'\0');
        autoScroll = true;
        scrollToBottom = false;
        addLog("Type 'help' for command list.");
    }

    AppConsole::~AppConsole()
    {
        clearLog();
        history.clear();
        items.clear();
    }

    void AppConsole::clearLog()
    {
        items.clear();
    }

    void AppConsole::execCommand(CommandLine line)
    {
        historyPos = -1;
        std::erase(history, std::string(line.command));
        history.emplace_back(line.command);
        if (const auto it = commandRegistry.find(line.command); it != commandRegistry.end())
        {
            const auto&[allowedFlags, action] = it->second;
            bool flagsValid = true;
            for (const auto& f : line.flags)
            {
                if (allowedFlags.contains(f))
                {
                    addLog("[E] Flag {} is not allowed for {}", f,line.command);
                    flagsValid = false;
                }
            }
            if (flagsValid)
                action(line);
        }
        else
            addLog("[E] Unknown command: {}", line.command);
        scrollToBottom = true;
    }

    i32 AppConsole::textEditCallbackStub(ImGuiInputTextCallbackData* data)
    {
        auto* console = static_cast<AppConsole*>(data->UserData);
        return console->textEditCallback(data);
    }

    i32 AppConsole::textEditCallback(ImGuiInputTextCallbackData* data)
    {
        switch (data->EventFlag)
        {
            case ImGuiInputTextFlags_CallbackCompletion:
            {
                const char* word_end = data->Buf + data->CursorPos;
                const char* word_start = word_end;
                while (word_start > data->Buf)
                {
                    const char c = word_start[-1];
                    if (c == ' ' || c == '\t' || c == ',' || c == ';')
                        break;
                    word_start--;
                }

                std::vector<std::string_view> candidates;
                std::string_view current_word(word_start, static_cast<size_t>(word_end - word_start));

                auto char_iequal = [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                };

                for (const auto &name: commandRegistry | std::views::keys)
                {
                    if (name.size() >= current_word.size() &&
                        std::equal(current_word.begin(), current_word.end(), name.begin(), char_iequal))
                    {
                        candidates.push_back(name);
                    }
                }

                if (candidates.empty())
                {
                    addLog("No match for {}", current_word);
                }
                else if (candidates.size() == 1)
                {
                    data->DeleteChars(static_cast<i32>(word_start - data->Buf), static_cast<i32>(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0].data());
                    data->InsertChars(data->CursorPos, " ");
                }
                else
                {
                    size_t match_len = current_word.size();
                    while (match_len < candidates[0].size())
                    {
                        const char expected_char = static_cast<char>(toupper(candidates[0][match_len]));
                        const bool is_match = std::all_of(candidates.begin() + 1, candidates.end(), [&](std::string_view cand) {
                            return match_len < cand.size() && static_cast<char>(toupper(cand[match_len])) == expected_char;
                        });

                        if (!is_match) break;

                        match_len++;
                    }

                    if (match_len > 0)
                    {
                        data->DeleteChars(static_cast<i32>(word_start - data->Buf), static_cast<int>(word_end - word_start));
                        data->InsertChars(data->CursorPos, candidates[0].data(), candidates[0].data() + match_len);
                    }
                    addLog("Commands:\n");
                    for (auto const& candidate : candidates)
                        addLog("{}", candidate);
                }
                break;
            }
            case ImGuiInputTextFlags_CallbackHistory:
            {
                const int prev_history_pos = historyPos;
                if (data->EventKey == ImGuiKey_UpArrow)
                {
                    if (historyPos == -1)
                        historyPos = static_cast<i32>(history.size()) - 1;
                    else if (historyPos > 0)
                        historyPos--;
                }
                else if (data->EventKey == ImGuiKey_DownArrow)
                {
                    if (historyPos != -1)
                        if (++historyPos >= static_cast<i32>(history.size()))
                            historyPos = -1;
                }

                if (prev_history_pos != historyPos)
                {
                    const char* historyStr = (historyPos >= 0) ? history[historyPos].c_str() : "";
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, historyStr);
                }
                break;
            }
            default: ;
        }
        return -1;
    }
}
