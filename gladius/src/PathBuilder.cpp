#include "PathBuilder.h"
#include "CLMath.h"
#include "contour/ContourValidator.h"
#include "src/Contour.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <psimpl.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <deque>
#include <iterator>
#include <limits>

namespace gladius
{
    PathBuilder::PathBuilder()
    {
        m_start.setZero();
    }

    void PathBuilder::moveTo(Vector2 point)
    {
        if (!m_currentPolyLine.vertices.empty())
        {
            closePath();
        }
        m_currentPolyLine.vertices.push_back(point);
        m_start = std::move(point);
    }

    void PathBuilder::lineTo(Vector2 point)
    {
        m_currentPolyLine.vertices.push_back(point);
    }

    void PathBuilder::quadricBezier(const Vector2 point, const Vector2 controlPoint)
    {
        const Vector2 previousPoint = m_currentPolyLine.vertices.back();

        const auto subdivisons = 100;
        for (int i = 1; i < subdivisons; ++i)
        {
            const float k = static_cast<float>(i) / static_cast<float>(subdivisons - 1);
            lineTo(quadricBezierFunc(previousPoint, point, controlPoint, k));
        }
    }

    void PathBuilder::closePath()
    {
        if (m_currentPolyLine.vertices.empty())
        {
            return;
        }

        // lineTo(m_currentPolyLine.vertices.front());

        m_currentPolyLine.isClosed = true;
        flush();
    }

    void PathBuilder::addToPrimitives(PrimitiveBuffer & primitives)
    {
        // closePath();
        determineContourModiAndSort();
        for (auto & path : m_polyLines)
        {
            if (fabs(path.area) <= std::numeric_limits<float>::epsilon())
            {
                continue;
            }

            PrimitiveMeta metaData{};
            metaData.primitiveType =
              path.contourMode == ContourMode::Outer ? SDF_OUTER_POLYGON : SDF_INNER_POLYGON;

            metaData.boundingBox.min = {{0.f, 0.f, 0.f, 0.f}};
            metaData.boundingBox.max = {{0.f, 0.f, std::numeric_limits<float>::max(), 0.f}};
            metaData.start = static_cast<int>(primitives.data.size());

            for (auto & vertex : path.vertices)
            {
                primitives.data.push_back(vertex.x());
                primitives.data.push_back(vertex.y());
                float constexpr offset = 1.0f;
                metaData.boundingBox.min.x =
                  std::min(metaData.boundingBox.min.x, vertex.x() - offset);
                metaData.boundingBox.min.y =
                  std::min(metaData.boundingBox.min.y, vertex.y() - offset);

                metaData.boundingBox.max.x =
                  std::max(metaData.boundingBox.max.x, vertex.x() + offset);
                metaData.boundingBox.max.y =
                  std::max(metaData.boundingBox.max.y, vertex.y() + offset);
            }
            metaData.end = static_cast<int>(primitives.data.size() - 2);
            primitives.meta.push_back(metaData);
        }
    }

    void PathBuilder::flush()
    {
        if (m_currentPolyLine.vertices.empty())
        {
            return;
        }
        m_polyLines.push_back(std::move(m_currentPolyLine));
        m_currentPolyLine = PolyLine();
    }

    void PathBuilder::determineContourModiAndSort()
    {
        for (auto & path : m_polyLines)
        {
            determineContourMode(path, OrientationMeaning::clockWiseIsInner);
            path.area = calcArea(path);
        }

        std::sort(
          std::begin(m_polyLines),
          std::end(m_polyLines),
          [](auto const & lhs, auto const & rhs)
          {
              if (lhs.contourMode == ContourMode::Outer && rhs.contourMode == ContourMode::Inner)
              {
                  return true;
              }

              if (lhs.contourMode == ContourMode::Inner && rhs.contourMode == ContourMode::Outer)
              {
                  return false;
              }

              return fabs(lhs.area) > fabs(rhs.area);
          });

        for (auto & path : m_polyLines)
        {
            if (path.contourMode == ContourMode::Outer && path.area < 0.f)
            {
                std::reverse(std::begin(path.vertices), std::end(path.vertices));
                path.area = -path.area;
            }
            if (path.contourMode == ContourMode::Inner && path.area > 0.f)
            {
                path.area = -path.area;
            }
        }

        for (auto & path : m_polyLines)
        {
            simplifyPolyline(path, 0.1f);
        }
    }

    Vector2 quadricBezierFunc(const Vector2 & start,
                              const Vector2 & end,
                              const Vector2 & controlPoint,
                              float k)
    {
        const Vector2 a = start + (controlPoint - start) * k;
        const Vector2 b = controlPoint + (end - controlPoint) * k;
        Vector2 p = a + (b - a) * k;
        return p;
    }

    float calcArea(const PolyLine & polyLine)
    {
        if (polyLine.vertices.size() < 2)
        {
            return 0.;
        }

        float area{0};
        for (auto it = std::begin(polyLine.vertices); it != std::prev(std::end(polyLine.vertices));
             ++it)
        {
            const auto nextIt = std::next(it);
            area += it->x() * nextIt->y() - nextIt->x() * it->y();
        }
        const auto first = polyLine.vertices.front();
        const auto last = polyLine.vertices.back();
        area += first.x() * last.y() - last.x() * first.y();

        area *= 0.5f;
        return area;
    }

    float vertexAngle(const Vector2 & a, const Vector2 & b, const Vector2 & c)
    {
        const auto ab = normalize({{b.x() - a.x(), b.y() - a.y()}});
        const auto bc = normalize({{c.x() - b.x(), c.y() - b.y()}});
        return angle(ab, bc);
    }

    void determineContourMode(PolyLine & target, OrientationMeaning orientationMeaning)
    {
        if (!target.isClosed)
        {
            target.contourMode = ContourMode::OpenLine;
            return;
        }
        if (target.vertices.size() < 3)
        {
            target.contourMode = ContourMode::ExcludeFromSlice;
            return;
        }

        auto area = calcArea(target);

        if (orientationMeaning == OrientationMeaning::clockWiseIsOuter)
        {
            if (area < 0.f)
            {
                target.contourMode = ContourMode::Inner;
            }
            else
            {
                target.contourMode = ContourMode::Outer;
            }
        }
        else
        {
            if (area < 0.f)
            {
                target.contourMode = ContourMode::Outer;
            }
            else
            {
                target.contourMode = ContourMode::Inner;
            }
        }
    }

    void simplifyPolyline(PolyLine & polyline, float tolerance)
    {
        std::deque<float> vertices;
        std::vector<float> result;

        for (const auto & inputVert : polyline.vertices)
        {
            vertices.push_back(inputVert.x());
            vertices.push_back(inputVert.y());
        }

        psimpl::simplify_douglas_peucker<2>(
          std::begin(vertices), std::end(vertices), tolerance, std::back_inserter(result));

        auto constexpr sliFileVertexLimit = std::numeric_limits<unsigned int>::max() - 4u;
        if (result.size() >= sliFileVertexLimit)
        {
            result.clear();
            psimpl::simplify_douglas_peucker_n<2>(std::begin(vertices),
                                                  std::end(vertices),
                                                  sliFileVertexLimit,
                                                  std::back_inserter(result));
        }

        polyline.vertices.clear();

        for (auto iter = std::begin(result); iter != std::end(result); ++iter)
        {
            auto x = *iter;
            ++iter;
            auto y = *iter;
            polyline.vertices.push_back(Vector2{x, y});
        }
    }
}
