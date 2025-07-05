#include "SliceView.h"

#include "imgui.h"
#include <fmt/format.h>

#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "ContourExtractor.h"
#include "GLView.h"
#include "Widgets.h"

namespace gladius
{
    class GLView;
}

namespace gladius::ui
{
    void SliceView::show()
    {
        m_visible = true;
    }

    void SliceView::hide()
    {
        m_visible = false;
    }

    bool SliceView::isVisible() const
    {
        return m_visible;
    }

    bool SliceView::isHovered() const
    {
        return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && isVisible();
    }

    bool SliceView::render(gladius::ComputeCore & core, GLView & view)
    {
        if (!isVisible())
        {
            return false;
        }
        bool windowIsActuallyVisible = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0., 0.));
        if (ImGui::Begin("Slice", &m_visible, ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::BeginMenuBar())
            {

                // Always show menu bar
                if (ImGui::MenuItem(
                      reinterpret_cast<const char *>(ICON_FA_COMPRESS_ARROWS_ALT "\tCenter View")))
                {
                    centerView();
                }
                ImGui::EndMenuBar();
            }

            if (!m_hideDeveloperTools)
            {
                ImGui::Checkbox("Show normals ", &m_renderNormals);
                ImGui::SameLine();
                ImGui::Checkbox("All vertices ", &m_renderSourceVertices);
                ImGui::SameLine();
                ImGui::Checkbox("Jumps ", &m_showJumps);
            }
            if (m_distanceMeasurement.end.has_value() && m_distanceMeasurement.start.has_value())
            {
                ImGui::SameLine();
                ImGui::TextUnformatted(fmt::format("Distance: {} mm",
                                                   (m_distanceMeasurement.start.value() -
                                                    m_distanceMeasurement.end.value())
                                                     .norm())
                                         .c_str());
            }

            windowIsActuallyVisible = true;
            ImVec2 canvasStart =
              ImGui::GetCursorScreenPos(); // ImDrawList API uses screen coordinates!
            ImVec2 canvasSize = ImGui::GetContentRegionAvail(); // Resize canvas to what's available
            canvasSize.x = std::max(50.f, canvasSize.x);
            canvasSize.y = std::max(50.f, canvasSize.y);
            ImVec2 canvasEnd = ImVec2(canvasStart.x + canvasSize.x, canvasStart.y + canvasSize.y);

            // Store canvas size for use in centerView()
            m_canvasSize = canvasSize;

            // Draw border and background color
            ImGuiIO & io = ImGui::GetIO();
            ImDrawList * drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(canvasStart,
                                    canvasEnd,
                                    ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)));

            // This will catch our interactions
            ImGui::InvisibleButton("canvas",
                                   canvasSize,
                                   ImGuiButtonFlags_MouseButtonLeft |
                                     ImGuiButtonFlags_MouseButtonMiddle);

            const bool isActive = ImGui::IsItemActive();

            if (ImGui::IsMouseHoveringRect(canvasStart, canvasEnd))
            {
                m_zoomTarget += m_zoom * 0.1f * io.MouseWheel;
                m_zoomTarget = std::max(0.5f, m_zoomTarget);
            }
            if (fabs(m_zoom - m_zoomTarget) >= 0.01f)
            {
                auto prevPos = screenToWorldPos(io.MousePos);
                m_zoom += (m_zoomTarget - m_zoom) * std::min(1.f, 10.f * io.DeltaTime);
                m_zoom = std::max(0.5f, m_zoom);
                auto newPos = screenToWorldPos(io.MousePos);
                m_scrolling.x -= (prevPos.x - newPos.x) * m_zoom;
                m_scrolling.y += (prevPos.y - newPos.y) * m_zoom;
            }

            if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.))
            {
                m_scrolling.x += io.MouseDelta.x;
                m_scrolling.y += io.MouseDelta.y;
            }

            if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {

                auto const pos = screenToWorldPos(io.MousePos);
                if (m_distanceMeasurement.measurementInProgress)
                {
                    m_distanceMeasurement.end = {pos.x, pos.y};
                }
                else
                {
                    m_distanceMeasurement.start = {pos.x, pos.y};
                    m_distanceMeasurement.measurementInProgress = true;
                }
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                m_distanceMeasurement.measurementInProgress = false;
            }

            m_origin = {canvasStart.x + m_scrolling.x, canvasStart.y + m_scrolling.y};

            // Draw grid + all lines in the canvas
            drawList->PushClipRect(canvasStart, canvasEnd, true);

            const float gridDistance_mm = 10.;
            constexpr auto width_mm = 400.f;
            constexpr auto height_mm = 400.f;

            float x = 0.f;
            for (int i = 0; x < width_mm; ++i)
            {
                x = static_cast<float>(i) * gridDistance_mm;
                drawList->AddLine(worldToCanvasPos(Vector2{x, 0.f}),
                                  worldToCanvasPos(Vector2{x, height_mm}),
                                  IM_COL32(200, 200, 200, 40));
            }

            float y = 0.f;
            for (int i = 0; y < height_mm; ++i)
            {
                y = static_cast<float>(i) * gridDistance_mm;
                drawList->AddLine(worldToCanvasPos(Vector2{0.f, y}),
                                  worldToCanvasPos(Vector2{width_mm, y}),
                                  IM_COL32(200, 200, 200, 40));
            }

            drawList->AddRect(m_origin,
                              worldToCanvasPos(Vector2{width_mm, height_mm}),
                              IM_COL32(155, 155, 155, 255),
                              0.f,
                              ImDrawFlags_RoundCornersAll,
                              5.f);

            auto const drawLine = [&](Vector2 start,
                                      Vector2 end,
                                      ImGuiCol color = IM_COL32(55, 155, 255, 255),
                                      float width = 1.0f)
            {
                drawList->AddLine(worldToCanvasPos(Vector2{start.x(), start.y()}),
                                  worldToCanvasPos(Vector2{end.x(), end.y()}),
                                  color,
                                  width);
            };

            auto const drawVector = [&](Vector2 start,
                                        Vector2 end,
                                        ImGuiCol color = IM_COL32(55, 155, 255, 255),
                                        float width = 1.0f,
                                        float tipSize = 0.01f)
            {
                drawList->AddLine(worldToCanvasPos(start), worldToCanvasPos(end), color, width);

                auto const startToEnd = end - start;
                auto const tipBegin = end - startToEnd.normalized() * tipSize;
                Vector2 const normal = Vector2{startToEnd.y(), -startToEnd.x()}.normalized();

                drawList->AddTriangleFilled(worldToCanvasPos(end),
                                            worldToCanvasPos(tipBegin + normal * tipSize * 0.5f),
                                            worldToCanvasPos(tipBegin - normal * tipSize * 0.5f),
                                            color);
            };

            auto renderLines =
              [&](PolyLines const & lines, ImGuiCol color = IM_COL32(55, 155, 255, 255))
            {
                Vector2 prevPoint{};
                ImGuiCol const red = IM_COL32(255, 0, 0, 255);
                ImGuiCol const yellow = IM_COL32(255, 255, 55, 64);
                ImGuiCol const darkRed = IM_COL32(128, 0, 0, 64);
                ImGuiCol const greenish = IM_COL32(55, 255, 155, 255);

                // Reset bounding rect for new contour calculation
                m_contourBounds.reset();

                for (auto const & line : lines)
                {
                    if (line.vertices.size() < 4)
                    {
                        continue;
                    }

                    if (m_showJumps)
                    {
                        drawLine(prevPoint, line.vertices.front(), IM_COL32(155, 155, 55, 128));
                        drawList->AddCircleFilled(
                          worldToCanvasPos(line.vertices.front()), 5.f, yellow);
                    }
                    prevPoint = line.vertices.front();

                    // Update bounding rect with first vertex
                    m_contourBounds.expand(prevPoint);

                    for (auto iter = std::begin(line.vertices) + 1; iter != std::end(line.vertices);
                         ++iter)
                    {
                        auto actualColor = color;

                        // Update bounding rect with current vertex
                        m_contourBounds.expand(*iter);

                        if (line.area < 0.f)
                        {
                            actualColor = greenish;
                        }

                        if (m_showSelfIntersections && line.hasIntersections)
                        {
                            actualColor = yellow;
                        }

                        if (!line.isClosed)
                        {
                            actualColor = red;
                        }

                        if (line.contourMode == ContourMode::ExcludeFromSlice)
                        {
                            actualColor = darkRed;
                        }

                        // drawLine(prevPoint, *iter, actualColor);
                        drawVector(prevPoint, *iter, actualColor);
                        prevPoint = *iter;
                    }

                    if (m_showSelfIntersections)
                    {
                        for (auto const & selfIntersections : line.selfIntersections)
                        {
                            drawList->AddCircleFilled(
                              worldToCanvasPos(selfIntersections), 2.0f, IM_COL32(200, 0, 0, 255));
                        }
                    }
                }
            };

            if (!core.isSlicingInProgress())
            {
                auto sliceParameter = contourOnlyParameter();
                sliceParameter.zHeight_mm = core.getSliceHeight();
                if (core.requestContourUpdate(sliceParameter))
                {
                    m_contours.reset();

                    ImGui::End();
                    ImGui::PopStyleVar();
                    return windowIsActuallyVisible;
                }
            }

            if (!m_contours.has_value() && !core.isSlicingInProgress())
            {
                auto const & contourExtractor = core.getContour();
                std::lock_guard<std::mutex> lockContourExtractor(core.getContourExtractorMutex());
                m_contours = contourExtractor->getContour();

                // If we just got contours but they're empty, mark as empty
                if (m_contours.has_value() && m_contours->empty())
                {
                    m_contourWasEmpty = true;
                }
            }

            if (!core.isSlicingInProgress() && m_contours.has_value())
            {
                // Check if we should auto-center: contours were empty before and now we have
                // content
                bool const contourHasContent = !m_contours->empty();
                bool const shouldAutoCenter = m_contourWasEmpty && contourHasContent;

                renderLines(*m_contours);

                // Auto-center if transitioning from empty to having content
                if (shouldAutoCenter && m_contourBounds.isValid)
                {
                    centerView();
                }

                // Update the "was empty" state for next frame
                m_contourWasEmpty = !contourHasContent;

                if (m_renderNormals)
                {
                    auto const & normals = core.getContour()->getNormals();

                    ImGuiCol constexpr lightBlue IM_COL32(200, 200, 255, 255);

                    for (const auto & [normal, position, successor] : normals)
                    {
                        drawLine(position, position + 0.2f * normal, lightBlue, 1.f);
                    }
                }

                if (m_renderSourceVertices)
                {
                    auto const vertices = core.getContour()->getSourceVertices();
                    for (auto const & vertex : vertices)
                    {
                        ImGuiCol const lightGreen = IM_COL32(static_cast<char>(5.f * vertex.w),
                                                             static_cast<char>(20.f * vertex.w),
                                                             static_cast<char>(5.f * vertex.w),
                                                             255);
                        ImGuiCol constexpr lightRed = IM_COL32(250, 150, 150, 255);

                        drawList->AddCircleFilled(worldToCanvasPos(Vector2{vertex.x, vertex.y}),
                                                  vertex.w * vertex.w,
                                                  (vertex.z < FLT_MAX) ? lightGreen : lightRed);
                    }
                }

                if (m_distanceMeasurement.start.has_value() &&
                    m_distanceMeasurement.end.has_value())
                {
                    drawLine(m_distanceMeasurement.start.value(),
                             m_distanceMeasurement.end.value(),
                             IM_COL32(255, 255, 255, 128));
                }
            }
            drawList->PopClipRect();
        }

        auto const contentMin =
          ImVec2{ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x,
                 ImGui::GetWindowPos().y + ImGui::GetWindowContentRegionMin().y};

        auto const contentMax =
          ImVec2{ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x,
                 ImGui::GetWindowPos().y + ImGui::GetWindowContentRegionMax().y};

        auto const windowCenter =
          ImVec2(0.5f * (contentMin.x + contentMax.x), 0.5f * (contentMin.y + contentMax.y));

        ImGui::End();

        ImGui::PopStyleVar();
        if (core.isSlicingInProgress() || core.isAnyCompilationInProgress())
        {
            view.startAnimationMode();
            ImGuiWindowFlags window_flags =
              ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
              ImGuiWindowFlags_NoNav;
#ifdef IMGUI_HAS_DOCK
            window_flags |= ImGuiWindowFlags_NoDocking;
#endif
            bool open = true;

            ImGui::SetNextWindowBgAlpha(0.0f);

            ImGui::SetNextWindowPos({windowCenter.x - 30.f, windowCenter.y - 30.f});
            if (ImGui::Begin("SliceProgressIndicator", &open, window_flags))
            {

                ui::loadingIndicatorCircle(" ",
                                           30,
                                           ImVec4(1.0f, 0.0, 0.0f, 0.8f),
                                           ImVec4(1.0f, 1.0f, 1.0f, 0.5f),
                                           12,
                                           10.0f);
                ImGui::End();
            }
        }
        return windowIsActuallyVisible;
    }

    [[nodiscard]] ImVec2 SliceView::worldToCanvasPos(ImVec2 worldPos) const
    {
        return {(m_origin.x + worldPos.x * m_zoom), (m_origin.y - worldPos.y * m_zoom)};
    }

    [[nodiscard]] ImVec2 SliceView::worldToCanvasPos(gladius::Vector2 worldPos) const
    {
        return {(m_origin.x + worldPos.x() * m_zoom), (m_origin.y - worldPos.y() * m_zoom)};
    }

    [[nodiscard]] ImVec2 SliceView::screenToWorldPos(ImVec2 screenPos) const
    {
        return {((screenPos.x - m_origin.x) / m_zoom), ((-screenPos.y + m_origin.y) / m_zoom)};
    }

    void SliceView::zoomIn()
    {
        // Zoom in by 20%
        m_zoomTarget *= 1.2f;
        // Clamp to reasonable values
        m_zoomTarget = std::min(m_zoomTarget, 50.0f);
    }

    void SliceView::zoomOut()
    {
        // Zoom out by 20%
        m_zoomTarget *= 0.8f;
        // Clamp to reasonable values
        m_zoomTarget = std::max(m_zoomTarget, 0.5f);
    }

    void SliceView::resetView()
    {
        // Reset zoom and position
        m_zoomTarget = 4.0f;
        m_scrolling = {0.0f, 250.0f};
    }

    void SliceView::centerView()
    {
        // If bounds are not valid but we have contours, calculate bounds first
        if (!m_contourBounds.isValid && m_contours.has_value())
        {
            calculateContourBounds();
        }

        if (!m_contourBounds.isValid)
        {
            // No contour data available, fall back to reset view
            resetView();
            return;
        }

        // Calculate the center of the contour in world coordinates
        Vector2 const contourCenter = m_contourBounds.center();

        // Calculate required zoom to fit the contour with some padding
        float const paddingFactor = 1.2f; // 20% padding around the contour
        float const contourWidth = m_contourBounds.width();
        float const contourHeight = m_contourBounds.height();

        // Use actual canvas size
        float const canvasWidth = m_canvasSize.x;
        float const canvasHeight = m_canvasSize.y;

        // Calculate zoom to fit both width and height
        float const zoomForWidth =
          (contourWidth > 0.0f) ? canvasWidth / (contourWidth * paddingFactor) : 1.0f;
        float const zoomForHeight =
          (contourHeight > 0.0f) ? canvasHeight / (contourHeight * paddingFactor) : 1.0f;

        // Use the smaller zoom to ensure everything fits
        m_zoomTarget = std::min(zoomForWidth, zoomForHeight);
        m_zoomTarget = std::max(0.5f, std::min(m_zoomTarget, 50.0f)); // Clamp zoom

        // Set zoom immediately to avoid animation delay affecting the calculation
        m_zoom = m_zoomTarget;

        // Set scrolling to center the contour
        // We want the contour center to be at the canvas center
        // Canvas center in screen coordinates: (canvasWidth/2, canvasHeight/2)
        // World point to canvas conversion: canvas = origin + world * zoom
        // So: origin = canvas - world * zoom
        m_scrolling.x = canvasWidth * 0.5f - contourCenter.x() * m_zoom;
        m_scrolling.y = canvasHeight * 0.5f +
                        contourCenter.y() * m_zoom; // Note: Y is flipped in screen coordinates
    }

    void SliceView::calculateContourBounds()
    {
        m_contourBounds.reset();

        if (!m_contours.has_value())
        {
            return;
        }

        for (auto const & line : *m_contours)
        {
            if (line.vertices.size() < 4)
            {
                continue;
            }

            for (auto const & vertex : line.vertices)
            {
                m_contourBounds.expand(vertex);
            }
        }
    }
}
