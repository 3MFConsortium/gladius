#pragma once

#include <Eigen/Core>

#include <float.h>

namespace gladius
{
    class BBox
    {
      public:
        void extend(Eigen::Vector3f pos);

        [[nodiscard]] Eigen::Vector3f getMax() const;
        [[nodiscard]] Eigen::Vector3f getMin() const;

      private:
        Eigen::Vector3f m_max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
        Eigen::Vector3f m_min{FLT_MAX, FLT_MAX, FLT_MAX};
    };

    inline void BBox::extend(Eigen::Vector3f pos)
    {
        m_min.x() = std::min(m_min.x(), pos.x());
        m_min.y() = std::min(m_min.y(), pos.y());
        m_min.z() = std::min(m_min.z(), pos.z());
        m_max.x() = std::max(m_max.x(), pos.x());
        m_max.y() = std::max(m_max.y(), pos.y());
        m_max.z() = std::max(m_max.z(), pos.z());
    }

    [[nodiscard]] inline Eigen::Vector3f BBox::getMax() const
    {
        return m_max;
    }

    [[nodiscard]] inline Eigen::Vector3f BBox::getMin() const
    {
        return m_min;
    }

}