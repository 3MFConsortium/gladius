#pragma once

#include "GLView.h"
#include "OrbitalCamera.h"
#include "compute/ComputeCore.h"
#include <atomic>
#include <chrono>
#include <filesystem>

namespace gladius::ui
{
    using TimeStamp =
      std::chrono::time_point<std::chrono::system_clock,
                              std::chrono::duration<long, std::ratio<1, 1000000000>>>;
    struct RenderWindowState
    {
        TimeStamp timeLastMove = std::chrono::system_clock::now();
        float renderQuality = 1.2f;
        float renderQualityWhileMoving = 0.2f;
        bool isRendering = false;
        bool isMoving = false;
        size_t currentLine = 0;
        size_t renderingStepSize = 5;
    };

    class RenderWindow
    {
      public:
        explicit RenderWindow() = default;
        void initialize(ComputeCore * core, GLView * view);

        void renderWindow();
        void updateCamera();
        bool isRenderingInProgress() const;
        void invalidateView();
        void invalidateViewDuetoModelUpdate();
        void renderScene(RenderWindowState & state);

        void hide();
        void show();

        void centerView();

        [[nodiscard]] bool isVisible() const;

        void handleKeyInput();

      private:
        void render(RenderWindowState & state);
        void slider();

        GLView * m_view{};

        ComputeCore * m_core;

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
    };
}
