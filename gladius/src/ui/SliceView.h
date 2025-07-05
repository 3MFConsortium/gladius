#pragma once

#include "compute/ComputeCore.h"

#include "Contour.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
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

        /**
         * @brief Center the view on the current contour and zoom to fit
         */
        void centerView();

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

        /// Current canvas size in pixels
        ImVec2 m_canvasSize = {800.0f, 600.0f};

        /// Bounding rectangle of the current contour in world coordinates
        struct BoundingRect
        {
            float minX = FLT_MAX;
            float minY = FLT_MAX;
            float maxX = -FLT_MAX;
            float maxY = -FLT_MAX;
            bool isValid = false;

            void reset()
            {
                minX = FLT_MAX;
                minY = FLT_MAX;
                maxX = -FLT_MAX;
                maxY = -FLT_MAX;
                isValid = false;
            }

            void expand(Vector2 const & point)
            {
                minX = std::min(minX, point.x());
                minY = std::min(minY, point.y());
                maxX = std::max(maxX, point.x());
                maxY = std::max(maxY, point.y());
                isValid = true;
            }

            float width() const
            {
                return maxX - minX;
            }
            float height() const
            {
                return maxY - minY;
            }
            Vector2 center() const
            {
                return Vector2{(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
            }
        };

        BoundingRect m_contourBounds;

        /// Track if contours were empty in the previous frame for auto-centering
        bool m_contourWasEmpty = true;

        [[nodiscard]] ImVec2 worldToCanvasPos(ImVec2 WorldPos) const;
        [[nodiscard]] ImVec2 worldToCanvasPos(gladius::Vector2 WorldPos) const;
        [[nodiscard]] ImVec2 screenToWorldPos(ImVec2 screenPos) const;

        /// Calculate bounding rectangle from current contour data
        void calculateContourBounds();
    };
}
