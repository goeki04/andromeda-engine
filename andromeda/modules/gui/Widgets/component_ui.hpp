#pragma once

/**
 * @file component_ui.hpp
 * @brief Per-component ImGui editor widgets used by the Inspector (DetailsPanel).
 *
 * @details Provides @c drawComponentUI<T>(), a function template specialized for each
 *          editable component type (Transform, Tag, Material, ...). Edits that should be
 *          undoable snapshot the component into a @c std::any before the change and
 *          dispatch an undo event once the edit is committed. @c renderEntityComponentUI()
 *          ties this together by iterating over all supported component types on an entity.
 */

#include "a_registry.hpp"
#include "imgui.h"
#include <string_view>
#include <any>
#include "resource_manager.h"
#include "a_event_manager.hpp"
#include "a_subsystem_manager.hpp"
#include "a_shader_generated.hpp"
#include "a_particle_component_ui.hpp"
/**
 * @namespace Andromeda::Gui::Component
 * @brief ImGui drawing routines that render the editable inspector UI for ECS components.
 */
namespace Andromeda::Gui::Component
{
    /**
     * @brief Primary template for rendering a component's inspector UI.
     * @details The unspecialized version intentionally does nothing; each editable
     *          component provides its own explicit specialization.
     * @tparam T The component type to draw a UI for.
     * @param handle Handle to the entity that owns the component.
     * @param undoState Scratch storage used to snapshot the component before an undoable edit.
     */
    template<typename T>
    inline void drawComponentUI(ECS::EntityHandle handle, std::any& undoState){
    }

    /**
     * @brief Inspector UI for the @c Transform component: position, rotation (degrees) and scale.
     * @details Rotation is presented in degrees for readability and converted back to a quaternion
     *          on edit. Each field snapshots the transform on activation and dispatches an
     *          @c OnPushUndoTransform event when the edit completes.
     * @param handle Handle to the entity that owns the Transform.
     * @param undoState Storage for the pre-edit Transform snapshot.
     */
    template<>
    inline void drawComponentUI<ECS::Component::Transform>(ECS::EntityHandle handle, std::any& undoState)
    {
            auto& transform = handle.get<ECS::Component::Transform>();
            auto& position = transform.position;
            auto& scale = transform.scale;
            ImGui::PushID("Transform");
            ImGui::Text("Position");
            ImGui::SameLine(100);
            ImGui::DragFloat3("##pos", &position.x, 0.1f);

            if (ImGui::IsItemActivated()) {
                undoState = transform;
            }

            if (ImGui::IsItemDeactivatedAfterEdit()) {
                EventManager::getInstance().Dispatch(EventType::OnPushUndoTransform,
                    PushUndoTransformEvent(handle.id, undoState));
            }
            ImGui::Text("Rotation");
            ImGui::SameLine(100);
            vec3 rotationRad = amath::eulerAngles(transform.rotation);
            vec3 rotationDeg = amath::degrees(rotationRad);
            if (ImGui::DragFloat3("##rot", &rotationDeg.x, 0.1f)) {
                transform.rotation = quat(amath::radians(rotationDeg));
            }

            if (ImGui::IsItemActivated()) undoState = transform;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                EventManager::getInstance().Dispatch(EventType::OnPushUndoTransform,
                    PushUndoTransformEvent(handle.id, undoState));
            }

            ImGui::Text("Scale");
            ImGui::SameLine(100);
            ImGui::DragFloat3("##scale", &scale.x, 0.1f);
            if (ImGui::IsItemActivated()) undoState = transform;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                EventManager::getInstance().Dispatch(EventType::OnPushUndoTransform,
                    PushUndoTransformEvent(handle.id, undoState));
            }
            ImGui::PopID();
            ImGui::Spacing();
    }

    /**
     * @brief Inspector UI for the @c Tag component: an editable text field for the entity's name.
     * @param handle Handle to the entity that owns the Tag.
     * @param undoState Unused for this component, present to satisfy the common signature.
     */
    template<>
    inline void drawComponentUI<ECS::Component::Tag>(ECS::EntityHandle handle, std::any& undoState){
        auto& tag = handle.get<ECS::Component::Tag>();

        ImGui::PushID("TagComponent");
        ImGui::Text("Name");
        ImGui::SameLine(100);
        char buffer[256];
        std::memset(buffer, 0, sizeof(buffer));
        std::strncpy(buffer, tag.name.c_str(), sizeof(buffer) - 1);

        if (ImGui::InputText("##tag_name", buffer, sizeof(buffer)))
        {
            tag.name = std::string(buffer);
        }

        ImGui::PopID();
        ImGui::Spacing();
    }

    /**
     * @brief Inspector UI for the @c Material component: reflects the material's UBO fields into editable widgets.
     * @details Looks up the material via the @c ResourceManager and, if it exposes a UBO structure,
     *          uses the generated @c reflect() visitor to emit a widget per field (drag, slider or
     *          color picker depending on type and metadata). Matrix fields are skipped. Changed values
     *          are pushed back to the GPU via @c updateUBOData().
     * @param handle Handle to the entity that owns the Material.
     * @param undoState Storage for the pre-edit Material snapshot.
     */
    template<>
    inline void drawComponentUI<ECS::Component::Material>(ECS::EntityHandle handle, std::any& undoState) {
        ResourceManager* rm = SystemManager::getInstance().getSubsystem<ResourceManager>();
        auto& matComp = handle.get<ECS::Component::Material>();

        auto material = rm->getMaterial(matComp.materialName);
        if (!material || !material->m_UboStructure.has_value()) {
            ImGui::TextDisabled("Keine editierbaren UBO-Daten für dieses Material.");
            return;
        }

        ImGui::PushID("MaterialComponent");

        bool valueChanged = false;

        auto imGuiVisitor = [&](const char* name, auto& value, ShaderDataType type, float min = 0.0f, float max = 0.0f, const char* widget = "Default") {
            std::string labelId = std::string("##ubo_") + name;

            if (type == ShaderDataType::Mat3 || type == ShaderDataType::Mat4) return;

            ImGui::Text("%s", name);
            ImGui::SameLine(100);

            using T = std::decay_t<decltype(value)>;

            bool useSlider = (min != max);
            bool isColor = (std::string_view(widget) == "Color");
            if constexpr (std::is_same_v<T, float>) {
                if (useSlider) {
                    if (ImGui::SliderFloat(labelId.c_str(), &value, min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp)) valueChanged = true;
                }
                else {
                    if (ImGui::DragFloat(labelId.c_str(), &value, 0.05f)) valueChanged = true;
                }
            }
            else if constexpr (std::is_same_v<T, vec2>) {
                if (useSlider) {
                    if (ImGui::SliderFloat2(labelId.c_str(), &value.x, min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp)) valueChanged = true;
                }
                else {
                    if (ImGui::DragFloat2(labelId.c_str(), &value.x, 0.05f)) valueChanged = true;
                }
            }
            else if constexpr (std::is_same_v<T, vec3>) {
                if (isColor) {
                    if (ImGui::ColorEdit3(labelId.c_str(), &value.x)) valueChanged = true;
                }
                else if (useSlider) {
                    if (ImGui::SliderFloat3(labelId.c_str(), &value.x, min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp)) valueChanged = true;
                }
                else {
                    if (ImGui::DragFloat3(labelId.c_str(), &value.x, 0.05f)) valueChanged = true;
                }
            }
            else if constexpr (std::is_same_v<T, vec4>) {
                if (useSlider) {
                    if (ImGui::SliderFloat4(labelId.c_str(), &value.x, min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp)) valueChanged = true;
                }
                else {
                    if (ImGui::DragFloat4(labelId.c_str(), &value.x, 0.05f)) valueChanged = true;
                }
            }
            else if constexpr (std::is_same_v<T, i32>) {
                if (useSlider) {
                    if (ImGui::SliderInt(labelId.c_str(), &value, static_cast<int>(min), static_cast<int>(max), "%d", ImGuiSliderFlags_AlwaysClamp)) valueChanged = true;
                }
                else {
                    if (ImGui::InputInt(labelId.c_str(), &value)) valueChanged = true;
                }
            }

            if (ImGui::IsItemActivated()) {
                undoState = matComp;
            }
            };

        if (matComp.materialName == "PBRMaterial") {
            auto& pbrData = std::any_cast<Generated::pbrMaterial&>(material->m_UboStructure);
            pbrData.reflect(imGuiVisitor);
            if (valueChanged) material->updateUBOData(pbrData);
        }

        ImGui::PopID();
        ImGui::Spacing();
    }

    template<>
    inline void drawComponentUI<ECS::Component::ParticleSystem>(ECS::EntityHandle handle, std::any& undoState) {
        auto& particleComp = handle.get<ECS::Component::ParticleSystem>();

        ImGui::Checkbox("Enable Particle Groups", &particleComp.useParticleGroups);
        if (particleComp.useParticleGroups) {
            drawParticleGroups(particleComp);
        } else {
            drawParticleGroupProperties(particleComp.getParticleGroups(), 0);
        }
    }

    /**
     * @brief Renders the inspector UI for every supported component attached to an entity.
     * @details Iterates a compile-time list of allowed component types; for each type present on
     *          the entity it emits a collapsing header (using the demangled type name) and calls
     *          the matching @c drawComponentUI<T>() specialization.
     * @param handle Handle to the entity to inspect.
     * @param rm The resource manager, forwarded for components that need resource lookups.
     * @param undoState Scratch storage for undo snapshots, shared across the drawn components.
     */
    inline void renderEntityComponentUI(const ECS::EntityHandle handle, ResourceManager* rm, std::any& undoState)
    {
        using AllowedComponents = std::tuple<
            ECS::Component::Tag,        
            ECS::Component::Transform,  
            ECS::Component::Material,
            ECS::Component::ParticleSystem
        >;

        std::apply([&]<typename... T>(T&&... args)
        {
            ([&]() {
                if (handle.registry->hasComponent<T>(handle.id))
                {
                    std::string componentName = typeid(T).name();
                    size_t lastScope = componentName.find_last_of("::");
                    if (lastScope != std::string::npos) {
                        componentName = componentName.substr(lastScope + 1);
                    }

                    if (ImGui::CollapsingHeader(componentName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        drawComponentUI<T>(handle, undoState);
                    }
                }
                }(), ...);
        }, AllowedComponents{});
    }
}
