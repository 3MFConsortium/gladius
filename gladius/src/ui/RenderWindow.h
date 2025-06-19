#pragma once

#include "GLView.h"
#include "OrbitalCamera.h"
#include "compute/ComputeCore.h"
#include "../types.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>
#include <CL/cl_platform.h>

namespace gladius::ui
{
    class ShortcutManager;
}

namespace gladius
{
    class ConfigManager;
}

namespace gladius::ui
{
    using TimeStamp =
      std::chrono::time_point<std::chrono::system_clock,
                              std::chrono::duration<long, std::ratio<1, 1000000000>>>;
    struct RenderWindowState
    {
        TimeStamp timeLastMove = std::chrono::system_clock::now();
        float renderQuality = 1.2f;
        float renderQualityWhileMoving = 0.02f;
        bool isRendering = false;
        bool isMoving = false;
        size_t currentLine = 0;
        size_t renderingStepSize = 5;

        
        float fpsPreviousError = 0.0f;
        float fpsIntegral = 0.0f;
    };

    class RenderWindow
    {
      public:
        explicit RenderWindow() = default;
        void initialize(ComputeCore * core, GLView * view, std::shared_ptr<ShortcutManager> shortcutManager, gladius::ConfigManager * configManager);

        void renderWindow();
        void updateCamera();
        bool isRenderingInProgress() const;
        void invalidateView();
        void invalidateViewDuetoModelUpdate();
        void renderScene(RenderWindowState & state);

        void hide();
        void show();

        void centerView();

        // Camera view methods
        void setTopView();
        void setFrontView();
        void setLeftView();
        void setRightView();
        void setBackView();
        void setBottomView();
        void setIsometricView();
        void togglePerspective();

        // Zoom methods
        void zoomIn();
        void zoomOut();
        void resetZoom();
        void zoomExtents();
        void zoomSelected();
        void frameAll();

        // Camera movement methods
        void panLeft();
        void panRight();
        void panUp();
        void panDown();
        void rotateLeft();
        void rotateRight();
        void rotateUp();
        void rotateDown();

        // View management
        void previousView();
        void nextView();
        void saveCurrentView();
        void restoreSavedView();

        // Camera modes
        void toggleFlyMode();
        void setOrbitMode();
        void setPanMode();
        void setZoomMode();
        void resetOrientation();
        
        // Permanent centering
        void togglePermanentCentering();
        void setPermanentCentering(bool enabled);
        [[nodiscard]] bool isPermanentCenteringEnabled() const;

        [[nodiscard]] bool isVisible() const;

        void handleKeyInput();

        /**
         * @brief Check if mouse is hovering over the render window
         * @return true if the render window is being hovered
         */
        bool isHovered() const;

      private:
        void render(RenderWindowState & state);
        void slider();

        GLView * m_view{};

        ComputeCore * m_core;
        std::shared_ptr<ShortcutManager> m_shortcutManager;
        gladius::ConfigManager * m_configManager;

        std::atomic<bool> m_dirty{true};
        std::atomic<bool> m_parameterDirty{false};
        std::atomic<bool> m_preComputedSdfDirty{true};

        ui::OrbitalCamera m_camera;

        float2 m_renderWindowSize_px{{128, 128}};

        bool m_isVisible{true};

        RenderWindowState m_renderWindowState{};

        bool m_centerViewRequested = false;
        bool m_enableHQRendering = true;

        ImVec2 m_contentAreaMin;
        ImVec2 m_contentAreaMax;

        TimeStamp m_lastLowResRenderTime;

        float m_uiScale = 1.0f;

        // View management
        struct CameraView
        {
            Vector3 position;
            Vector3 target;
            Vector3 up;
            float distance;
            bool isPerspective;
        };
        
        std::vector<CameraView> m_viewHistory;
        size_t m_currentViewIndex = 0;
        CameraView m_savedView;
        bool m_hasSavedView = false;
        
        // Camera modes
        enum class CameraMode
        {
            Orbit,
            Pan,
            Zoom,
            Fly
        };
        
        CameraMode m_cameraMode = CameraMode::Orbit;
        bool m_flyModeEnabled = false;
        
        // Camera movement parameters
        float m_panSensitivity = 0.1f;
        float m_rotateSensitivity = 0.02f;
        float m_zoomSensitivity = 0.1f;
        
        // Permanent centering state
        bool m_permanentCenteringEnabled = false;
        bool m_lastCameraStateValid = false;
        
        // Camera state tracking for permanent centering
        struct CameraState 
        {
            Position lookAt;
            float pitch;
            float yaw;
            float distance;
            
            bool operator==(CameraState const & other) const
            {
                return lookAt.isApprox(other.lookAt, 1e-6f) &&
                       std::abs(pitch - other.pitch) < 1e-6f &&
                       std::abs(yaw - other.yaw) < 1e-6f &&
                       std::abs(distance - other.distance) < 1e-6f;
            }
            
            bool operator!=(CameraState const & other) const
            {
                return !(*this == other);
            }
        };
        
        CameraState m_lastCameraState;
        bool m_modelModifiedSinceLastCenter = false;
        float2 m_lastViewportSize{{0, 0}};
        bool m_viewportSizeChangedSinceLastCenter = false;
        
        // Helper methods for permanent centering
        void updateCameraStateTracking();
        bool shouldRecalculateCenter();
        CameraState getCurrentCameraState();
        void onCameraManuallyMoved();
    };
}
