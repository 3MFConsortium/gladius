

#include "OrbitalCamera.h"
#include <CL/cl_platform.h>
#include <cmath>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Geometry>
#include <eigen3/Eigen/src/Core/Matrix.h>
#include <fmt/format.h>
#include <iostream>
#include <array>

namespace gladius::ui
{

    void OrbitalCamera::mouseInputHandler(int button, int state, float x, float y)
    {
        m_mouseButton = -1;
        if (state == 0)
        {
            m_mouseButton = button;
        }

        m_prevMousPosX = x;
        m_prevMousPosY = y;
    }

    auto OrbitalCamera::mouseMotionHandler(float x, float y) -> bool
    {
        auto const deltaX = x - m_prevMousPosX;
        auto const deltaY = y - m_prevMousPosY;

        bool moving = false;
        if ((m_mouseButton == 0) || (m_mouseButton == 1))
        {
            m_pitchTarget += deltaY * 3.E-3f;
            m_yawTarget -= deltaX * 3.E-3f;
            m_yawTarget = std::fmod(m_yawTarget, 2.0f * CL_M_PI_F);
            m_pitchTarget = std::fmod(m_pitchTarget, 2.0f * CL_M_PI_F);
           
            moving = true;
        }

        // Panning
        if (m_mouseButton == 2)
        {
            Position const eyeToTarget = (m_lookAt - computeEyePosition()).normalized();
            Position const upVector = {0., 0., 1.};

            Position const cameraXAxis = eyeToTarget.cross(upVector);
            Position const cameraYAxis = eyeToTarget.cross(cameraXAxis);

            m_lookAtTarget =
              m_lookAtTarget - cameraXAxis * deltaX * 0.3f - cameraYAxis * deltaY * 0.3f;
                
            moving = true;
        }

        m_prevMousPosX = x;
        m_prevMousPosY = y;

        return moving;
    }

    void OrbitalCamera::zoom(float increment)
    {
        m_eyeDistTarget += m_eyeDistTarget * increment;
    }

    auto OrbitalCamera::computeModelViewPerspectiveMatrix() const -> cl_float16
    {
        cl_float16 matrix;
        Position eyePos = computeEyePosition();
        Position ww = m_lookAt - eyePos;
        ww.normalize();
        Position const upVector = {0., 0., 1.};
        Position uu = ww.cross(upVector);
        uu.normalize();
        Position vv = uu.cross(ww);
        vv.normalize();

        matrix.s0 = uu.x();
        matrix.s1 = uu.y();
        matrix.s2 = uu.z();
        matrix.s3 = 0.0f;

        matrix.s4 = vv.x();
        matrix.s5 = vv.y();
        matrix.s6 = vv.z();
        matrix.s7 = 0.0f;

        matrix.s8 = ww.x();
        matrix.s9 = ww.y();
        matrix.sa = ww.z();
        matrix.sb = 0.0f;

        matrix.sc = 0.0f;
        matrix.sd = 0.0f;
        matrix.se = 0.0f;
        matrix.sf = 1.0f;

        return matrix;
    }

    bool OrbitalCamera::update(float deltaTime_ms)
    {
        bool changed = false;
        auto constexpr tolerance = 0.0001f;
        auto const speedFactor = 15.E-3f;
        if (deltaTime_ms > 50.f)
        {
            m_pitch = m_pitchTarget;
            m_yaw = m_yawTarget;
            m_eyeDist = m_eyeDistTarget;
            m_lookAt = m_lookAtTarget;
            return false;
        }
        // deltaTime_ms = std::min(deltaTime_ms, 50.f);

        auto const pitchDelta = (m_pitchTarget - m_pitch);
        if (fabs(pitchDelta) > tolerance)
        {
            changed = true;
            m_pitch += pitchDelta * deltaTime_ms * speedFactor;
        }

        auto const yawDelta = (m_yawTarget - m_yaw);
        if (fabs(yawDelta) > tolerance)
        {
            changed = true;
            m_yaw += yawDelta * deltaTime_ms * speedFactor;
        }

        auto const eyeDistDelta = (m_eyeDistTarget - m_eyeDist);
        if (fabs(eyeDistDelta) > tolerance)
        {
            changed = true;
            m_eyeDist += eyeDistDelta * deltaTime_ms * speedFactor * 0.5f;
        }

        Position const lookAtDelta = m_lookAtTarget - m_lookAt;
        if (lookAtDelta.norm() > tolerance)
        {
            changed = true;
            m_lookAt = m_lookAt + lookAtDelta * deltaTime_ms * speedFactor;
        }

        return changed;
    }

    void OrbitalCamera::centerView(BoundingBox const & bbox)
    {
        m_lookAtTarget = {(bbox.max.x + bbox.min.x) * 0.5f,
                          (bbox.max.y + bbox.min.y) * 0.5f,
                          (bbox.max.z + bbox.min.z) * 0.5f};

    }

    void OrbitalCamera::adjustDistanceToTarget(BoundingBox const & bbox)
    {
        m_eyeDistTarget = 100.;
        float const factor = 1.5f;
        m_eyeDistTarget = std::max(m_eyeDistTarget, (bbox.max.x - bbox.min.x) * factor);
        m_eyeDistTarget = std::max(m_eyeDistTarget, (bbox.max.y - bbox.min.y) * factor);
        m_eyeDistTarget = std::max(m_eyeDistTarget, (bbox.max.z - bbox.min.z) * factor);
    }

    void OrbitalCamera::adjustDistanceToTarget(BoundingBox const & bbox, float viewportWidth, float viewportHeight)
    {
        // Calculate bounding box center
        Position const center = {(bbox.max.x + bbox.min.x) * 0.5f,
                                 (bbox.max.y + bbox.min.y) * 0.5f,
                                 (bbox.max.z + bbox.min.z) * 0.5f};

        // Get all 8 corners of the bounding box
        std::array<Position, 8> corners = {{
            {bbox.min.x, bbox.min.y, bbox.min.z}, // 0: min corner
            {bbox.max.x, bbox.min.y, bbox.min.z}, // 1: +x
            {bbox.min.x, bbox.max.y, bbox.min.z}, // 2: +y  
            {bbox.max.x, bbox.max.y, bbox.min.z}, // 3: +x+y
            {bbox.min.x, bbox.min.y, bbox.max.z}, // 4: +z
            {bbox.max.x, bbox.min.y, bbox.max.z}, // 5: +x+z
            {bbox.min.x, bbox.max.y, bbox.max.z}, // 6: +y+z
            {bbox.max.x, bbox.max.y, bbox.max.z}  // 7: max corner
        }};

        // Calculate the aspect ratio
        float const aspectRatio = viewportHeight > 0.0f ? viewportWidth / viewportHeight : 1.0f;

        // Get camera basis vectors (same as in computeModelViewPerspectiveMatrix)
        Position const eyePos = computeEyePosition();
        Position ww = m_lookAt - eyePos;
        ww.normalize();
        Position const upVector = {0., 0., 1.};
        Position uu = ww.cross(upVector);
        uu.normalize();
        Position vv = uu.cross(ww);
        vv.normalize();

        // The ray casting system uses a lensLength of 1.0f as the "focal length"
        // and screenPos ranges from -0.5 to +0.5, with aspect ratio correction
        // This means the effective field of view is: fov = 2 * atan(0.5 / lensLength)
        float constexpr lensLength = 1.0f;
        float const halfFovY = std::atan(0.5f / lensLength);
        float const halfFovX = std::atan(0.5f * aspectRatio / lensLength);

        float maxRequiredDistance = 10.0f; // minimum distance

        // For each bounding box corner, calculate the minimum distance required
        // to ensure it stays within the view frustum
        for (Position const & corner : corners)
        {
            // Transform corner relative to look-at point
            Position const relativePos = corner - m_lookAt;
            
            // Project onto camera coordinate system
            float const cameraX = relativePos.dot(uu);  // right
            float const cameraY = relativePos.dot(vv);  // up  
            float const cameraZ = relativePos.dot(ww);  // forward (toward camera)

            // For perspective projection, we need: |cameraX| / distance <= tan(halfFovX)
            // and |cameraY| / distance <= tan(halfFovY)
            // Solving for distance: distance >= |cameraX| / tan(halfFovX)
            
            float const tanHalfFovX = std::tan(halfFovX);
            float const tanHalfFovY = std::tan(halfFovY);
            
            if (tanHalfFovX > 0.0f && tanHalfFovY > 0.0f)
            {
                // Calculate required distance for X constraint
                float const requiredDistanceX = std::abs(cameraX) / tanHalfFovX;
                
                // Calculate required distance for Y constraint  
                float const requiredDistanceY = std::abs(cameraY) / tanHalfFovY;
                
                // Take the maximum of both constraints
                float const requiredDistance = std::max(requiredDistanceX, requiredDistanceY);
                
                // Also account for the Z offset - we need enough distance to see the corner
                // The actual distance from eye to corner should be at least requiredDistance
                float const cornerDistance = requiredDistance - cameraZ;
                
                maxRequiredDistance = std::max(maxRequiredDistance, cornerDistance);
            }
        }

        // Add some padding for better framing (10% margin)
        maxRequiredDistance *= 1.1f;

        // Ensure minimum distance
        m_eyeDistTarget = std::max(10.0f, maxRequiredDistance);
    }

    void OrbitalCamera::setAngle(float pitch, float yaw)
    {
        m_pitchTarget = pitch;
        m_yawTarget = yaw;
    }

    void OrbitalCamera::rotate(float pitch, float yaw)
    {
        m_pitchTarget += pitch;
        m_yawTarget += yaw;
    }

    auto OrbitalCamera::getEyePosition() const -> cl_float3
    {
        auto const eyePos = computeEyePosition();
        return {{eyePos.x(), eyePos.y(), eyePos.z()}};
    }

    auto OrbitalCamera::computeEyePosition() const -> Position
    {
        return {
          static_cast<cl_float>(m_lookAt.x() + m_eyeDist * std::cos(m_yaw) * std::cos(m_pitch)),
          static_cast<cl_float>(m_lookAt.y() + m_eyeDist * std::sin(m_yaw) * std::cos(m_pitch)),
          static_cast<cl_float>(m_lookAt.z() + m_eyeDist * std::sin(m_pitch))};
    }

    auto OrbitalCamera::getLookAt() -> cl_float3
    {
        return {{m_lookAt.x(), m_lookAt.y(), m_lookAt.z()}};
    }

    void OrbitalCamera::setLookAt(Position const & lookAt)
    {
        m_lookAtTarget = lookAt;
    }
}