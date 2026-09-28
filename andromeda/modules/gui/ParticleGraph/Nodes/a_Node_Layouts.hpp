#pragma once

/**
 * @file a_Node_Layouts.hpp
 * @brief Nodes that draw more than their fields: drawLayout() overloads picked up by NodeBuilder.
 */

#include <cmath>
#include "imgui.h"
#include "a_Nodes.hpp"
#include "a_Node_Fields.hpp"
#include <string>
#include "bmv080_telemetry.hpp"
#include <string_view>
#include "a_bindable_fields.hpp"
#include "generated_telemetry_meta.hpp"
namespace Andromeda::Gui::Node {

    inline constexpr float kTwoPi = 6.28318530718f;

    template <typename Fn>
    void drawCurvePreview(const float phase, Fn&& fn, const ImVec2 size = ImVec2(120.0f, 40.0f)) {
        ImGui::Dummy(size);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);
        
        const float midY = (min.y + max.y) * 0.5f;
        const float scaleY = size.y * 0.5f - 2.0f; // minus 2 pixels to avoid touching the top and bottom of the box

        draw->AddLine(ImVec2(min.x, midY), ImVec2(max.x, midY), ImGui::GetColorU32(ImGuiCol_Separator));

        constexpr i32 kSamples = 48;
        ImVec2 points[kSamples];
        for (i32 i = 0; i < kSamples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSamples - 1);
            points[i] = ImVec2(min.x + t * size.x, midY - fn(t * kTwoPi) * scaleY);
        }
        draw->AddPolyline(points, kSamples, PIN_ORANGE, ImDrawFlags_None, 1.5f);

        float wrapped = std::fmod(phase, kTwoPi);
        if (wrapped < 0.0f)
            wrapped += kTwoPi;
        draw->AddCircleFilled(ImVec2(min.x + wrapped / kTwoPi * size.x, midY - fn(wrapped) * scaleY), 3.0f,
                              ImGui::GetColorU32(ImGuiCol_Text));
    }

    // Builder is named instead of written as 'auto&': an auto parameter makes the function a template
    // with an unnamed parameter, and clangd reports those as shadowing each other.

    template<typename Builder>
    void drawLayout(Sin& node, Builder& builder) {
        builder.row(node.value, node.result);
        drawCurvePreview(node.value.value, [](float x) { return std::sin(x); });
    }

    template<typename Builder>
    void drawLayout(Cos& node, Builder& builder) {
        builder.row(node.value, node.result);
        drawCurvePreview(node.value.value, [](const float x) { return std::cos(x); });
    }
    // The channels a sensor node can read, split by output type and taken from the telemetry struct
    // itself. A new sensor field appears here without a change; a new sensor *type* does not yet, see
    // the TODO below.
    // TODO: one telemetry type today. When SensorTelemetry gains alternatives, list all of their fields.
    inline constexpr auto kSensorValueChannels = makeNumericFieldNames<BMV080Telemetry>();

    /**
     * @brief A button that switches to the next channel of @p channels at every click.
     * @details A real dropdown would need a popup, and a popup inside a node ends the editor's canvas
     *          window while the node's ImGui ID is still pushed, which trips an assertion. With a
     *          handful of channels, clicking through them is quick enough.
     */
    template<typename Channels>
    void drawChannelPicker(std::string& channel, const Channels& channels) {
        const std::string label = channel.empty() ? "select channel" : channel;
        constexpr float width = kParamWidth * 1.6f;

        if (ImGui::Button(label.c_str(), ImVec2(width, 0.0f)) && !channels.empty()) {
            const auto it = std::ranges::find(channels, channel);
            const size_t next =
                it == channels.end() ? 0 : (static_cast<size_t>(it - channels.begin()) + 1) % channels.size();
            channel = std::string(channels[next]);
        }
        // No tooltip: a tooltip is a window of its own, and inside a node it is placed in canvas
        // space, which drops it somewhere else entirely - it showed up over the hierarchy panel.
    }

    template<typename Builder>
    void drawLayout(Sensor& node, Builder& builder) {
        builder.row(node.value);
        drawChannelPicker(node.channel, kSensorValueChannels);
    }
} 