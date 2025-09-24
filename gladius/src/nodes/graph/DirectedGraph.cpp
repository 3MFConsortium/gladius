#include "DirectedGraph.h"

#include <algorithm>
#include <unordered_set>

namespace gladius::nodes::graph
{
    DirectedGraph::DirectedGraph(size_t const size)
        : IDirectedGraph(size)
        , m_graphData(size * size, false)
        , m_size(size)
        , m_predecessors(size)
    {
    }

    void DirectedGraph::addDependency(Identifier id, Identifier idOfDependency)
    {
        if (id == idOfDependency)
        {
            return;
        }

        addVertex(id);
        addVertex(idOfDependency);

        auto const index = id * m_size + idOfDependency;
        if (!m_graphData[index])
        {
            m_graphData[index] = true;
            m_predecessors[id].push_back(idOfDependency);
        }
    }

    void DirectedGraph::removeDependency(Identifier id, Identifier idOfDependency)
    {
        auto const index = id * m_size + idOfDependency;
        if (m_graphData[index])
        {
            m_graphData[index] = false;
            auto & preds = m_predecessors[id];
            preds.erase(std::remove(preds.begin(), preds.end(), idOfDependency), preds.end());
        }
    }

    auto DirectedGraph::isDirectlyDependingOn(Identifier id, Identifier dependencyInQuestion) const
      -> bool
    {
        auto const index = id * m_size + dependencyInQuestion;
        return m_graphData[index];
    }

    auto DirectedGraph::getSize() const -> size_t
    {
        return m_size;
    }

    auto DirectedGraph::isInRange(Identifier id) const -> bool
    {
        return id >= 0 && id < static_cast<Identifier>(m_size);
    }

    void DirectedGraph::removeVertex(Identifier id)
    {
        auto const iterElemToRemove = m_vertices.find(id);
        if (iterElemToRemove == m_vertices.end())
        {
            return;
        }
        m_vertices.erase(iterElemToRemove);

        for (auto vertex : m_vertices)
        {
            removeDependency(id, vertex);
            removeDependency(vertex, id);
        }
    }

    auto DirectedGraph::getVertices() const -> const DependencySet &
    {
        return m_vertices;
    }

    void DirectedGraph::addVertex(Identifier id)
    {
        m_vertices.insert(id);
    }

    auto DirectedGraph::hasPredecessors(Identifier id) const -> bool
    {
        return !m_predecessors[id].empty();
    }
} // namespace gladius::nodes::graph