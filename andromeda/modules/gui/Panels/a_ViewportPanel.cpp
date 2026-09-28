#include "a_ViewportPanel.hpp"
#include "a_primitives.hpp"
#include "a_math.hpp"
#include "a_EditorContext.hpp"
#include "resource_manager.h"
#include "a_event_manager.hpp"
#include "a_EventTypes.hpp"
#include "a_logger.hpp"
#include "ImGuizmo.h"
namespace Andromeda::Gui {
    void ViewportPanel::initPanel(EditorContext& ctx)
    {
        m_SelectedEntity.registry = ctx.registry;
        EventManager::getInstance().AddEventListener<SceneObjectSelected>([this](const SceneObjectSelected& event) {
            m_SelectedEntity.id = event.entity;
            });

        std::array textureIDs = {
            ctx.resourceManager->getEditorIconID("translate"),
            ctx.resourceManager->getEditorIconID("scale"),
            ctx.resourceManager->getEditorIconID("rotate"),
            ctx.resourceManager->getEditorIconID("select")
        };

        for(i32 i = 0; i < textureIDs.size(); i++)
        {
            if (textureIDs[i] == 0)
            {
                A_ERROR("Viewport icon not found (index {})", i);
                return;
            }
            m_TextureHandles.handles[i] = static_cast<ImTextureID>(static_cast<intptr_t>(textureIDs[i]));
        }
    }
    
    void ViewportPanel::onGuiRender(EditorContext& ctx)
    {
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (ImGui::Begin("Viewport", nullptr, windowFlags))
        {
            handleViewportInput(*ctx.viewportDrawInfo);
            drawViewportImage(ctx, *ctx.viewportDrawInfo);

            if (m_SelectedEntity.id != ECS::INVALID_ENTITY_ID && m_SelectedEntity.registry != nullptr)
            {
                updateGizmos(ctx);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::updateGizmos(EditorContext& ctx)
    {
        if (!m_SelectedEntity.has<ECS::Component::Transform>()) return;

        // drawViewportImage() measures the rect this frame; without it ImGuizmo would
        // divide by a zero-sized viewport when mapping the mouse.
        if (m_ImageRect.size.x <= 0.0f || m_ImageRect.size.y <= 0.0f) return;

        auto& objectTransform = m_SelectedEntity.get<ECS::Component::Transform>();
        mat4 modelMatrix = objectTransform.modelMatrix();

        prepareImGuizmo();
        std::optional<ImGuizmo::OPERATION> operation = TransformIcons::getImGuizmoTool(m_ActiveTool);
        if (operation.has_value()) {
            ImGuizmo::Manipulate(
                glm::value_ptr(ctx.cameraData->viewMatrix),
                glm::value_ptr(ctx.cameraData->projection),
                operation.value(),
                ImGuizmo::WORLD,
                amath::glmValuePtr(modelMatrix)
            );
        }

        handleGizmoInteraction(objectTransform, modelMatrix);
    }

    void ViewportPanel::prepareImGuizmo()
    {
        // ImGuizmo maps the mouse into NDC through this rect, so it has to be the rect the
        // image really occupies on screen. ViewportDrawInfo::framebufferSize is unusable
        // here: it travels through EditorContext::layout and so lags a frame behind, and
        // the renderer resizes the FBO only after a delay (Renderer::processResizeTimer).
        // A rect that disagrees with the drawn image gives a skewed picking ray.
        ImGuizmo::SetRect(m_ImageRect.min.x, m_ImageRect.min.y, m_ImageRect.size.x, m_ImageRect.size.y);
        ImGuizmo::SetDrawlist();

        // On by default: an axis pointing away from the camera is drawn flipped, and it can
        // flip over mid-drag, which throws the object off in the opposite direction.
        ImGuizmo::AllowAxisFlip(false);
    }

    void ViewportPanel::handleGizmoInteraction(ECS::Component::Transform& transform, const mat4& modelMatrix)
    {
        if (ImGuizmo::IsUsing())
        {
            if (!m_IsDraggingGizmo) {
                m_IsDraggingGizmo = true;
                m_ActiveUndoState = transform;
            }

            applyGizmoTransform(transform, modelMatrix);
        }
        else if (m_IsDraggingGizmo)
        {
            finalizeGizmoInteraction(transform);
        }
    }

    void ViewportPanel::applyGizmoTransform(ECS::Component::Transform& transform, const mat4& matrix) const
    {
        // Each basis vector's length is that axis' scale; dividing it out leaves a pure rotation.
        const vec3 scale(glm::length(vec3(matrix[0])),
                         glm::length(vec3(matrix[1])),
                         glm::length(vec3(matrix[2])));

        // Only the field the active tool drives is written back. Feeding position, rotation
        // and scale back every frame round-tripped the rotation through ImGuizmo's Euler
        // angles, which its own header calls numerically unstable, and whose convention
        // differs from the one glm::quat(vec3) assumes. The error accumulated while dragging
        // until the Euler decomposition flipped - that was the jump.
        switch (m_ActiveTool)
        {
        case TransformIcons::Translate:
            transform.position = vec3(matrix[3]);
            break;

        case TransformIcons::Scale:
            transform.scale = scale;
            break;

        case TransformIcons::Rotate:
            // Taken off the normalized basis, so no Euler angles are involved at all.
            if (scale.x > 0.0f && scale.y > 0.0f && scale.z > 0.0f)
            {
                const mat3 basis(vec3(matrix[0]) / scale.x,
                                 vec3(matrix[1]) / scale.y,
                                 vec3(matrix[2]) / scale.z);
                transform.rotation = glm::normalize(glm::quat_cast(basis));
            }
            break;

        default:
            break;
        }
    }

    void ViewportPanel::finalizeGizmoInteraction(ECS::Component::Transform& currentTransform)
    {
        m_IsDraggingGizmo = false;
        auto oldTransform = std::any_cast<ECS::Component::Transform>(m_ActiveUndoState);

        if (currentTransform.position != oldTransform.position ||
            currentTransform.scale != oldTransform.scale ||
            currentTransform.rotation != oldTransform.rotation)
        {
            EventManager::getInstance().Dispatch(
                EventType::OnPushUndoTransform,
                PushUndoTransformEvent(m_SelectedEntity.id, m_ActiveUndoState)
            );
        }
    }

    void ViewportPanel::setOverlayStyle() {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    }

    [[nodiscard]]
    ImGuiWindowFlags ViewportPanel::setOverlayFlags() {
        constexpr ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove;
        return overlayFlags;
    }

    void ViewportPanel::resetOverlayStyle() {
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    void ViewportPanel::drawTransformButtons(const TransformIcons& textureHandles, const ImGuiWindowFlags flags)
    {
        constexpr float buttonSpacing = 15.0f;
        constexpr auto buttonSize = ImVec2(28, 28);

        constexpr float pillPaddingX = 8.0f;
        constexpr float btnSizeX = 28.0f;

        const float contentWidth = btnSizeX * static_cast<float>(textureHandles.Count)
            + buttonSpacing * static_cast<float>(textureHandles.Count - 1);

        const float totalPillWidth = contentWidth + (2.0f * pillPaddingX);
        constexpr float totalPillHeight = 42.0f;

        const ImVec2 startScreenPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec2 pillMin = startScreenPos;
        const auto pillMax = ImVec2(startScreenPos.x + totalPillWidth, startScreenPos.y + totalPillHeight);

        const ImU32 pillColor = ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 0.6f));

        constexpr float pillCornerRadius = totalPillHeight * 0.5f;

        drawList->AddRectFilled(pillMin, pillMax, pillColor, pillCornerRadius);


        ImGui::SetCursorScreenPos(ImVec2(startScreenPos.x + pillPaddingX, startScreenPos.y + 7.0f));
        for (u32 i = 0; i < textureHandles.Count; i++)
        {
            const ImTextureID texID = m_TextureHandles.handles[i];
            const ImVec2 currentBtnScreenPos = ImGui::GetCursorScreenPos();

            ImGui::PushID(i);

            ImGui::InvisibleButton(textureHandles.toolNames[i], buttonSize);

            const bool isHovered = ImGui::IsItemHovered();
            const bool isActive = ImGui::IsItemActive();

            if (isActive) {
                m_ActiveTool = static_cast<TransformIcons::Type>(i);
            }
            if (isHovered || isActive || m_ActiveTool == static_cast<TransformIcons::Type>(i)) {
                const auto btnCenter = ImVec2(currentBtnScreenPos.x + buttonSize.x * 0.5f, currentBtnScreenPos.y + buttonSize.y * 0.5f);

                constexpr float circleRadius = 28.0f * 0.5f + 4.0f;

                auto circleColorVec = ImVec4(0.25f, 0.25f, 0.25f, 0.7f);
                if (isActive) circleColorVec = ImVec4(0.4f, 0.4f, 0.4f, 0.8f);

                drawList->AddCircleFilled(btnCenter, circleRadius, ImGui::GetColorU32(circleColorVec));
            }

            ImGui::SetCursorScreenPos(currentBtnScreenPos);
            ImGui::Image(texID, buttonSize);

            ImGui::PopID();

            if (i < textureHandles.Count - 1) {
                ImGui::SetCursorScreenPos(ImVec2(currentBtnScreenPos.x + buttonSize.x + buttonSpacing, currentBtnScreenPos.y));
            }
        }
    }
    void ViewportPanel::drawWireframeControl(const u32& textureID) {
        const ImTextureID imguiID = static_cast<ImTextureID>(static_cast<intptr_t>(textureID));
        constexpr auto childSize = ImVec2(32, 32);

        const ImVec2 screenPos = ImGui::GetCursorScreenPos();
        const auto center = ImVec2(screenPos.x + childSize.x * 0.5f, screenPos.y + childSize.y * 0.5f);
        constexpr float radius = childSize.x * 0.5f + 6.0f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImGui::PushID("wireframe_bg");

        bool clicked = false;
        ImGui::InvisibleButton("wireframe_logic", ImVec2(childSize.x, childSize.y));
        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();
        if (ImGui::IsItemClicked()) clicked = true;

        auto color = ImVec4(0.1f, 0.1f, 0.1f, 0.6f);
        if (isActive || m_WireframeEnabled)      color = ImVec4(0.3f, 0.3f, 0.3f, 0.8f);
        else if (isHovered) color = ImVec4(0.25f, 0.25f, 0.25f, 0.7f);

        drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(color));

        ImGui::SetCursorScreenPos(screenPos);
        ImGui::Image(imguiID, childSize);

        ImGui::PopID();

        if (clicked) {
            m_WireframeEnabled = !m_WireframeEnabled;
            EventManager::getInstance().Dispatch(EventType::OnEnableWireframe,OnEnableWireFrame(m_WireframeEnabled));
        }
    }

    void ViewportPanel::drawViewportOverlay(const u32& textureID, const TransformIcons& textureHandles) {
        const ImVec2 startPos = ImGui::GetCursorStartPos();

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float childWidth = availableWidth;
        constexpr float childHeight = 55.0f;
        ImGui::SetCursorPos(ImVec2(startPos.x, startPos.y));

        const ImGuiWindowFlags overlayFlags = setOverlayFlags();
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::BeginChild("ToolbarOverlay", ImVec2(childWidth, childHeight), ImGuiChildFlags_None, overlayFlags))
        {
            constexpr float padding = 10.0f;
            setOverlayStyle();

            ImGui::SetCursorPos(ImVec2(padding, 11.0f));
            drawWireframeControl(textureID);

            constexpr float btnSize = 28.0f;
            constexpr float btnSpacing = 10.0f;
            constexpr float transformWidth = btnSize * 4.0f + btnSpacing * 3.0f;

            const float centerPosX = (childWidth - transformWidth) * 0.5f;

            ImGui::SetCursorPos(ImVec2(centerPosX, 13.5f));
            drawTransformButtons(textureHandles, overlayFlags);

            resetOverlayStyle();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::updateImGuiMousePos(const ViewportDimension& vpDimension, EditorContext& ctx, const ViewportDrawInfo& drawInfo) {
        auto* cam = drawInfo.camData;
        cam->viewportSize = vec2(vpDimension.size.x, vpDimension.size.y);
        cam->viewportPos = vec2(vpDimension.min.x, vpDimension.min.y);

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        ctx.state.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ctx.state.viewportHovered = hovered;
        cam->hasValidPickRay = hovered;

        if (hovered) {
            cam->imGuiMouseX = mousePos.x - vpDimension.min.x;
            cam->imGuiMouseY = mousePos.y - vpDimension.min.y;
            cam->framebufferSize = glm::vec2(vpDimension.size.x, vpDimension.size.y);
        }
        else {
            cam->imGuiMouseX = cam->imGuiMouseY = -1.0f;
        }
    }
    void ViewportPanel::drawViewportImage(EditorContext& ctx, const ViewportDrawInfo& drawInfo)
    {
        const ImVec2 currentSize = ImGui::GetContentRegionAvail();
        ctx.layout.viewportSize = vec2(currentSize.x, currentSize.y);

        ImGui::Image(drawInfo.postProcessingFboTexture.textureID,
            currentSize, ImVec2(0, 1), ImVec2(1, 0));

        const ImVec2 rectMin = ImGui::GetItemRectMin();
        const ImVec2 rectMax = ImGui::GetItemRectMax();

        const ViewportDimension vpDimension{
            rectMin,
            rectMax,
            currentSize
        };
        m_ImageRect = vpDimension;
        updateImGuiMousePos(vpDimension, ctx, drawInfo);
        const u32 id = ctx.resourceManager->getEditorIconID("box");

        drawViewportOverlay(id, m_TextureHandles);
    }
    void ViewportPanel::handleViewportInput(const ViewportDrawInfo& drawInfo)
    {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            drawInfo.camData->canRotate = true;
            ImGui::SetWindowFocus();
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            drawInfo.camData->canRotate = false;
        }
    }

}
