#include "RenderWindow.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "../CLMath.h"
#include "../ComputeContext.h"
#include "../ConfigManager.h"
#include "../ContourExtractor.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "../ImageRGBA.h"
#include "../TimeMeasurement.h"
#include "../io/MeshExporter.h"
#include "GLView.h"
#include "Profiling.h"
#include "ShortcutManager.h"
#include "Widgets.h"
#include "compute/ComputeCore.h"
#include "imgui.h"
#include <nodes/Model.h>

namespace gladius::ui
{
    using namespace std;

    void RenderWindow::initialize(ComputeCore * core,
                                  GLView * view,
                                  std::shared_ptr<ShortcutManager> shortcutManager,
                                  gladius::ConfigManager * configManager)
    {
        m_core = core;
        m_view = view;
        m_shortcutManager = shortcutManager;
        m_configManager = configManager;
        
        auto & settings = m_core->getResourceContext()->getRenderingSettings();
        m_renderWindowState.renderQuality = settings.quality;
        m_renderWindowState.renderQualityWhileMoving = settings.quality * 0.5f;
        
        // Load permanent centering state from config
        if (m_configManager)
        {
            m_permanentCenteringEnabled = m_configManager->getValue<bool>(
                "renderWindow", "permanentCenteringEnabled", false);
        }
    }

    void RenderWindow::renderWindow()
    {
        ProfileFunction;
        if (!m_isVisible)
        {
            return;
        }

        // only render if can get a compute token
        auto token = m_core->requestComputeToken();
        if (token)
        {
            render(m_renderWindowState);
        }

        auto const img = m_core->getResultImage();

        auto const textureId = img->GetTextureId();
        auto & io = ImGui::GetIO();
        ImGuiWindowFlags const window_flags =
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar;
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("Preview", &m_isVisible, window_flags);

        // Cache window state for isHovered() and isFocused() methods
        m_isWindowHovered = ImGui::IsWindowHovered();
        m_isWindowFocused = ImGui::IsWindowFocused();

        // if has focus, but not any item has focus, handle key input and content area is hovered
        if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemFocused() &&
            ImGui::IsMouseHoveringRect(m_contentAreaMin, m_contentAreaMax))
        {
            handleKeyInput();
        }

        if (ImGui::BeginMenuBar())
        {

            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>(ICON_FA_COMPRESS_ARROWS_ALT "\tCenter View")))
            {
                if (m_core->updateBBox() && m_core->getBoundingBox().has_value())
                {
                    centerView();
                }
            }

            // Toggle for permanent centering
            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>(ICON_FA_CROSSHAIRS "\tPermanent Centering"),
                  nullptr,
                  m_permanentCenteringEnabled))
            {
                togglePermanentCentering();
            }

            if (ImGui::IsItemHovered())
            {
                std::string shortcutText = "No shortcut assigned";
                if (m_shortcutManager)
                {
                    auto shortcut =
                      m_shortcutManager->getShortcut("camera.togglePermanentCentering");
                    if (!shortcut.isEmpty())
                    {
                        shortcutText = shortcut.toString();
                    }
                }

                ImGui::SetTooltip("Automatically center view when model changes, camera moves, or "
                                  "viewport resizes\nShortcut: %s",
                                  shortcutText.c_str());
            }

            toggleButton({reinterpret_cast<const char *>(ICON_FA_ROBOT "\tHQ")},
                         &m_enableHQRendering);

            int renderingFlags = m_core->getResourceContext()->getRenderingSettings().flags;

            // submenu for rendering flags
            bool flagsChanged = false;
            if (ImGui::BeginMenu("..."))
            {
                flagsChanged |=
                  ImGui::CheckboxFlags("Show Build Plate", &renderingFlags, RF_SHOW_BUILDPLATE);
                flagsChanged |=
                  ImGui::CheckboxFlags("Cut Off Object", &renderingFlags, RF_CUT_OFF_OBJECT);
                flagsChanged |= ImGui::CheckboxFlags("Show Field", &renderingFlags, RF_SHOW_FIELD);
                flagsChanged |= ImGui::CheckboxFlags("Show Stack", &renderingFlags, RF_SHOW_STACK);
                flagsChanged |= ImGui::CheckboxFlags(
                  "Show Coordinate System", &renderingFlags, RF_SHOW_COORDINATE_SYSTEM);

                ImGui::Separator(); // Quality slider
                float quality = m_core->getResourceContext()->getRenderingSettings().quality;
                ImGui::SetNextItemWidth(150.f * m_uiScale);
                bool qualityChanged = ImGui::SliderFloat("Quality", &quality, 0.1f, 2.0f);

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Rendering quality (0.1 = Fast, 2.0 = Highest Quality)");
                }
                if (qualityChanged)
                {
                    m_core->getResourceContext()->getRenderingSettings().quality = quality;
                    m_renderWindowState.renderQuality = quality;
                    m_renderWindowState.renderQualityWhileMoving = quality * 0.5f;
                    invalidateView();
                }
                m_renderWindowState.renderQuality = quality;

                ImGui::EndMenu();
            }

            if (flagsChanged)
            {
                invalidateView();
            }

            m_core->getResourceContext()->getRenderingSettings().flags = renderingFlags;

            if (m_core->isAnyCompilationInProgress())
            {
                ImGui::TextUnformatted("Compilation in progress");
            }
            else
            {
                auto const bb = m_core->getBoundingBox();
                if (bb.has_value())
                {
                    ImGui::TextUnformatted(fmt::format("{:.3f} mm x {:.3f} mm x {:.3f} mm",
                                                       bb->max.x - bb->min.x,
                                                       bb->max.y - bb->min.y,
                                                       bb->max.z - bb->min.z)
                                             .c_str());

                    ImGui::SameLine();

                    auto const range = 20000.f;
                    bool const bboxValuesInRange =
                      fabs(bb->max.x) < range && fabs(bb->max.y) < range &&
                      fabs(bb->max.z) < range && fabs(bb->min.x) < range &&
                      fabs(bb->min.y) < range && fabs(bb->min.z) < range;

                    if ((!isinf(bb->max.x) && !isinf(bb->max.y) && !isinf(bb->max.z) &&
                         !isinf(bb->min.x) && !isinf(bb->min.y) && !isinf(bb->min.z)) &&
                        bboxValuesInRange)
                    {

                        ImGui::TextUnformatted(
                          fmt::format(
                            "(min = x:{:.3f} y:{:.3f} z:{:.3f} ", bb->min.x, bb->min.y, bb->min.z)
                            .c_str());

                        ImGui::SameLine();

                        ImGui::TextUnformatted(
                          fmt::format(
                            "max = x:{:.3f} y:{:.3f} z:{:.3f})", bb->max.x, bb->max.y, bb->max.z)
                            .c_str());
                    }
                }
            }

            auto const contentWidth =
              ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

            ImGui::SetCursorPosX(contentWidth - 260.f * m_uiScale);
            auto z = m_core->getSliceHeight();
            ImGui::SetNextItemWidth(150.f * m_uiScale);
            bool zChanged = ImGui::InputFloat("  ", &z, 0.08f, 1.f, "%.2f mm");

            ImGui::SameLine();
            // button for resetting z to 0
            if (ImGui::Button("Reset"))
            {
                z = 0.f;
                zChanged = true;
            }

            m_dirty = m_dirty || zChanged;

            m_core->setSliceHeight(z);
            if (zChanged)
            {
                invalidateView();
            }

            ImGui::EndMenuBar();
        }

        m_contentAreaMin = ImGui::GetWindowContentRegionMin();
        m_contentAreaMax = ImGui::GetWindowContentRegionMax();

        auto constexpr sliderWidth_px = 30.f;

        auto const prevRenderWindowSize = m_renderWindowSize_px;
        m_renderWindowSize_px = {
          {ImGui::GetWindowWidth() - sliderWidth_px,
           ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y}};
        float constexpr tolerance = 1.E-4f;
        if (fabs(prevRenderWindowSize.x - m_renderWindowSize_px.x) > tolerance ||
            fabs(prevRenderWindowSize.y - m_renderWindowSize_px.y) > tolerance)
        {
            invalidateView();

            // Mark viewport size as changed for permanent centering
            if (m_permanentCenteringEnabled)
            {
                m_viewportSizeChangedSinceLastCenter = true;
            }
        }

        ImGui::Image(reinterpret_cast<void *>(static_cast<intptr_t>(textureId)),
                     ImVec2(static_cast<float>(m_renderWindowSize_px.x),
                            static_cast<float>(m_renderWindowSize_px.y)));

        auto const contentMin =
          ImVec2{ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x,
                 ImGui::GetWindowPos().y + ImGui::GetWindowContentRegionMin().y};

        auto const contentMax =
          ImVec2{ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x,
                 ImGui::GetWindowPos().y + ImGui::GetWindowContentRegionMax().y};

        auto const windowCenter =
          ImVec2(0.5f * (contentMin.x + contentMax.x), 0.5f * (contentMin.y + contentMax.y));

        m_renderWindowState.isMoving =
          m_renderWindowState.isMoving || m_camera.update(io.DeltaTime * 1000.f);
        m_dirty = m_dirty || m_renderWindowState.isMoving;

        // Event handling:
        if (ImGui::IsWindowHovered() && io.MousePos.x < contentMax.x - sliderWidth_px)
        {

            io.MouseDragThreshold = 1.f;
            auto mousePos = io.MousePos;

            if (m_camera.mouseMotionHandler(mousePos.x, mousePos.y))
            {
                invalidateView();
            }
            if (!ImGui::IsAnyMouseDown())
            {
                m_camera.mouseInputHandler(ImGuiMouseButton_Left, -1, mousePos.x, mousePos.y);
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                ImGui::IsMouseHoveringRect(contentMin, contentMax))
            {
                m_camera.mouseInputHandler(ImGuiMouseButton_Left, 0, mousePos.x, mousePos.y);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
                ImGui::IsMouseHoveringRect(contentMin, contentMax))
            {
                m_camera.mouseInputHandler(ImGuiMouseButton_Right, 0, mousePos.x, mousePos.y);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) &&
                ImGui::IsMouseHoveringRect(contentMin, contentMax))
            {
                m_camera.mouseInputHandler(ImGuiMouseButton_Middle, 0, mousePos.x, mousePos.y);
            }

            if (fabs(io.MouseWheel) > 0. && ImGui::IsMouseHoveringRect(contentMin, contentMax))
            {
                m_camera.zoom(-io.MouseWheel * 0.1f);
                m_renderWindowState.isMoving = true;
                m_dirty = true;
                m_renderWindowState.currentLine = 0;
            }
        }

        ImGui::SameLine();
        slider();

        ImGui::End();
        ImGui::PopStyleVar();
        img->unbind();

        if (!m_core->isRendererReady() || m_core->isAnyCompilationInProgress())
        {
            m_view->startAnimationMode();
            ImGuiWindowFlags window_flags =
              ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
              ImGuiWindowFlags_NoNav;
#ifdef IMGUI_HAS_DOCK
            window_flags |= ImGuiWindowFlags_NoDocking;
#endif
            bool open = true;

            ImGui::SetNextWindowBgAlpha(0.0f);

            if (ImGui::Begin("ProgressIndicator", &open, window_flags))
            {
                ImGui::SetWindowPos({windowCenter.x - 15.f, windowCenter.y - 15.f});
                ui::loadingIndicatorCircle("compiling",
                                           30,
                                           ImVec4(1.0f, 0.0, 0.0f, 0.8f),
                                           ImVec4(1.0f, 1.0f, 1.0f, 0.5f),
                                           12,
                                           10.0f);
                ImGui::End();
            }
        }
    }

    void RenderWindow::updateCamera()
    {
        m_core->applyCamera(m_camera);
    }

    bool RenderWindow::isRenderingInProgress() const
    {
        return m_renderWindowState.isRendering;
    }

    void RenderWindow::invalidateView()
    {
        m_dirty = true;
        m_renderWindowState.isMoving = true;
        m_renderWindowState.currentLine = 0;
        m_renderWindowState.renderingStepSize = 1;
        m_renderWindowState.isRendering = false;
    }

    void RenderWindow::invalidateViewDuetoModelUpdate()
    {
        invalidateView();
        m_preComputedSdfDirty = true;
        m_parameterDirty = true;
        m_renderWindowState.renderingStepSize = 1;

        // Mark model as modified for permanent centering
        m_modelModifiedSinceLastCenter = true;
    }

    void RenderWindow::renderScene(RenderWindowState & state)
    {

        ProfileFunction;
        LOG_LOCATION

        if (!m_core->isRendererReady())
        {
            return;
        }
        auto computeToken = m_core->requestComputeToken();
        if (!computeToken.has_value())
        {
            return;
        }

        if (!m_enableHQRendering)
        {
            m_core->setPreCompSdfSize(128u);
        }

        if (state.isMoving)
        {
            m_core->renderLowResPreview();
            m_lastLowResRenderTime = std::chrono::system_clock::now();
            return;
        }

        if (!m_enableHQRendering)
        {
            return;
        }

        m_core->setPreCompSdfSize(256u);

        auto const timeSinceLastLowResRender =
          std::chrono::system_clock::now() - m_lastLowResRenderTime;

        if (timeSinceLastLowResRender < std::chrono::seconds(1))
        {
            return;
        }

        if (!state.isMoving)
        {
            m_core->precomputeSdfForWholeBuildPlatform();
        }
        {
            auto const maxHeight = m_core->getResultImage()->getHeight();

            if (state.currentLine < maxHeight)
            {

                if (m_core->renderScene(state.currentLine,
                                        state.currentLine + state.renderingStepSize))
                {
                    state.currentLine += state.renderingStepSize;
                }
            }
            else
            {
                m_dirty = false;
            }
        }
    }

    void RenderWindow::hide()
    {
        m_isVisible = false;
    }

    void RenderWindow::show()
    {
        m_isVisible = true;
    }

    void RenderWindow::centerView()
    {
        m_centerViewRequested = true;
        invalidateView();
    }

    void RenderWindow::setTopView()
    {
        // Top view: looking down the Z axis (pitch = +90°, yaw = 0°)
        m_camera.setAngle(CL_M_PI_F / 2.0f, 0.0f);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::setFrontView()
    {
        // Front view: looking along the Y axis (pitch = 0°, yaw = -90°)
        m_camera.setAngle(0.0f, -CL_M_PI_F / 2.0f);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::setLeftView()
    {
        // Left view: looking along the X axis (pitch = 0°, yaw = 0°)
        m_camera.setAngle(0.0f, 0.0f);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::setRightView()
    {
        // Right view: looking along the negative X axis (pitch = 0°, yaw = 180°)
        m_camera.setAngle(0.0f, CL_M_PI_F);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::setBackView()
    {
        // Back view: looking along the negative Y axis (pitch = 0°, yaw = 90°)
        m_camera.setAngle(0.0f, CL_M_PI_F / 2.0f);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::setBottomView()
    {
        // Bottom view: looking up the Z axis (pitch = -90°, yaw = 0°)
        m_camera.setAngle(-CL_M_PI_F / 2.0f, 0.0f);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::setIsometricView()
    {
        // Isometric view: pitch = -35.26°, yaw = 45° (standard CAD isometric)
        float const pitch = -std::atan(1.0f / std::sqrt(2.0f)); // ~-35.26 degrees
        float const yaw = CL_M_PI_F / 4.0f;                     // 45 degrees
        m_camera.setAngle(pitch, yaw);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::togglePerspective()
    {
        // This would require camera implementation to support orthographic/perspective toggle
        // For now, we'll save current view and restore it
        saveCurrentView();
        invalidateView();
    }

    void RenderWindow::frameAll()
    {
        centerView();  // Same as center view for now
        zoomExtents(); // Zoom to fit all objects in view
    }

    void RenderWindow::zoomExtents()
    {
        // Zoom to fit all objects in view
        if (m_core->updateBBox() && m_core->getBoundingBox().has_value())
        {
            auto const bbox = m_core->getBoundingBox().value();
            m_camera.adjustDistanceToTarget(bbox, m_renderWindowSize_px.x, m_renderWindowSize_px.y);
            invalidateView();
        }
    }

    void RenderWindow::zoomSelected()
    {
        // For now, same as zoom extents since we don't have selection
        zoomExtents();
    }

    void RenderWindow::panLeft()
    {
        // Get current look at position and move it left
        auto currentLookAt = m_camera.getLookAt();
        Position newLookAt{currentLookAt.x - m_panSensitivity, currentLookAt.y, currentLookAt.z};
        m_camera.setLookAt(newLookAt);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::panRight()
    {
        // Get current look at position and move it right
        auto currentLookAt = m_camera.getLookAt();
        Position newLookAt{currentLookAt.x + m_panSensitivity, currentLookAt.y, currentLookAt.z};
        m_camera.setLookAt(newLookAt);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::panUp()
    {
        // Get current look at position and move it up
        auto currentLookAt = m_camera.getLookAt();
        Position newLookAt{currentLookAt.x, currentLookAt.y, currentLookAt.z + m_panSensitivity};
        m_camera.setLookAt(newLookAt);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::panDown()
    {
        // Get current look at position and move it down
        auto currentLookAt = m_camera.getLookAt();
        Position newLookAt{currentLookAt.x, currentLookAt.y, currentLookAt.z - m_panSensitivity};
        m_camera.setLookAt(newLookAt);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::rotateLeft()
    {
        m_camera.rotate(0.0f, -m_rotateSensitivity);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::rotateRight()
    {
        m_camera.rotate(0.0f, m_rotateSensitivity);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::rotateUp()
    {
        m_camera.rotate(m_rotateSensitivity, 0.0f);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::rotateDown()
    {
        m_camera.rotate(-m_rotateSensitivity, 0.0f);
        onCameraManuallyMoved();
        invalidateView();
    }

    void RenderWindow::previousView()
    {
        if (!m_viewHistory.empty() && m_currentViewIndex > 0)
        {
            m_currentViewIndex--;
            auto const & view = m_viewHistory[m_currentViewIndex];

            // Restore the view (this would require camera API support)
            // For now, we'll invalidate the view
            invalidateView();
        }
    }

    void RenderWindow::nextView()
    {
        if (!m_viewHistory.empty() && m_currentViewIndex < m_viewHistory.size() - 1)
        {
            m_currentViewIndex++;
            auto const & view = m_viewHistory[m_currentViewIndex];

            // Restore the view (this would require camera API support)
            // For now, we'll invalidate the view
            invalidateView();
        }
    }

    void RenderWindow::saveCurrentView()
    {
        // Save current camera state using available methods
        auto eyePos = m_camera.getEyePosition();
        auto lookAt = m_camera.getLookAt();

        m_savedView.position = Vector3{eyePos.x, eyePos.y, eyePos.z};
        m_savedView.target = Vector3{lookAt.x, lookAt.y, lookAt.z};
        m_savedView.up = Vector3{0.0f, 0.0f, 1.0f}; // Assuming Z-up
        m_savedView.distance = 100.0f;    // Default distance since we can't access private members
        m_savedView.isPerspective = true; // Default to perspective
        m_hasSavedView = true;

        // Also add to history
        CameraView currentView = m_savedView;
        m_viewHistory.push_back(currentView);
        m_currentViewIndex = m_viewHistory.size() - 1;

        // Limit history size
        if (m_viewHistory.size() > 20)
        {
            m_viewHistory.erase(m_viewHistory.begin());
            if (m_currentViewIndex > 0)
            {
                m_currentViewIndex--;
            }
        }
    }

    void RenderWindow::restoreSavedView()
    {
        if (m_hasSavedView)
        {
            // Restore the saved view (this would require camera API support)
            // For now, we'll invalidate the view
            invalidateView();
        }
    }

    void RenderWindow::toggleFlyMode()
    {
        m_flyModeEnabled = !m_flyModeEnabled;
        m_cameraMode = m_flyModeEnabled ? CameraMode::Fly : CameraMode::Orbit;
        invalidateView();
    }

    void RenderWindow::setOrbitMode()
    {
        m_cameraMode = CameraMode::Orbit;
        m_flyModeEnabled = false;
    }

    void RenderWindow::setPanMode()
    {
        m_cameraMode = CameraMode::Pan;
        m_flyModeEnabled = false;
    }

    void RenderWindow::setZoomMode()
    {
        m_cameraMode = CameraMode::Zoom;
        m_flyModeEnabled = false;
    }

    void RenderWindow::resetOrientation()
    {
        m_cameraMode = CameraMode::Orbit;
        m_flyModeEnabled = false;

        // Reset to isometric view
        setIsometricView();
    }

    void RenderWindow::togglePermanentCentering()
    {
        setPermanentCentering(!m_permanentCenteringEnabled);
        frameAll(); // Recenter view when toggling
    }

    void RenderWindow::setPermanentCentering(bool enabled)
    {
        m_permanentCenteringEnabled = enabled;

        // Save to config
        if (m_configManager)
        {
            m_configManager->setValue("renderWindow", "permanentCenteringEnabled", enabled);
            m_configManager->save();
        }

        if (enabled)
        {
            // Initialize tracking state
            updateCameraStateTracking();
            m_modelModifiedSinceLastCenter = true; // Force initial centering
            m_lastViewportSize = m_renderWindowSize_px;
            m_viewportSizeChangedSinceLastCenter = false;
        }
        else
        {
            // Clear tracking state when disabled
            m_lastCameraStateValid = false;
            m_viewportSizeChangedSinceLastCenter = false;
        }
    }

    bool RenderWindow::isPermanentCenteringEnabled() const
    {
        return m_permanentCenteringEnabled;
    }

    void RenderWindow::updateCameraStateTracking()
    {
        m_lastCameraState = getCurrentCameraState();
        m_lastCameraStateValid = true;
    }

    bool RenderWindow::shouldRecalculateCenter()
    {
        if (!m_permanentCenteringEnabled)
        {
            return false;
        }

        // Always recalculate if model was modified
        if (m_modelModifiedSinceLastCenter)
        {
            return true;
        }

        // Always recalculate if viewport size changed
        if (m_viewportSizeChangedSinceLastCenter)
        {
            return true;
        }

        // Check if camera has moved
        if (!m_lastCameraStateValid)
        {
            return true;
        }

        auto const currentState = getCurrentCameraState();
        return currentState != m_lastCameraState;
    }

    RenderWindow::CameraState RenderWindow::getCurrentCameraState()
    {
        CameraState state;

        // Get current camera parameters - we need to access these through the public API
        auto const eyePos = m_camera.getEyePosition();
        auto const lookAt = m_camera.getLookAt();

        state.lookAt = Position{lookAt.x, lookAt.y, lookAt.z};

        // For pitch/yaw and distance, we'd need to compute them from eye position and look at
        // Since we don't have direct access, we'll use the eye position as a proxy for changes
        Position const eyePosition{eyePos.x, eyePos.y, eyePos.z};
        Position const lookAtPosition{lookAt.x, lookAt.y, lookAt.z};
        Position const eyeToLookAt = lookAtPosition - eyePosition;

        state.distance = eyeToLookAt.norm();

        // Calculate pitch and yaw from the eye-to-lookat vector
        state.pitch = std::asin(eyeToLookAt.z() / state.distance);
        state.yaw = std::atan2(eyeToLookAt.y(), eyeToLookAt.x());

        return state;
    }

    void RenderWindow::onCameraManuallyMoved()
    {
        // When camera is manually moved, disable permanent centering temporarily
        // until the next model update or manual center request
        if (m_permanentCenteringEnabled)
        {
            updateCameraStateTracking();
        }
    }

    void RenderWindow::render(RenderWindowState & state)
    {
        ProfileFunction;
        m_uiScale = ImGui::GetIO().FontGlobalScale * 2.0f;

        if (!m_core->isRendererReady() || m_core->isAnyCompilationInProgress())
        {
            m_view->startAnimationMode();
            state.isRendering = false;
            state.renderQualityWhileMoving = 0.1f;

            invalidateViewDuetoModelUpdate();
            return;
        }

        m_view->stopAnimationMode();

        if (!m_dirty || state.isRendering)
        {
            return;
        }

        if (m_dirty)
        {
            m_view->startAnimationMode();
        }

        // Handle both manual center requests and permanent centering
        bool const shouldCenter = m_centerViewRequested || shouldRecalculateCenter();

        if (shouldCenter)
        {
            bool boundingBoxValid = m_core->getBoundingBox().has_value();
            if (boundingBoxValid)
            {
                auto bb = m_core->getBoundingBox().value();
                boundingBoxValid =
                  (fabs(bb.max.x - bb.min.x > 0.f) && fabs(bb.max.y - bb.min.y > 0.f) &&
                   fabs(bb.max.z - bb.min.z > 0.f));

                if (boundingBoxValid)
                {
                    m_camera.centerView(bb);
                    m_camera.adjustDistanceToTarget(
                      bb, m_renderWindowSize_px.x, m_renderWindowSize_px.y);

                    // Update tracking state for permanent centering
                    if (m_permanentCenteringEnabled)
                    {
                        updateCameraStateTracking();
                        m_modelModifiedSinceLastCenter = false;
                        m_viewportSizeChangedSinceLastCenter = false;
                        m_lastViewportSize = m_renderWindowSize_px;
                    }

                    m_centerViewRequested = false;
                }
            }

            if (!boundingBoxValid) // no bounding box or bounding box contains inf or nan
            {
                // just set the look at point to the center of the build platform
                m_camera.setLookAt({200.0, 200.0, 50.0});

                // Update tracking state for permanent centering
                if (m_permanentCenteringEnabled)
                {
                    updateCameraStateTracking();
                    m_modelModifiedSinceLastCenter = false;
                    m_viewportSizeChangedSinceLastCenter = false;
                    m_lastViewportSize = m_renderWindowSize_px;
                }
            }

            invalidateView();
        }
        if (state.isMoving)
        {
            if (m_preComputedSdfDirty)
            {
                m_core->getResourceContext()->getRenderingSettings().approximation = AM_FULL_MODEL;
            }
        }

        if (m_core->setScreenResolution(
              static_cast<int>(
                std::clamp(m_renderWindowSize_px.x * state.renderQuality, 1.f, 16000.f)),
              static_cast<int>(
                std::clamp(m_renderWindowSize_px.y * state.renderQuality, 1.f, 16000.f))))
        {
            invalidateView();
        }

        bool previewResolutionChanged = false;

        std::pair<int, int> const lowResPreviewResolution = m_core->getLowResPreviewResolution();

        int newWidth = static_cast<int>(
          std::clamp(m_renderWindowSize_px.x * state.renderQualityWhileMoving, 1.f, 16000.f));
        int newHeight = static_cast<int>(
          std::clamp(m_renderWindowSize_px.y * state.renderQualityWhileMoving, 1.f, 16000.f));

        float widthChangePercent = std::abs(newWidth - lowResPreviewResolution.first) /
                                   static_cast<float>(lowResPreviewResolution.first) * 100.0f;
        float heightChangePercent = std::abs(newHeight - lowResPreviewResolution.second) /
                                    static_cast<float>(lowResPreviewResolution.second) * 100.0f;

        float currentAspectRatio =
          static_cast<float>(lowResPreviewResolution.first) / lowResPreviewResolution.second;
        float newAspectRatio = static_cast<float>(newWidth) / newHeight;

        if (widthChangePercent > 20.0f || heightChangePercent > 20.0f ||
            std::abs(currentAspectRatio - newAspectRatio) > 0.01f)
        {
            m_core->setLowResPreviewResolution(newWidth, newHeight);
            previewResolutionChanged = true;
        }
        state.isRendering = true;
        auto const renderFrame = [&]()
        {
            renderScene(state);

            auto const img = m_core->getResultImage();
            img->bind(); // triggers texture transfer
            img->unbind();
        };

        // PID controller parameters
        float constexpr kp = 0.0001f;   // Proportional gain
        float constexpr ki = 0.00001f;  // Integral gain
        float constexpr kd = 0.000001f; // Derivative gain

        auto const executionDuration_ms =
          measure<std::chrono::milliseconds>::execution(renderFrame);

        auto constexpr progressiveTargetRenderTime_ms = 100;
        auto constexpr tolerance_ms = 1;
        auto constexpr targetFrameTime_ms = 50; // Target frame time for 60 FPS
        // Calculate the error
        float error = targetFrameTime_ms - executionDuration_ms;
        if (!previewResolutionChanged && (state.isMoving || m_core->isAnyCompilationInProgress()) &&
            executionDuration_ms > 0 && fabs(error) > 0)
        {
            state.fpsIntegral *= 0.8f;
            // Update integral and derivative
            state.fpsIntegral += error;

            float derivative = error - state.fpsPreviousError;

            // Calculate the adjustment using PID formula
            float adjustment = kp * error + ki * state.fpsIntegral + kd * derivative;

            // Adjust render quality
            state.renderQualityWhileMoving += adjustment;

            // Clamp the render quality to valid range
            state.renderQualityWhileMoving =
              std::clamp(state.renderQualityWhileMoving, 0.05f, state.renderQuality);
        }
        if (!state.isMoving && !m_core->isAnyCompilationInProgress() && executionDuration_ms > 0)
        {
            if (executionDuration_ms > progressiveTargetRenderTime_ms + tolerance_ms)
            {
                auto const fraction = m_preComputedSdfDirty ? 0.1f : 0.5f;
                state.renderingStepSize =
                  std::clamp(static_cast<size_t>(state.renderingStepSize * fraction),
                             size_t{2},
                             m_core->getResultImage()->getHeight());
            }

            if (executionDuration_ms < progressiveTargetRenderTime_ms - tolerance_ms)
            {
                state.renderingStepSize =
                  std::clamp(static_cast<size_t>(state.renderingStepSize * 1.5 + 1),
                             size_t{1},
                             m_core->getResultImage()->getHeight());
            }
        }

        state.isMoving = false;
        state.isRendering = false;
    }

    void RenderWindow::slider()
    {
        ProfileFunction;
        if (!m_core->getBoundingBox().has_value())
        {
            return;
        }

        auto z = m_core->getSliceHeight();
        auto const maxZ =
          m_core->getBoundingBox().has_value() ? m_core->getBoundingBox()->max.z : 200.f;
        auto const minZ =
          m_core->getBoundingBox().has_value() ? m_core->getBoundingBox()->min.z : 0.f;
        const bool zChanged = ImGui::VSliderFloat(
          " ",
          ImVec2(15, m_contentAreaMax.y - m_contentAreaMin.y - 10.f * m_uiScale),
          &z,
          minZ,
          maxZ,
          " ");

        m_dirty = m_dirty || zChanged;
        m_renderWindowState.isMoving = m_renderWindowState.isMoving || zChanged;

        m_core->setSliceHeight(z);
        if (zChanged)
        {
            m_core->invalidatePreCompSdf();
            m_core->precomputeSdfForWholeBuildPlatform();
            invalidateView();
        }
    }

    bool RenderWindow::isVisible() const
    {
        return m_isVisible;
    }

    bool RenderWindow::isHovered() const
    {
        return m_isWindowHovered && isVisible();
    }

    bool RenderWindow::isFocused() const
    {
        return m_isWindowFocused && isVisible();
    }

    void RenderWindow::handleKeyInput()
    {
        // Handle keyboard input - this can be extended later
        // For now, this is just a placeholder implementation
    }

    void RenderWindow::zoomIn()
    {
        m_camera.zoom(-0.1f); // Negative zoom means zoom in
        invalidateView();
    }

    void RenderWindow::zoomOut()
    {
        m_camera.zoom(0.1f); // Positive zoom means zoom out
        invalidateView();
    }

    void RenderWindow::resetZoom()
    {
        // Reset to a reasonable default distance
        // We need to access the private members, so let's use the centerView logic
        if (m_core->updateBBox() && m_core->getBoundingBox().has_value())
        {
            auto const bbox = m_core->getBoundingBox().value();
            m_camera.adjustDistanceToTarget(bbox, m_renderWindowSize_px.x, m_renderWindowSize_px.y);
            invalidateView();
        }
    }

}
