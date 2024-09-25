#include "ContourExtractor.h"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <vector>

#include <CL/cl_platform.h>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/util/Constants.h>
#include <fmt/format.h>

#include "Contour.h"
#include "ContourValidator.h"
#include "PathBuilder.h"

namespace gladius
{
    nodes::SliceParameter contourOnlyParameter()
    {
        nodes::SliceParameter sliceParameter;
        return sliceParameter;
    }

    float roundTo(float value, float layerThickness)
    {
        auto alignedValue = value + layerThickness * 0.5f;
        alignedValue -= fmod(alignedValue, layerThickness);
        return alignedValue;
    }

    ContourExtractor::ContourExtractor(events::SharedLogger logger)
        : m_logger(std::move(logger))
    {
    }

    auto ContourExtractor::getContour() const -> const PolyLines &
    {
        return m_closedContours;
    }

    auto ContourExtractor::getContour() -> PolyLines &
    {
        return m_closedContours;
    }

    auto ContourExtractor::getOpenContours() -> PolyLines &
    {
        return m_openContours;
    }

    void ContourExtractor::includeOpenContours()
    {
        for (auto & polyline : m_openContours)
        {
            polyline.contourMode = ContourMode::OpenLine;
        }
        m_closedContours.insert(std::end(m_closedContours),
                                std::make_move_iterator(std::begin(m_openContours)),
                                std::make_move_iterator(std::end(m_openContours)));
        m_openContours.clear();
    }

    void ContourExtractor::setSimplificationTolerance(float tol)
    {
        m_simplificationTolerance = tol;
    }

    void ContourExtractor::runPostProcessing()
    {
        simplify();
        updateContourMode();
        measureQuality();
    }

    SourceVertices const & ContourExtractor::getSourceVertices() const
    {
        return m_sourceVertices;
    }

    NormalVectors const & ContourExtractor::getNormals() const
    {
        return m_normals;
    }

    SliceQuality const & ContourExtractor::getSliceQuality() const
    {
        return m_quality;
    }

    using Direction = Eigen::Vector2i;
    Direction directionFromState(char state, Direction previousDirection)
    {

        switch (state)
        {
        case 1:
            // Up
            // 1 |
            //-------
            //   |
            return {0, -1};
        case 2:
        case 3:
            // Right
            //   | 2
            //-------
            //   |

            // Right
            // 1 | 2
            //-------
            //   |
            return {1, 0};
        case 4:
            // Left
            //   |
            //-------
            // 4 |
            return {-1, 0};
        case 5:
            // Up
            //  1 |
            //-------
            //  4 |
            return {0, -1};
        case 6:
            //   | 2
            //-------
            // 4 |

            if (previousDirection == Direction{0, -1})
            {
                // Left
                return {-1, 0};
            }
            // Right
            return {1, 0};
        case 7:
            // 1 | 2
            //-------
            // 4 |

            // Right
            return {1, 0};
        case 8:
            //  |
            //-------
            //  | 8
            // Down
            return {0, 1};
        case 9:
            // 1 |
            //-------
            //   | 8

            if (previousDirection == Direction{1, 0})
            {
                // Up
                return {0, -1};
            }
            // Down
            return {0, 1};
        case 10:
        case 11:
            //  | 2
            //-------
            //  | 8
            // or
            // 1 | 2
            //-------
            //   | 8
            // Down
            return {0, 1};
        case 12:
            //   |
            //-------
            // 4 | 8

            // Left
            return {-1, 0};
        case 13:
            // 1 |
            //-------
            // 4 | 8

            // Up
            return {0, -1};
        case 14:
            //   | 2
            //-------
            // 4 | 8

            // Left
            return {-1, 0};
        default:
            // 1 | 2
            //-------
            // 4 | 8
            // or
            // 0 | 0
            //-------
            // 0 | 0
            // None
            return {0, 0};
        }
    }

    using Coordinates = Eigen::Vector2i;

    Direction getDirection(Coordinates coord,
                           Direction previousDirection,
                           MarchingSquaresStates const & marchingSquareStates)
    {
        return directionFromState(marchingSquareStates.getValue(coord.x(), coord.y()),
                                  previousDirection);
    }

    std::optional<Coordinates> findStart(MarchingSquaresStates const & marchingSquareStates,
                                         Coordinates previousStart)
    {

        for (size_t y = previousStart.y(); y < marchingSquareStates.getHeight(); ++y)
        {
            for (size_t x = 0u; x < marchingSquareStates.getWidth(); ++x)
            {
                auto state = marchingSquareStates.getValue(x, y);
                if (state > 1 && state < 15 && state != 6 &&
                    state != 9) // starting at the saddle point would be ambigious because we
                                // don'tknow the previous direction yet
                {
                    return Coordinates{x, y};
                }
            }
        }

        return {};
    }

    std::optional<Vector2> toWorldPos(Vector2 const & coord,
                                      MarchingSquaresStates const & marchingSquareStates,
                                      float4 const & clippingArea)
    {
        if (coord.x() > static_cast<float>(marchingSquareStates.getWidth()) ||
            coord.y() > static_cast<float>(marchingSquareStates.getHeight()))
        {
            return {};
        }

        auto const width = clippingArea.z - clippingArea.x;
        auto const height = clippingArea.w - clippingArea.y;

        if (fabs(width) < FLT_EPSILON || fabs(height) < FLT_EPSILON)
        {
            return {};
        }

        auto const cellWidth = width / static_cast<float>(marchingSquareStates.getWidth());
        auto const cellHeight = height / static_cast<float>(marchingSquareStates.getHeight());

        return Vector2{clippingArea.x + cellWidth * coord.x(),
                       clippingArea.y + cellHeight * coord.y()};
    }

    void
    ContourExtractor::addIsoLineFromMarchingSquare(MarchingSquaresStates & marchingSquareStates,
                                                   float4 const & clippingArea)
    {
        m_sourceVertices.clear();

        auto start = findStart(marchingSquareStates, {0, 0});
        while (start.has_value())
        {
            PolyLine newContour;
            auto startVertex = toWorldPos(
              Vector2(start.value().x(), start.value().y()), marchingSquareStates, clippingArea);
            if (!startVertex.has_value())
            {
                throw std::runtime_error("Cannot add an empty point to a polyline");
            }
            newContour.vertices.push_back(startVertex.value());
            // marching
            Direction previousDirection = {};
            Coordinates currentPos = start.value();

            do
            {
                auto const state = marchingSquareStates.getValue(currentPos.x(), currentPos.y());
                auto direction = directionFromState(state, previousDirection);
                if (direction == Direction{0, 0})
                {
                    break;
                }

                auto vertex = toWorldPos(Vector2(currentPos.x(), currentPos.y()) +
                                           Vector2(static_cast<float>(direction.x()) * 0.5f,
                                                   static_cast<float>(direction.y()) * 0.5f),
                                         marchingSquareStates,
                                         clippingArea);
                if (!vertex.has_value())
                {
                    throw std::runtime_error("Cannot add an empty point to a polyline");
                }

                newContour.vertices.push_back(vertex.value());

                if (state != 6 &&
                    state != 9) // we need to be able to run through saddle points twice
                {
                    marchingSquareStates.setValue(currentPos.x(),
                                                  currentPos.y(),
                                                  0); // clear the pixel, so we cannot use it again
                }

                currentPos += direction;
                previousDirection = direction;
            } while (currentPos != start.value());

            if (newContour.vertices.size() > 2) // we need at least 3 points to form a triangle
            {
                auto const stateStart =
                  marchingSquareStates.getValue(start.value().x(), start.value().y());
                if (stateStart != 6 &&
                    stateStart != 9) // we need to be able to run through saddle points twice
                {
                    marchingSquareStates.setValue(start.value().x(),
                                                  start.value().y(),
                                                  0); // clear the pixel, so we cannot use it again
                }

                std::reverse(newContour.vertices.begin(), newContour.vertices.end());
                closePolyLineIfPossible(newContour, 0.2f);
                if (newContour.isClosed)
                {
                    m_closedContours.emplace_back(std::move(newContour));
                }
                else
                {
                    m_openContours.emplace_back(std::move(newContour));
                }
            }
            start = findStart(marchingSquareStates, start.value());
        }

        mergeOpenContoursWithNearestNeighbor();
        calcAreas();
    }

    void ContourExtractor::clear()
    {
        m_closedContours.clear();
        m_openContours.clear();
    }

    auto isMergable(const Vector2 & start, const Vector2 & end, float allowedGapSize) -> bool
    {
        auto distEndToStart = (start - end).norm();
        return distEndToStart <= allowedGapSize;
    }

    auto isClosed(const Vector2 & start, const Vector2 & end) -> bool
    {
        auto sqDistEndToStart = (start - end).squaredNorm();
        auto constexpr sqAllowedGapSize = 1E-6f;
        return sqDistEndToStart <= sqAllowedGapSize;
    }

    void
    mergePolyLinesIfPossible(PolyLine & target, PolyLine & toBeConcatenated, float allowedGapSize)
    {
        if (target.vertices.empty() || toBeConcatenated.vertices.empty())
        {
            return;
        }

        if ((target.vertices.back() - toBeConcatenated.vertices.front()).norm() > allowedGapSize)
        {
            return;
        }

        target.vertices.insert(target.vertices.end(),
                               std::make_move_iterator(toBeConcatenated.vertices.begin()),
                               std::make_move_iterator(toBeConcatenated.vertices.end()));
        toBeConcatenated.vertices.clear();
    }

    void closePolyLineIfPossible(gladius::PolyLine & poly, float allowedGapSize)
    {
        if (isMergable(poly.vertices.front(), poly.vertices.back(), allowedGapSize))
        {
            if (contour::endCrossesStart(poly))
            {
                poly.vertices.erase(poly.vertices.begin());
            }

            if (!isClosed(poly.vertices.front(), poly.vertices.back()))
            {
                poly.vertices.push_back(poly.vertices.front());
            }
            poly.isClosed = true;
        }
    }

    void ContourExtractor::mergeOpenContoursWithNearestNeighbor()
    {
        if (m_openContours.empty())
        {
            return;
        }

        struct NearestNbResult
        {
            PolyLines::iterator iter{};
            float dist{};
        };

        auto const findNearestNeighbor = [&](PolyLines::iterator iterPos)
        {
            auto minDist = std::numeric_limits<float>::max();
            auto minIter = std::end(m_openContours);
            if (iterPos->vertices.empty())
            {
                return NearestNbResult{minIter, minDist};
            }
            for (auto iterCandidate = std::begin(m_openContours);
                 iterCandidate != std::end(m_openContours);
                 ++iterCandidate)
            {
                if (iterCandidate->vertices.empty())
                {
                    continue;
                }
                auto const dist =
                  (iterCandidate->vertices.front() - iterPos->vertices.back()).norm();

                if (dist < minDist)
                {
                    minDist = dist;
                    minIter = iterCandidate;
                }
            }
            return NearestNbResult{minIter, minDist};
        };

        auto constexpr toleratedDistanceForClosing = 1.f;

        auto const iterationLimit = m_openContours.size() + 1u;
        auto i = 0u;

        while (!m_openContours.empty() && ++i < iterationLimit)
        {
            for (auto iter = std::begin(m_openContours); iter != std::end(m_openContours); ++iter)
            {
                auto nearestNb = findNearestNeighbor(iter);
                if (nearestNb.iter == std::end(m_openContours))
                {
                    continue;
                }
                if (iter == nearestNb.iter)
                {
                    closePolyLineIfPossible(*iter, toleratedDistanceForClosing);
                }
                else
                {
                    mergePolyLinesIfPossible(*iter, *nearestNb.iter, toleratedDistanceForClosing);
                }
                if (iter->isClosed)
                {
                    contour::validate(*iter);
                    m_quality.selfIntersections += iter->selfIntersections.size();
                    m_closedContours.emplace_back(std::move(*iter));
                    *iter = PolyLine();
                }
            }

            m_openContours.erase(std::remove_if(m_openContours.begin(),
                                                m_openContours.end(),
                                                [](auto & polyLine) -> bool
                                                { return polyLine.isClosed; }),
                                 m_openContours.end());
        }
    }

    void ContourExtractor::closeRemainingPolyLines()
    {
        PolyLines remaining;
        for (auto iter = std::begin(m_openContours); iter != std::end(m_openContours); ++iter)

        {
            if ((iter->vertices.size() > 2) &&
                isMergable(iter->vertices.front(), iter->vertices.back(), 0.5f))
            {
                iter->vertices.push_back(iter->vertices.front());
                iter->isClosed = true;
                determineContourMode(*iter);
                m_closedContours.emplace_back(std::move(*iter));
                *iter = PolyLine();
            }
            else
            {
                remaining.emplace_back(std::move(*iter));
            }
        }

        std::swap(m_openContours, remaining);
    }

    void ContourExtractor::markToSmallAreasForExclusion()
    {
        auto numberOfExcludedContours = 0;
        auto constexpr minArea = 0.05f; // [mm2]
        for (auto & polyline : m_closedContours)
        {
            polyline.area = calcArea(polyline);
            if (fabsf(polyline.area) < minArea)
            {
                polyline.contourMode = ContourMode::ExcludeFromSlice;
                ++numberOfExcludedContours;
            }
        }
        m_quality.ignoredPolyLines = numberOfExcludedContours;
        if (numberOfExcludedContours)
        {
            m_logger->addEvent(
              {fmt::format(
                 "{} polylines are smaller than {} mm^2 and are thereby excluded from the slice",
                 numberOfExcludedContours,
                 minArea),
               events::Severity::Warning});
        }
    }

    void ContourExtractor::calcAreas()
    {
        for (auto & polyline : m_closedContours)
        {
            polyline.area = calcArea(polyline);
        }
    }

    void ContourExtractor::updateContourMode()
    {
        for (auto & polyline : m_closedContours)
        {
            determineContourMode(polyline);
        }
    }

    void ContourExtractor::simplify()
    {
        if (m_simplificationTolerance == 0.f)
        {
            return;
        }

        for (auto & polyline : m_closedContours)
        {
            simplifyPolyline(polyline, m_simplificationTolerance);
        }

        for (auto & polyline : m_openContours)
        {
            simplifyPolyline(polyline, m_simplificationTolerance);
        }
    }

    void ContourExtractor::measureQuality()
    {
        m_quality.initiallyOpenPolygons = m_openContours.size();
        m_quality.unusedVertices = 0u;
        m_quality.consideredNumberOfVertices = 0u;
        for (auto & poly : m_openContours)
        {
            poly.hasIntersections = !contour::validate(poly).intersectionFree;
            m_quality.selfIntersections += poly.selfIntersections.size();

            m_quality.openPolyLinesThatCouldNotBeClosed++;
        }

        for (auto & poly : m_closedContours)
        {
            poly.hasIntersections = !contour::validate(poly).intersectionFree;
            m_quality.selfIntersections += poly.selfIntersections.size();
            m_quality.enclosedArea += fabs(calcArea(poly));
            m_quality.consideredNumberOfVertices += poly.vertices.size();

            if (!poly.isClosed && (poly.contourMode == ContourMode::OpenLine))
            {
                m_quality.openPolyLinesThatCouldNotBeClosed++;
            }
        }

        m_quality.unusedVertices =
          m_quality.expectedNumberOfVertices -
          std::min(
            m_quality.consideredNumberOfVertices,
            m_quality.expectedNumberOfVertices); // consideredNumberOfVertices might be greater
                                                 // than expectedNumberOfVertices when vertices
                                                 // are connected multiple times

        m_quality.closedPolyLines = m_closedContours.size();
#ifdef _DEBUG
        m_logger->addEvent(
          {fmt::format("{} vertices of {} unused;\t {} "
                       "self intersections; initially {} open polylines; {} remaining open "
                       "polylines, {} excluded polylines ; area: {}",
                       m_quality.unusedVertices,
                       m_quality.expectedNumberOfVertices,
                       m_quality.selfIntersections,
                       m_quality.initiallyOpenPolygons,
                       m_quality.openPolyLinesThatCouldNotBeClosed,
                       m_quality.ignoredPolyLines,
                       m_quality.enclosedArea),
           (m_quality.unusedVertices > 0u) ? events::Severity::Warning : events::Severity::Info});
#endif
    }
}
