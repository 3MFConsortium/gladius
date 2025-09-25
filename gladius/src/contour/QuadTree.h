#pragma once
#include "../Contour.h" //TODO move Contour into Contour/

#include <array>
#include <optional>

namespace gladius::contour
{
    class Quad;

    struct PointWithNormal
    {
        Vector2 position;
        Vector2 normal;
    };

    struct Rect
    {
        Vector2 startPos;
        Vector2 endPos;

        bool isInside(Vector2 point) const
        {
            return point.x() >= startPos.x() && point.y() >= startPos.y() &&
                   point.x() <= endPos.x() && point.y() <= endPos.y();
        }
    };

    using QuadTreeNodes = std::vector<PointWithNormal>;
    using Points = std::vector<PointWithNormal>;

    using ChildNodes = std::array<std::unique_ptr<Quad>, 4>;
    using OptionalPoint = std::optional<PointWithNormal>;

    class Quad
    {
      public:
        Quad(Rect rect, std::optional<Quad *> parent);
        bool insert(PointWithNormal const & point);
        void printRects();

        [[nodiscard]] bool isInside(gladius::Vector2 const & point) const;

        [[nodiscard]] ChildNodes const & getChildNodes() const;

        [[nodiscard]] bool isLeaf() const;

        [[nodiscard]] Rect const & getRect() const;

        [[nodiscard]] OptionalPoint getPoint() const;

        void removePoint();

        void removeEmptyChildren();

      private:
        void split();
        void insertToChild(PointWithNormal const & point) const;
        void setPoint(PointWithNormal const & point);

        ChildNodes m_children;

        OptionalPoint m_point;

        Rect m_rect;

        bool m_hasChildren = false;

        std::optional<Quad *> m_parent;
    };

    bool areRectsIntersecting(Rect const & a, Rect const & b);

    class QuadTree
    {
      public:
        explicit QuadTree(Rect rect);
        void insert(PointWithNormal const & point);

        [[nodiscard]] std::optional<Quad *> find(Vector2 const & position) const;

        [[nodiscard]] OptionalPoint findNearestNeighbor(Vector2 const & position) const;

        [[nodiscard]] Points findNeighbors(Vector2 const & position, float maxDistance) const;

        void remove(PointWithNormal const & point);

        /**
         * \brief
         * \return a point, that can be used as start for polyline tracing, empty, if no point could
         * be found.
         */
        [[nodiscard]] OptionalPoint getAnyPoint() const;

      private:
        [[nodiscard]] std::optional<Quad *> find(Vector2 const & position,
                                                 Quad const * currentQuad) const;

        [[nodiscard]] OptionalPoint getAnyPointImpl(Quad const & quad) const;

        void
        findNeighbors(Rect const & searchRect, Quad const * currentQuad, Points & candidates) const;
        Quad m_rootQuad;
    };

}
