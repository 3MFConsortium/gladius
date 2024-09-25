#include "DirectedGraph.h"

#include <algorithm>

namespace gladius::nodes::graph
{
    DirectedGraph::DirectedGraph(size_t const size) :
        IDirectedGraph(size),
        m_graphData(size * size, false),
        m_size(size),
        m_predecessors(size)
    {
    }

    void DirectedGraph::addDependency(Identifier id, Identifier idOfDependency)
    {
        addVertex(id);
        addVertex(idOfDependency);
        if (id == idOfDependency)
        {
            return;
        }
        auto const index = id * m_size + idOfDependency;
        m_graphData[index] = true;

        if (std::find(std::begin(m_predecessors[id]), std::end(m_predecessors[id]), idOfDependency) == std::end(m_predecessors[id]))
        {
            m_predecessors[id].push_back(idOfDependency);
        }
    }

    void DirectedGraph::removeDependency(Identifier id, Identifier idOfDependency)
    {
        auto const index = id * m_size + idOfDependency;
        m_graphData[index] = false;

        auto const iterElemToRemove = std::find(std::begin(m_predecessors[id]), std::end(m_predecessors[id]), idOfDependency);
        if (iterElemToRemove != std::end(m_predecessors[id]))
        {
            m_predecessors[id].erase(iterElemToRemove);
        }
    }

    auto DirectedGraph::isDirectlyDependingOn(Identifier id, Identifier dependencyInQuestion) const -> bool
    {
        auto const index = id * m_size + dependencyInQuestion;
        return m_graphData[index];
    }

    auto DirectedGraph::getSize() const -> size_t
    {
        return m_size;
    }

    void DirectedGraph::removeVertex(Identifier id)
    {
        auto const iterElemToRemove = std::find(std::begin(m_vertices), std::end(m_vertices), id);
        if (iterElemToRemove == std::end(m_vertices))
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

    auto DirectedGraph::getVertices() const -> const DependencySet&
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
