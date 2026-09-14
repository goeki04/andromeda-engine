#include "a_graphics_base.hpp"
#include "editor.hpp"
#include "renderer.h"
#include "window_manager.hpp"
#include "scene.hpp"
#include "resource_manager.h"
#include "a_subsystem_manager.hpp"
#include "a_guiTypes.hpp"
#include "a_registry.hpp"
#include <a_event_manager.hpp>
#include "input_manager.hpp"
namespace Andromeda::Editor {
    void Editor::initEditorContext()
    {
        assert(Window::m_GlContext && "OpenGL context is not initialized!");
        m_Renderer = Andromeda::SystemManager::getInstance().getSubsystem<Andromeda::Renderer>();
        assert(m_Renderer && "m_Renderer is nullptr in Editor::Start()");
        m_SceneManager = Andromeda::SystemManager::getInstance().getSubsystem<Andromeda::SceneManager>();
        assert(m_SceneManager && "m_SceneManager is nullptr in Editor::Start()");
        auto* rm = Andromeda::SystemManager::getInstance().getSubsystem<Andromeda::ResourceManager>();
        assert(rm && "rm is nullptr in Editor::Start()");
        m_EditorContext.cameraData = &m_SceneManager->m_EditorCamData;
        m_EditorContext.registry = &m_SceneManager->m_Registry;
        m_EditorContext.sceneManager = m_SceneManager;
        m_EditorContext.selection = &m_Selection;
        m_EditorContext.resourceManager = rm;
        m_EditorContext.windowContext.glslVersion = Andromeda::Renderer::glsl_version;
        m_EditorContext.windowContext.glContext = Window::m_GlContext;
        m_EditorContext.windowContext.window = Window::g_Window;
        m_EditorContext.modelProvider = rm;
        m_EditorContext.panelController = &m_GuiRenderer;
    }

    void Editor::start()
    {
        initEditorContext();
        m_GuiRenderer.init(m_EditorContext);
        m_MouseMotionListenerId = EventManager::getInstance().AddEventListener<MouseMoved>(
            [this](const MouseMoved& event) {
                if (m_SceneManager->m_EditorCamData.canRotate) {
                    float relX = event.xrel;
                    float relY = event.yrel;

                    auto& cam = m_SceneManager->m_EditorCamData;
                    cam.yaw += relX * cam.sensitivity;
                    cam.pitch -= relY * cam.sensitivity;
                    cam.pitch = std::clamp(cam.pitch, -89.0f, 89.0f);
                }
            }
        );
        m_PushUndoListenerId = EventManager::getInstance().AddEventListener<PushUndoTransformEvent>(
            [this](const PushUndoTransformEvent& event) {
                Undo::UndoTransformData undoData;
                undoData.entityID = event.entity;
                undoData.oldState = std::any_cast<ECS::Component::Transform>(event.oldState);

                m_UndoBuffer.pushCommand(Undo::CommandType::UpdateTransform, undoData);
            }
        );
        m_UndoListenerId = EventManager::getInstance().AddEventListener<KeyDown>([this](const KeyDown& keyEvent) {
            if (keyEvent.keycode == Keycode::Z) {
                bool isCtrlDown = InputSystem::isKeyHeld(Keycode::LeftControl) ||
                    InputSystem::isKeyHeld(Keycode::RightControl);

                if (isCtrlDown) {
                    if (m_SceneManager) {
                        m_UndoBuffer.undo(*m_EditorContext.registry);
                    }
                }
            }
        });
    }
    void Editor::update()
    {
        if (m_EditorContext.state.viewportFocused) {
            cameraMovement(m_SceneManager->m_EditorCamData);
        }
        vec2 viewportSize = m_EditorContext.layout.viewportSize;
        if (viewportSize.x > 0 && viewportSize.y > 0) {
            setProjectionMatrix(m_SceneManager->m_EditorCamData, viewportSize.x, viewportSize.y);
        }
        updatePickingRay(m_SceneManager->m_EditorCamData);
        editorPicking(&m_SceneManager->m_EditorCamData);

        m_VpDrawInfo.camData = &m_SceneManager->m_EditorCamData;
        m_VpDrawInfo.framebufferSize = viewportSize;
        m_VpDrawInfo.postProcessingFboTexture = m_Renderer->getFinalSceneViewportTexture();
        m_EditorContext.viewportDrawInfo = &m_VpDrawInfo;
        m_GuiRenderer.update(m_EditorContext);
    }

    void Editor::updateEvent(SDL_Event* event) {
        ImGui_ImplSDL3_ProcessEvent(event);
    }

    void Editor::destroy()
    {
        m_GuiRenderer.destroy();
        EventManager::getInstance().RemoveEventListener(m_UndoListenerId);
        EventManager::getInstance().RemoveEventListener(m_PushUndoListenerId);
        EventManager::getInstance().RemoveEventListener(m_MouseMotionListenerId);
    }

    bool Editor::RayIntersectAABB(const amath::CameraData& cam, const ECS::Component::AABB& aabb, const glm::mat4& modelMatrix)
    {
        const auto& [origin, direction] = cam.cursorToWorldRay;

        const mat4 invModel = glm::inverse(modelMatrix);

        auto o = glm::vec3(invModel * vec4(origin, 1.0f));
        vec3 d = glm::normalize(vec3(invModel * vec4(direction, 0.0f)));

        float tMin = 0.001f;
        float tMax = 1e30f;

        for (int i = 0; i < 3; ++i)
        {
            float oi = o[i];
            float di = d[i];

            if (std::fabs(di) < 1e-8f) {
                if (oi < aabb.min[i] || oi > aabb.max[i]) return false;
                continue;
            }

            float invD = 1.0f / di;
            float t0 = (aabb.min[i] - oi) * invD;
            float t1 = (aabb.max[i] - oi) * invD;
            if (t0 > t1) std::swap(t0, t1);

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMin > tMax) return false;
        }
        return true;
    }
    void Editor::editorPicking(const amath::CameraData* cam)
    {
        if (!m_EditorContext.state.viewportHovered) {
            return;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const auto& aabbPool = m_SceneManager->m_Registry.getPool<ECS::Component::AABB>();
            Entity hitEntity = ECS::INVALID_ENTITY_ID;
            for (const Entity e : aabbPool.getEntities()) {
                ECS::EntityHandle handle = { e,&m_SceneManager->m_Registry };
                if (!handle.has<ECS::Component::Transform>()) {
                    continue;
                }
                const mat4 modelMatrix = handle.get<ECS::Component::Transform>().modelMatrix();
                const auto& aabb = handle.get<ECS::Component::AABB>();

                if (RayIntersectAABB(*cam, aabb, modelMatrix)) {
                    hitEntity = e;
                    break;
                }
            }
            if (hitEntity != m_EditorContext.selection->getSelectedEntity()) {
                m_EditorContext.selection->setSelectedEntity(hitEntity);
                EventManager::getInstance().Dispatch(EventType::OnSceneObjectSelected, SceneObjectSelected(static_cast<u32>(hitEntity)));
            }
        }
    }
    amath::Ray Editor::cursorToWorldRay(const amath::CameraData& cam) {
        const float x = (2.0f * cam.imGuiMouseX) / cam.framebufferSize.x - 1.0f;
        const float y = 1.0f - (2.0f * cam.imGuiMouseY) / cam.framebufferSize.y;
        const vec4 rayClip(x, y, -1.0f, 1.0f);

        vec4 rayView = amath::inverse(cam.projection) * rayClip;
        rayView = vec4(rayView.x, rayView.y, -1.0f, 0.0f);
        const vec4 rayDir4 = amath::inverse(cam.viewMatrix) * rayView;
        amath::Ray ray{ vec3(0.0f),vec3(0.0f) };

        ray.direction = amath::normalize(vec3(rayDir4));
        ray.origin = cam.cameraPos;
        return ray;
    }
    //call this function after m_EditorCamData has been initialized
    void Editor::updateEditorCameraRay() const {
        if (m_SceneManager->m_EditorCamData.hasValidPickRay) {
            m_SceneManager->m_EditorCamData.cursorToWorldRay = cursorToWorldRay(m_SceneManager->m_EditorCamData);
        }
    }

    void Editor::cameraMovement(amath::CameraData& cam)
    {
        if (cam.canRotate == false) {
            return;
        }

        SDL_Window* currentWindow = SDL_GL_GetCurrentWindow();
        const Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
        const bool rightMouseDown = mouseState & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT);

        static bool wasRightMouseDown = false;

        if (rightMouseDown && !wasRightMouseDown) {
            SDL_GetMouseState(&cam.lastMouseX, &cam.lastMouseY);
            SDL_SetWindowRelativeMouseMode(currentWindow, true);
        }
        else if (!rightMouseDown && wasRightMouseDown) {
            SDL_WarpMouseInWindow(currentWindow, cam.lastMouseX, cam.lastMouseY);
            SDL_SetWindowRelativeMouseMode(currentWindow, false);
        }
        wasRightMouseDown = rightMouseDown;

        float radYaw = glm::radians(cam.yaw);
        float radPitch = glm::radians(cam.pitch);

        cam.forward.x = cos(radYaw) * cos(radPitch);
        cam.forward.y = sin(radPitch);
        cam.forward.z = sin(radYaw) * cos(radPitch);
        cam.forward = amath::normalize(cam.forward);

        cam.right = amath::normalize(amath::cross(cam.forward, glm::vec3(0.0f, 1.0f, 0.0f)));

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        glm::vec3 moveDir(0.0f);

        if (keyboard[SDL_SCANCODE_W]) moveDir += cam.forward;
        if (keyboard[SDL_SCANCODE_S]) moveDir -= cam.forward;
        if (keyboard[SDL_SCANCODE_A]) moveDir -= cam.right;
        if (keyboard[SDL_SCANCODE_D]) moveDir += cam.right;

        if (glm::length(moveDir) > 0.0f) {
            moveDir = glm::normalize(moveDir);
            cam.cameraPos += moveDir * cam.speed * SystemManager::s_deltaTime;
        }

        cam.target = cam.cameraPos + cam.forward;
        cam.viewMatrix = amath::lookAt(cam.cameraPos, cam.target, vec3(0.0f, 1.0f, 0.0f));
    }

    vec3 Editor::getCameraPos(const amath::CameraData& cam) {
        return cam.cameraPos;
    }

    mat4 Editor::getViewMatrix(const amath::CameraData& cam) {
        return cam.viewMatrix;
    }
    void Editor::updatePickingRay(amath::CameraData& cam)
    {
        const ImVec2 mouse = ImGui::GetMousePos();

        const float localX = mouse.x - cam.viewportPos.x;
        const float localY = mouse.y - cam.viewportPos.y;

        if (localX < 0 || localY < 0 || localX >= cam.viewportSize.x || localY >= cam.viewportSize.y) {
            cam.hasValidPickRay = false;
            return;
        }

        cam.hasValidPickRay = true;
        cam.imGuiMouseX = localX;
        cam.imGuiMouseY = localY;
        cam.framebufferSize = vec2(cam.viewportSize.x, cam.viewportSize.y);
        cam.cursorToWorldRay = cursorToWorldRay(cam);
    }

    void Editor::zoom(amath::CameraData& cam, const SDL_Event* event) {
        if (event->type == SDL_EVENT_MOUSE_WHEEL) {
            if (event->wheel.y > 0) {
                cam.fov = std::clamp(cam.fov + 1, cam.fovMin, cam.fovMax);
            }
            else if (event->wheel.y < 0) {
                cam.fov = std::clamp(cam.fov - 1, cam.fovMin, cam.fovMax);
            }
        }
        setProjectionMatrix(cam, cam.viewportSize.x, cam.viewportSize.y);
    }

    void Editor::setProjectionMatrix(amath::CameraData& cam, float viewportSizeX, float viewportSizeY) {
        const float aspect = (viewportSizeY > 0) ? (viewportSizeX / viewportSizeY) : 1.0f;
        cam.projection = amath::perspective(amath::radians(cam.fov), aspect, 0.1f, 100.0f);
    }

    mat4 Editor::getProjectionMatrix(const amath::CameraData& cam) {
        return cam.projection;
    }

    mat4 Editor::calculateCameraOrbit(amath::CameraData& cam)
    {
        cam.cameraPos = vec3(2, 5, 0);
        return amath::lookAt(cam.cameraPos, vec3(0.0f, 0.0f, 0.0f), cam.up);
    }
}