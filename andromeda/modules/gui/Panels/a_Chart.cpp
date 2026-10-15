#include "a_Chart.hpp"
#include "a_EditorContext.hpp"
#include <implot.h>
#include "a_event_manager.hpp"
namespace Andromeda::Gui {
    void Chart::initPanel(EditorContext& ctx)
    {
        m_SelectedEntity.registry = ctx.registry;
        EventManager::getInstance().AddEventListener<SceneObjectSelected>([this](const SceneObjectSelected& event) {
            m_SelectedEntity.id = event.entity;
            });
    }

    void Chart::onGuiRender(EditorContext& ctx) {
            // m_Name, not a separate literal: ImGui identifies windows by their title, so a
            // title that differs from the panel name leaves the Window menu toggling one thing
            // and the dock layout in a_Docking.cpp addressing another that never exists.
            // &m_IsOpen rather than nullptr gives the window its close button back and lets
            // ImGui clear the flag itself - without it the only way out was the menu.
            if (!ImGui::Begin(m_Name, &m_IsOpen)) {
                ImGui::End();
                return;
            }

            static ScrollingBuffer sdata(1000);
            static float t = 0;

            t += ImGui::GetIO().DeltaTime;

            float current_value = sinf(t * 2.0f);
            sdata.AddPoint(t, current_value);
            static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

            ImPlot::SetNextAxesToFit();

            if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, 300))) {
                ImPlot::SetupAxes("Zeit", "Wert", flags, flags);

                ImPlot::PlotLine("Live Signal", &sdata.Data[0].x, &sdata.Data[0].y,
                    sdata.Data.size(), 0, sdata.Offset, 2 * sizeof(float));

                ImPlot::EndPlot();
            }

            ImGui::End();
    }
}