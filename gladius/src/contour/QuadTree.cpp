#include "QuadTree.h"

#include <iostream>
#include <utility>

namespace gladius::contour
{
    Quad::Quad(Rect rect, std::optional<Quad *> parent)
        : m_rect(std::move(rect))
        , m_parent(parent)
    {
    }

    bool Quad::insert(PointWithNormal const & point)
    {
        if (!isInside(point.position))
        {
            return false;
        }

        if (!m_point.has_value() && isLeaf())
        {
            setPoint(point);
            return true;
        }

        if (!m_hasChildren)
        {
            split();
        }
        insertToChild(point);
        return true;
    }

    void Quad::printRects()
    {
        std::cout << "Splitting rect from " << m_rect.startPos.transpose() << " to "
                  << m_rect.endPos.transpose() << " into\n\n";
        for (auto const & child : m_children)
        {
            std::cout << child->getRect().startPos.transpose() << " to "
                      << child->getRect().endPos.transpose() << "\n";
        }
    }

    void Quad::split()
    {
        Vector2 const center = (m_rect.startPos + m_rect.endPos) * 0.5f;
        m_children[0] = std::make_unique<Quad>(Rect{m_rect.startPos, center}, this);
        m_children[1] = std::make_unique<Quad>(
          Rect{Vector2{center.x(), m_rect.startPos.y()}, Vector2{m_rect.endPos.x(), center.y()}},
          this);
        m_children[2] = std::make_unique<Quad>(
          Rect{Vector2{m_rect.startPos.x(), center.y()}, Vector2{center.x(), m_rect.endPos.y()}},
          this);
        m_children[3] = std::make_unique<Quad>(
          Rect{Vector2{center.x(), center.y()}, Vector2{m_rect.endPos.x(), m_rect.endPos.y()}},
          this);
#ifdef DEBUG
        printRects();
#endif

        m_hasChildren = true;
        if (m_point.has_value())
        {
            insertToChild(m_point.value());
            m_point.reset();
        }
    }

    void Quad::insertToChild(PointWithNormal const & point) const
    {
        for (auto const & child : m_children)
        {
            if (child->insert(point))
            {
                return;
            }
        }

        throw std::runtime_error("Inserting point to quad tree failed");
    }

    void Quad::setPoint(PointWithNormal const & point)
    {
        m_point = point;
    }

    bool Quad::isInside(gladius::Vector2 const & point) const
    {
        return (point.x() >= m_rect.startPos.x()) && //
               (point.x() < m_rect.endPos.x()) &&    //
               (point.y() >= m_rect.startPos.y()) && //
               (point.y() < m_rect.endPos.y());
    }

    ChildNodes const & Quad::getChildNodes() const
    {
        return m_children;
    }

    bool Quad::isLeaf() const
    {
        return !m_hasChildren;
    }

    Rect const & Quad::getRect() const
    {
        return m_rect;
    }

    OptionalPoint Quad::getPoint() const
    {
        return m_point;
    }

    void Quad::removePoint()
    {
        m_point = {};
        if (m_parent.has_value())
        {
            m_parent.value()->removeEmptyChildren();
        }
    }

    void Quad::removeEmptyChildren()
    {
        if (isLeaf())
        {
            return;
        }
        for (auto const & child : m_children)
        {
            if (!child)
            {
                continue;
            }
            if (!child->isLeaf() || child->getPoint().has_value())
            {
                return;
            }
        }

        for (auto & child : m_children)
        {
            child.reset();
        }
    }

    bool areRectsIntersecting(Rect const & a, Rect const & b)
    {
        auto const widthA = a.endPos.x() - a.startPos.x();
        auto const widthB = b.endPos.x() - b.startPos.x();

        auto const heightA = a.endPos.y() - a.startPos.y();
        auto const heightB = b.endPos.y() - b.startPos.y();

        auto const centerA = (a.startPos + a.endPos) * 0.5f;
        auto const centerB = (a.startPos + a.endPos) * 0.5f;

        return (fabs(centerA.x() - centerB.x()) * 2.f < (widthA + widthB)) && //
               (fabs(centerA.y() - centerB.y()) * 2.f < (heightA + heightB));
    }

    QuadTree::QuadTree(Rect rect)
        : m_rootQuad{rect, nullptr}
    {
    }

    void QuadTree::insert(PointWithNormal const & point)
    {
        if (!m_rootQuad.insert(point))
        {
            throw std::domain_error("cannot insert point outside of domain");
        }
    }

    std::optional<Quad *> QuadTree::find(Vector2 const & position) const
    {
        return find(position, &m_rootQuad);
    }

    OptionalPoint QuadTree::findNearestNeighbor(Vector2 const & position) const
    {
        auto const & quadContainingPos = find(position);
        if (!quadContainingPos.has_value())
        {
            return {};
        }

        auto pointInCurrentQuad = quadContainingPos.value()->getPoint();
        if (!pointInCurrentQuad.has_value() && quadContainingPos.value()->isLeaf())
        {
            return {};
        }

        auto const distance = (pointInCurrentQuad.value().position - position).norm();

        // Search for closer points
        auto const otherPoints = findNeighbors(position, distance);

        if (otherPoints.empty())
        {
            return pointInCurrentQuad;
        }

        auto const closestPoint = std::min_element(
          otherPoints.begin(),
          otherPoints.end(),
          [&position](auto const & a, auto const & b)
          {
              return (a.position - position).squaredNorm() < (b.position - position).squaredNorm();
          });

        return *closestPoint;
    }

    Points QuadTree::findNeighbors(Vector2 const & position, float maxDistance) const
    {
        Rect const searchRect{position - Vector2(maxDistance, maxDistance),
                              position + Vector2(maxDistance, maxDistance)};
        Points neighbors;
        findNeighbors(searchRect, &m_rootQuad, neighbors);
        return neighbors;
    }

    void QuadTree::remove(PointWithNormal const & point)
    {
        auto quad = find(point.position);
        if (!quad.has_value() || !quad.value()->getPoint().has_value())
        {
            return;
        }

        if (quad.value()->getPoint().value().position == point.position)
        {
            quad.value()->removePoint();
        }
    }

    OptionalPoint QuadTree::getAnyPoint() const
    {
        return getAnyPointImpl(m_rootQuad);
    }

    std::optional<Quad *> QuadTree::find(Vector2 const & position, Quad const * currentQuad) const
    {
        for (auto const & child : currentQuad->getChildNodes())
        {
            if (!child)
            {
                continue;
            }

            if (child->isInside(position))
            {
                if (child->isLeaf())
                {
                    return {child.get()};
                }

                auto const leaf = find(position, child.get());
                if (leaf.has_value())
                {
                    return leaf.value();
                }
            }
        }

        return {};
    }

    OptionalPoint QuadTree::getAnyPointImpl(Quad const & quad) const
    {
        if (quad.isLeaf() && quad.getPoint().has_value())
        {
            return quad.getPoint();
        }

        for (auto const & child : quad.getChildNodes())
        {
            if (!child)
            {
                continue;
            }
            auto point = getAnyPointImpl(*child);
            if (point.has_value())
            {
                return point;
            }
        }

        return {};
    }

    void QuadTree::findNeighbors(Rect const & searchRect,
                                 Quad const * const currentQuad,
                                 Points & candidates) const
    {
        for (auto const & child : currentQuad->getChildNodes())
        {
            if (!child)
            {
                continue;
            }

            if (areRectsIntersecting(searchRect, child->getRect()))
            {
                if (child->isLeaf() && child->getPoint().has_value())
                {
                    if (searchRect.isInside(child->getPoint().value().position))
                    {
                        candidates.push_back(child->getPoint().value());
                    }
                    continue;
                }

                findNeighbors(searchRect, child.get(), candidates);
            }
        }
    }
}
