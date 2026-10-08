#pragma once
#include "a_EditorPanel.hpp"
#include "a_primitives.hpp"
#include "a_model_record.hpp"
#include "imgui.h"

namespace Andromeda::Gui {
    struct EditorContext;

    /**
     * @class BrowserPanel
     * @brief A UI panel that allows users to browse, search, and instantiate devices.
     * * The BrowserPanel displays a grid of available devices as interactive tiles.
     * It supports real-time filtering via a search query and features drag-and-drop
     * functionality to place devices directly into the 3D scene.
     */
    class BrowserPanel : public EditorPanel {
    public:
        /**
         * @brief Constructs a new Browser Panel.
         * @param name The display name of the panel window.
         */
        explicit BrowserPanel(const char* name) : EditorPanel(name) {}

        /**
         * @brief Main render loop for the panel.
         * @param ctx Reference to the centralized editor context containing state and providers.
         */
        void onGuiRender(EditorContext& ctx) override;

    private:
        /**
         * @struct Tile
         * @brief Internal data structure representing a single device entry in the browser grid.
         * * This structure encapsulates all visual, spatial, and interaction states for a
         * specific device tile during a single frame.
         */
        struct Tile {
            const ModelRecord& blueprint;   ///< Reference to the underlying device data.
            u32 texID;         ///< GPU texture handle of the image to draw: a rendered model preview, or the device icon as a fallback.
            i32 index;              ///< Unique index of the device in the provider list.

            /**
             * @brief Whether the texture has to be drawn with its V axis flipped.
             *
             * @details True for a rendered preview and false for a device icon, because the two
             *          arrive with opposite row order: a render target has its origin at the
             *          bottom left, while an icon loaded from an image file starts at the top left.
             *          Drawing both with the same UVs leaves one of them upside down.
             */
            bool flipV = false;

            bool hovered = false;   ///< Is the mouse currently over this tile?
            bool active = false;    ///< Is the mouse button currently held down on this tile?
            bool clicked = false;   ///< Was this tile clicked during the current frame?
            bool dragged = false;   ///< Is the user currently dragging this tile?
            bool dragEnded = false; ///< Was the drag operation released this frame?
            bool selected = false;  ///< Is this device currently selected in the editor?

            ImVec2 pMin;            ///< Top-left screen coordinate of the icon.
            ImVec2 pMax;            ///< Bottom-right screen coordinate of the icon.
            ImVec2 totalSize;       ///< Combined size of the icon and the text label below it.

            /**
             * @brief Calculates the standard icon size based on current ImGui frame height.
             * @return A scaled ImVec2 representing the icon dimensions.
             */
            static ImVec2 getIconSize() {
                float h = ImGui::GetFrameHeight();
                return { h * 2.5f, h * 2.5f };
            }
        };

        /**
         * @brief Renders the search input field at the top of the panel.
         * @param ctx Reference to the editor context.
         */
        void renderSearch(EditorContext& ctx);

        /**
         * @brief Manages the grid layout and iterates through available devices.
         * @param ctx Reference to the editor context.
         */
        void renderGrid(EditorContext& ctx);

        /**
         * @brief Processes interaction and state for an individual device tile.
         * @param ctx Reference to the editor context.
         * @param device The device record to process.
         * @param idx The index of the device.
         * @return True if the tile was clicked, false otherwise.
         */
        static bool handleTile(EditorContext& ctx, const ModelRecord& device, int idx);

        /**
         * @brief Handles 3D world interaction for dragging devices into the scene.
         * @param ctx Reference to the editor context.
         * @param tile The tile being dragged.
         */
        static void handleDragAndDrop(EditorContext& ctx, const Tile& tile);

        /**
         * @brief Performs low-level ImGui drawing for the tile's icon, text, and highlights.
         * @param dl The draw list of the current window.
         * @param tile The pre-calculated tile data to render.
         */
        static void drawTileVisuals(ImDrawList* dl, const Tile& tile);

        char m_SearchQuery[128] = ""; ///< Buffer for the search input text.
    };
}