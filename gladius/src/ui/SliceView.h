#pragma once

#include "compute/ComputeCore.h"
 
#include "imgui.h"
#include "Contour.h"
#include <optional>

namespace gladius
{
    class GLView;
}

namespace gladius::ui
{

    struct DistanceMeasurement
    {
        std::optional<Vector2> start;
        std::optional<Vector2> end;
        bool measurementInProgress = false;
    };

    class SliceView
    {
      public:
        void show();
        void hide();
        bool isVisible() const;
        /// \returns Returns true, if the window was rendered
        [[nodiscard]] bool render(ComputeCore & core, GLView & view);

        /**
         * @brief Check if mouse is hovering over the slice view
         * @return true if the slice view is being hovered
         */
        bool isHovered() const;

        /**
         * @brief Zoom in the slice view
         */
        void zoomIn();
        
        /**
         * @brief Zoom out the slice view
         */
        void zoomOut();
        
        /**
         * @brief Reset the slice view to default position and zoom
         */
        void resetView();

      private:
        bool m_visible{false};

        float m_zoom = 4.f;
        float m_zoomTarget = 4.f;
        ImVec2 m_scrolling = {0.0f, 250.0f};
        ImVec2 m_origin = {};

        DistanceMeasurement m_distanceMeasurement;

        bool m_renderNormals = false;
        bool m_renderSourceVertices = false;
        bool m_showJumps = false;
        bool m_showSelfIntersections = false;
        bool m_hideDeveloperTools = true;

        std::optional<PolyLines> m_contours;

        [[nodiscard]] ImVec2 worldToCanvasPos(ImVec2 WorldPos) const;
        [[nodiscard]] ImVec2 worldToCanvasPos(gladius::Vector2 WorldPos) const;
        [[nodiscard]] ImVec2 screenToWorldPos(ImVec2 screenPos) const;
    };
}
