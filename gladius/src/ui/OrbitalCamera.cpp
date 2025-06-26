#include "OrbitalCamera.h"
#include <CL/cl_platform.h>
#include <cmath>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Geometry>
#include <eigen3/Eigen/src/Core/Matrix.h>
#include <fmt/format.h>
#include <iostream>
#include <array>
#include <limits>

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
        //if (deltaTime_ms > 50.f)
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


    void OrbitalCamera::adjustDistanceToTarget(BoundingBox const & bbox, float viewportWidth, float viewportHeight)
    {
        // Calculate the aspect ratio (matches the renderer exactly)
        float const aspectRatio = viewportHeight > 0.0f ? viewportWidth / viewportHeight : 1.0f;

        // Use the same parameters as the OpenCL renderer
        float constexpr lensLength = 1.0f;  // matches renderer.cl
        
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

        float maxRequiredDistance = 10.0f; // minimum distance
        
        // For each corner, calculate the minimum camera distance required
        for (Position const & corner : corners)
        {
            // Try different camera distances to find the minimum that makes this corner visible
            float testDistance = 10.0f;
            float stepSize = 100.0f;
            bool foundValidDistance = false;
            
            // Binary search for the minimum distance
            float minValid = 10000.0f;
            float maxInvalid = 0.0f;
            
            // First, find a range where the corner is visible
            for (int coarseSearch = 0; coarseSearch < 20; ++coarseSearch)
            {
                // Calculate eye position at this test distance
                Position const testEyePos = {
                    m_lookAt.x() + testDistance * std::cos(m_yaw) * std::cos(m_pitch),
                    m_lookAt.y() + testDistance * std::sin(m_yaw) * std::cos(m_pitch),
                    m_lookAt.z() + testDistance * std::sin(m_pitch)
                };
                
                // Create view matrix (matches computeModelViewPerspectiveMatrix exactly)
                Position ww = m_lookAt - testEyePos;
                ww.normalize();
                Position const upVector = {0., 0., 1.};
                Position uu = ww.cross(upVector);
                uu.normalize();
                Position vv = uu.cross(ww);
                vv.normalize();
                
                // Transform corner to view space (matches matrixVectorMul3f in OpenCL)
                Position const cornerInViewSpace = {
                    (corner - testEyePos).dot(uu),  // u component (right)
                    (corner - testEyePos).dot(vv),  // v component (up)
                    (corner - testEyePos).dot(ww)   // w component (forward)
                };
                
                // Skip if corner is behind camera
                if (cornerInViewSpace.z() <= 0.0f)
                {
                    testDistance += stepSize;
                    continue;
                }
                
                // Calculate screen position (matches renderer.cl logic exactly)
                // In renderer: rayDirection = normalize(MVP * (screenPos.x, screenPos.y/aspectRatio, lensLength))
                // So: screenPos = inverse_transform of rayDirection
                Position const rayDir = cornerInViewSpace.normalized();
                
                // From rayDir, extract screen coordinates
                // rayDir should equal normalize((screenPos.x, screenPos.y/aspectRatio, lensLength))
                // So: screenPos.x = rayDir.x() * lensLength / rayDir.z()
                //     screenPos.y = rayDir.y() * lensLength * aspectRatio / rayDir.z()
                float const screenX = rayDir.x() * lensLength / rayDir.z();
                float const screenY = rayDir.y() * lensLength * aspectRatio / rayDir.z();
                
                // Check if corner is within visible screen bounds [-0.5, 0.5]
                bool const isVisible = (std::abs(screenX) <= 0.5f) && (std::abs(screenY) <= 0.5f);
                
                if (isVisible)
                {
                    minValid = std::min(minValid, testDistance);
                    foundValidDistance = true;
                    testDistance -= stepSize * 0.5f; // Try smaller distance
                    stepSize *= 0.8f; // Reduce step size for finer search
                }
                else
                {
                    maxInvalid = std::max(maxInvalid, testDistance);
                    testDistance += stepSize; // Try larger distance
                }
                
                // Early termination if we found a tight bound
                if (foundValidDistance && (minValid - maxInvalid) < 1.0f)
                {
                    break;
                }
            }
            
            if (foundValidDistance)
            {
                maxRequiredDistance = std::max(maxRequiredDistance, minValid);
            }
            else
            {
                // Fallback: use a conservative estimate based on bounding box size
                float const bboxSize = std::max({
                    bbox.max.x - bbox.min.x,
                    bbox.max.y - bbox.min.y, 
                    bbox.max.z - bbox.min.z
                });
                maxRequiredDistance = std::max(maxRequiredDistance, bboxSize * 2.0f);
            }
        }

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
// Alternative MVP-based approach (for reference):
    // We could also use a proper 4x4 view matrix and transform the corners:
    // 
    // Eigen::Matrix4f viewMatrix;
    // viewMatrix.setIdentity();
    // viewMatrix.block<3,1>(0,0) = uu;  // right
    // viewMatrix.block<3,1>(0,1) = vv;  // up  
    // viewMatrix.block<3,1>(0,2) = ww;  // forward
    // viewMatrix.block<3,1>(0,3) = -(viewMatrix.block<3,3>(0,0) * eyePos);  // translation
    //
    // Then for each corner:
    // Eigen::Vector4f homogeneousCorner(corner.x(), corner.y(), corner.z(), 1.0f);
    // Eigen::Vector4f cameraSpaceCorner = viewMatrix * homogeneousCorner;
    // float cameraX = cameraSpaceCorner.x();
    // float cameraY = cameraSpaceCorner.y(); 
    // float cameraZ = cameraSpaceCorner.z();