#include "a_BrowserPanel.hpp"
#include "a_EditorContext.hpp"
#include "a_ImGuiOperators.hpp"
#include "a_Style.hpp"
#include <cmath>
#include <string>
#include <cctype>
#include <vector>
#include "scene.hpp"
#include "a_FloatingSlider.hpp"
namespace Andromeda::Gui {

    namespace {
        // A rendered preview comes out of a framebuffer, whose first row is the bottom one, while an
        // icon loaded from a file starts at the top. Flipping V for the former draws both upright.
        ImVec2 tileUV0(const bool flipV) { return flipV ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f); }
        ImVec2 tileUV1(const bool flipV) { return flipV ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f); }

        /**
         * @brief Shortens a label to fit a width, ending it with an ellipsis.
         * @details The draw list ignores item bounds, so a name wider than its tile is drawn
         *          straight over the neighbouring tiles - visible as soon as the icon scale is
         *          turned down. Cutting it here keeps every label inside its own tile.
         * @param text The full label.
         * @param maxWidth Space available, in pixels.
         * @return The label unchanged when it fits, otherwise a shortened copy ending in "...".
         */
        std::string fitTextToWidth(const std::string& text, const float maxWidth) {
            if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
                return text;
            }

            constexpr const char* ellipsis = "...";
            const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;

            // Nothing sensible fits: three dots alone already overflow.
            if (ellipsisWidth > maxWidth) {
                return {};
            }

            size_t end = text.size();
            while (end > 0) {
                // Back off whole UTF-8 characters: a continuation byte (10xxxxxx) is the tail of
                // a multi-byte sequence, and cutting between the two halves of an umlaut would
                // leave a replacement glyph behind.
                do {
                    --end;
                } while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80);

                if (ImGui::CalcTextSize(text.c_str(), text.c_str() + end).x + ellipsisWidth <= maxWidth) {
                    break;
                }
            }

            return text.substr(0, end) + ellipsis;
        }

        /**
         * @brief Splits a label over at most two lines, shortening the second if it still overflows.
         *
         * @details Asset names are not prose, so linguistic hyphenation does not apply - the places
         *          a name wants to break are the ones its author put there. Tried in order:
         *          a space, an underscore or hyphen, a lowercase-to-uppercase step ("CustomModel"),
         *          a letter-to-digit step ("Room2"), and only failing all of those a hard cut at
         *          whatever character still fits. Candidates closest to the middle win, so the two
         *          lines come out roughly even instead of one word hanging off the end.
         *
         * @param text The full label.
         * @param maxWidth Width of one line, in pixels.
         * @return One or two lines, both guaranteed to fit @p maxWidth.
         */
        /** @brief True when byte @p i starts a character rather than continuing one. */
        bool isUtf8Start(const std::string& text, const size_t i) {
            return (static_cast<unsigned char>(text[i]) & 0xC0) != 0x80;
        }

        /**
         * @brief Scores how good a break before byte @p i would be.
         * @details Asset names are not prose, so linguistic hyphenation does not apply - the
         *          places a name wants to break are the ones its author put there.
         * @return 0 when the position is no break at all, higher is better.
         */
        i32 breakRank(const std::string& text, const size_t i) {
            const unsigned char prev = static_cast<unsigned char>(text[i - 1]);
            const unsigned char cur = static_cast<unsigned char>(text[i]);

            if (cur == ' ')                                 return 4;
            if (cur == '_' || cur == '-')                    return 3;
            if (std::islower(prev) && std::isupper(cur))     return 2;  // CustomModel
            if (std::isalpha(prev) && std::isdigit(cur))     return 1;  // Room2
            return 0;
        }

        /** @brief Where a label could be broken, both results 0 when not even one character fits. */
        struct BreakCandidates {
            size_t best = 0;        ///< Highest ranked position, closest to the middle on a tie.
            size_t lastFitting = 0; ///< Last position at which the first line still fits at all.
        };

        /**
         * @brief Finds the break positions for a label, in a single pass.
         * @details Both answers come out of the same walk because they share its expensive part -
         *          measuring the prefix - and its stop condition: once the first line no longer
         *          fits, no later position can either.
         * @param text The full label.
         * @param maxWidth Width of one line, in pixels.
         */
        BreakCandidates findBreakCandidates(const std::string& text, const float maxWidth) {
            BreakCandidates candidates;

            const float middle = static_cast<float>(text.size()) * 0.5f;
            i32 bestRank = 0;
            float bestDistance = 0.0f;

            for (size_t i = 1; i < text.size(); ++i) {
                if (!isUtf8Start(text, i)) {
                    continue;
                }
                if (ImGui::CalcTextSize(text.c_str(), text.c_str() + i).x > maxWidth) {
                    break;
                }
                candidates.lastFitting = i;

                const i32 rank = breakRank(text, i);
                if (rank == 0) {
                    continue;
                }

                // Nearest to the middle among equals, so the two lines come out roughly even
                // instead of one word hanging off the end.
                const float distance = std::fabs(static_cast<float>(i) - middle);
                if (rank > bestRank || (rank == bestRank && distance < bestDistance)) {
                    bestRank = rank;
                    bestDistance = distance;
                    candidates.best = i;
                }
            }

            return candidates;
        }

        /** @brief Drops the separator the break consumed, so neither line starts or ends on one. */
        void trimAroundBreak(std::string& first, std::string& second) {
            if (!second.empty() && (second.front() == ' ' || second.front() == '_' || second.front() == '-')) {
                second.erase(second.begin());
            }
            while (!first.empty() && first.back() == ' ') {
                first.pop_back();
            }
        }

        /**
         * @brief Splits a label over at most two lines, shortening the second if it still overflows.
         * @param text The full label.
         * @param maxWidth Width of one line, in pixels.
         * @return One or two lines, both guaranteed to fit @p maxWidth.
         */
        std::vector<std::string> layoutLabel(const std::string& text, const float maxWidth) {
            if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
                return { text };
            }

            const BreakCandidates candidates = findBreakCandidates(text, maxWidth);

            // A natural break if there is one, otherwise a hard cut at the last character that
            // still fits.
            const size_t splitAt = candidates.best != 0 ? candidates.best : candidates.lastFitting;
            if (splitAt == 0) {
                // Not even one character fits - nothing to split, let the ellipsis handle it.
                return { fitTextToWidth(text, maxWidth) };
            }

            std::string first = text.substr(0, splitAt);
            std::string second = text.substr(splitAt);
            trimAroundBreak(first, second);

            if (second.empty()) {
                return { std::move(first) };
            }
            return { std::move(first), fitTextToWidth(second, maxWidth) };
        }
    }

    void BrowserPanel::onGuiRender(EditorContext& ctx) {
        if (ImGui::Begin(m_Name, &m_IsOpen)) {
            renderSearch(ctx);
            ImGui::SameLine();
            Component::drawFloatingSlider("##iconScale", m_IconScale, 0.5f, 1.0f, 100.0f);
            renderGrid(ctx);
        }
        ImGui::End();
    }

    void BrowserPanel::renderSearch(EditorContext& ctx) {
        ImGui::InputTextWithHint("##search", "Search components...", m_SearchQuery, IM_ARRAYSIZE(m_SearchQuery));
    }

    void BrowserPanel::renderGrid(EditorContext& ctx) {
        const float availX = ImGui::GetContentRegionAvail().x;
        const float spacingX = ImGui::GetStyle().ItemSpacing.x;
        const ImVec2 iconSize = Tile::getIconSize(m_IconScale);

        int perRow = static_cast<int>(std::floor((availX + spacingX) / (iconSize.x + spacingX)));
        if (perRow < 1) perRow = 1;

        const uint32_t itemCount = ctx.modelProvider->getModelCount();
        bool anyTileClicked = false;
        int visibleCount = 0;

        for (int i = 0; i < static_cast<int>(itemCount); ++i) {
            const auto& device = ctx.modelProvider->getModelData(i);

            if (m_SearchQuery[0] != '\0' && device.name.find(m_SearchQuery) == std::string::npos)
                continue;

            if (handleTile(ctx, device, i, iconSize)) {
                anyTileClicked = true;
            }

            if ((visibleCount + 1) % perRow != 0 && i < (int)itemCount - 1) {
                ImGui::SameLine();
            }
            visibleCount++;
        }
        if (ImGui::IsMouseClicked(0) && !anyTileClicked && ImGui::IsWindowHovered()) {
            ctx.state.selectedIdx = -1;
        }
    }

    bool BrowserPanel::handleTile(EditorContext& ctx, const ModelRecord& device, int idx, ImVec2 iconSize) {
        ImGui::PushID(idx);
        ImGui::BeginGroup();

        u32 texID = ctx.modelProvider->getPreviewTextureID(device.meshID);
        const bool isPreview = texID != 0;
        if (!isPreview) {
            texID = ctx.modelProvider->getDeviceIconID(device.type);
        }
        Tile tile{ device, texID, idx, isPreview };
        // Two lines are reserved whether or not the name needs them: with a per-tile height the
        // grid rows would be as tall as their tallest tile, so a single long name would space out
        // the whole row. A few pixels of slack under short names is the cheaper trade.
        const float labelHeight = ImGui::GetTextLineHeight() * 2.0f;
        tile.pMin = ImGui::GetCursorScreenPos();
        tile.totalSize = { iconSize.x, iconSize.y + ImGui::GetStyle().ItemSpacing.y + labelHeight };
        tile.pMax = tile.pMin + ImVec2(iconSize.x, iconSize.y);

        ImGui::InvisibleButton("##tile_btn", tile.totalSize);

        tile.hovered = ImGui::IsItemHovered();
        tile.active = ImGui::IsItemActive();
        tile.clicked = ImGui::IsItemClicked(0);
        tile.dragged = ImGui::IsMouseDragging(0, 2.0f);
        tile.dragEnded = ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(0);
        tile.selected = (ctx.state.selectedIdx == idx);

        if (tile.clicked) ctx.state.selectedIdx = idx;

        drawTileVisuals(ImGui::GetWindowDrawList(), tile);
        handleDragAndDrop(ctx, tile, iconSize);

        ImGui::EndGroup();
        ImGui::PopID();

        return tile.clicked;
    }

    void BrowserPanel::drawTileVisuals(ImDrawList* dl, const Tile& tile) {
        if (tile.texID == 0) {
            dl->AddRectFilled(tile.pMin, tile.pMax, IM_COL32(45, 50, 70, 255), 4.0f);
        }
        else {
            dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tile.texID)),
              tile.pMin, tile.pMax, tileUV0(tile.flipV), tileUV1(tile.flipV));
        }

        if (tile.selected) {
            // Same accent as every other "selected" state in the editor - this used to be a
            // separately hand-typed (0,120,255), close to but not the same blue as the theme.
            dl->AddRect(tile.pMin, tile.pMax, ImGui::GetColorU32(AccentColor), 4.0f, 0, 2.5f);
            dl->AddRectFilled(tile.pMin, tile.pMax, ImGui::GetColorU32(withAlpha(AccentColor, 0.16f)), 4.0f);
        }
        else if (tile.hovered) {
            dl->AddRect(tile.pMin, tile.pMax, IM_COL32(255, 255, 255, 100), 4.0f);
        }

        const std::vector<std::string> lines = layoutLabel(tile.blueprint.name, tile.totalSize.x);
        const float lineHeight = ImGui::GetTextLineHeight();
        float textY = tile.pMax.y + ImGui::GetStyle().ItemSpacing.y;

        for (const std::string& line : lines) {
            const float textX = tile.pMin.x + (tile.totalSize.x - ImGui::CalcTextSize(line.c_str()).x) * 0.5f;
            dl->AddText({ textX, textY }, IM_COL32_WHITE, line.c_str());
            textY += lineHeight;
        }

        // Once a name is cut, the tooltip is the only place the whole of it is still readable.
        // Two lines that together spell the name out are not cut, so no tooltip for those.
        const bool truncated = lines.size() == 1 ? lines[0] != tile.blueprint.name
                                                 : lines[1].ends_with("...");
        if (tile.hovered && truncated) {
            ImGui::SetTooltip("%s", tile.blueprint.name.c_str());
        }
    }

    void BrowserPanel::handleDragAndDrop(EditorContext& ctx, const Tile& tile, ImVec2 iconSize) {
        if (tile.dragged && tile.active) {
            ctx.state.hasLastHitpoint = false;
            glm::vec3 hit;
            if (ctx.cameraData->hasValidPickRay && amath::RayIntersectsXZPlane(ctx.cameraData->cursorToWorldRay, 0.0f, hit)) {
                ctx.state.lastHitPoint = hit;
                ctx.state.hasLastHitpoint = true;
            }

            ImVec2 previewSize = iconSize * 0.8f;
            ImGui::GetForegroundDrawList()->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tile.texID)),
                ImGui::GetMousePos() - (previewSize * 0.5f),
                ImGui::GetMousePos() + (previewSize * 0.5f),
                tileUV0(tile.flipV), tileUV1(tile.flipV), IM_COL32(255, 255, 255, 150));
        }

        if (tile.dragEnded && ctx.cameraData->hasValidPickRay && ctx.state.hasLastHitpoint) {
            ECS::Component::Transform t;
            t.position = ctx.state.lastHitPoint;
            ctx.sceneManager->addEntity(tile.blueprint.meshID, tile.blueprint.name, t);
        }
    }
}