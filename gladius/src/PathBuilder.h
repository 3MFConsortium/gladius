#pragma once

#include "Contour.h"
#include "Primitives.h"

namespace gladius
{
    class PathBuilder
    {
      public:
        PathBuilder();
        void moveTo(Vector2 point);
        void lineTo(Vector2 point);
        void quadricBezier(Vector2 point, Vector2 controlPoint);
        void closePath();

        void addToPrimitives(PrimitiveBuffer & primitives);

      private:
        PolyLines m_polyLines;
        PolyLine m_currentPolyLine;
        void flush();
        void determineContourModiAndSort();
        Vector2 m_start;
    };
    Vector2 quadricBezierFunc(Vector2 const & start,
                              Vector2 const & end,
                              Vector2 const & controlPoint,
                              float k);
    float calcArea(const PolyLine & polyLine);
    float vertexAngle(Vector2 const & a, Vector2 const & b, Vector2 const & c);

    enum class OrientationMeaning
    {
        clockWiseIsInner,
        clockWiseIsOuter
    };

    void determineContourMode(
      PolyLine & target,
      OrientationMeaning orientationMeaning = OrientationMeaning::clockWiseIsOuter);

    void simplifyPolyline(PolyLine & polyline, float tolerance);
}