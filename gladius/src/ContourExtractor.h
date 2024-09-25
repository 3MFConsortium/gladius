#pragma once

#include "Buffer.h"
#include "nodes/BuildParameter.h"

#include "Contour.h"
#include "EventLogger.h"
#include "ImageRGBA.h"
#include "SlicerProgram.h"

namespace gladius
{
    using SourceVertices = std::vector<cl_float4>;

    struct NormalWithPos
    {
        Vector2 normal;
        Vector2 position;
        Vector2 successor;
    };

    using NormalVectors = std::vector<NormalWithPos>;

    nodes::SliceParameter contourOnlyParameter();

    float roundTo(float value, float layerThickness);

    struct SliceQuality
    {
        size_t initiallyOpenPolygons = 0u;
        size_t selfIntersections = 0u;
        size_t unusedVertices = 0u;
        size_t ignoredPolyLines = 0u;
        size_t closedPolyLines = 0u;
        size_t expectedNumberOfVertices = 0u;
        size_t consideredNumberOfVertices = 0u;
        size_t openPolyLinesThatCouldNotBeClosed = 0u;
        float enclosedArea = 0.f;
    };

    class ContourExtractor
    {
      public:
        explicit ContourExtractor(events::SharedLogger logger);

        ContourExtractor(const ContourExtractor & other) = delete;

        [[nodiscard]] const PolyLines & getContour() const;
        [[nodiscard]] PolyLines & getContour();

        [[nodiscard]] PolyLines & getOpenContours();

        void addIsoLineFromMarchingSquare(MarchingSquaresStates & marchingSquareStates,
                                          float4 const & clippingArea);

        void clear();

        void setSimplificationTolerance(float tol);

        [[nodiscard]] const SourceVertices & getSourceVertices() const;

        [[nodiscard]] const NormalVectors & getNormals() const;
        [[nodiscard]] const SliceQuality & getSliceQuality() const;
        void runPostProcessing();

      private:
        void includeOpenContours();

        void mergeOpenContoursWithNearestNeighbor();
        void closeRemainingPolyLines();
        void markToSmallAreasForExclusion();
        void calcAreas();
        void updateContourMode();
        void simplify();
        void measureQuality();

        PolyLines m_closedContours;
        PolyLines m_openContours;

        float m_simplificationTolerance = 1.E-2f;

        bool m_collectVertices = true;

        SourceVertices m_sourceVertices;

        NormalVectors m_normals;

        events::SharedLogger m_logger;

        SliceQuality m_quality;
    };

    void closePolyLineIfPossible(PolyLine & poly, float allowedGapSize);
}
