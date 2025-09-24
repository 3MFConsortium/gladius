#pragma once

#include "../kernel/types.h"
#include <CL/cl_platform.h>
#include <cmath>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/src/Core/Matrix.h>

namespace gladius::ui
{

    using Position = Eigen::Vector3f;
    class OrbitalCamera
    {
      public:
        void mouseInputHandler(int button, int state, float x, float y);
        auto mouseMotionHandler(float x, float y) -> bool;
        [[nodiscard]] auto getEyePosition() const -> cl_float3;
        auto getLookAt() -> cl_float3;
        void setLookAt(Position const & lookAt);

        void zoom(float increment);

        auto computeModelViewPerspectiveMatrix() const -> cl_float16;
        bool update(float deltaTime_ms);

        void centerView(BoundingBox const & bbox);
        void
        adjustDistanceToTarget(BoundingBox const & bbox, float viewportWidth, float viewportHeight);
        void setAngle(float pitch, float yaw);
        void rotate(float pitch, float yaw);

      private:
        auto computeEyePosition() const -> Position;

        float m_eyeDist = 100.0f;
        float m_eyeDistTarget = 800.0f;

        float m_pitch = 0.6f;
        float m_yaw = -1.6f;

        float m_pitchTarget = 0.6f;
        float m_yawTarget = -1.6f;

        float m_prevMousPosX = 0.f;
        float m_prevMousPosY = 0.f;
        int m_mouseButton = -1;
        Position m_lookAt{200.f, 200.f, 10.f};
        Position m_lookAtTarget{200.f, 200.f, 10.f};
    };
}