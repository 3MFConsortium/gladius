#include "AdjacencyListDirectedGraph.h"
#include <algorithm>

namespace gladius::nodes::graph
{
    AdjacencyListDirectedGraph::AdjacencyListDirectedGraph() :
        IDirectedGraph(0)
    {
    }
    
    AdjacencyListDirectedGraph::AdjacencyListDirectedGraph(std::size_t const size) :
        IDirectedGraph(size)
    {
    }

    void AdjacencyListDirectedGraph::addDependency(Identifier id, Identifier idOfDependency)
    {
        // Self-dependencies are not allowed
        if (id == idOfDependency)
        {
            return;
        }

        // Add vertices if they don't exist
        addVertex(id);
        addVertex(idOfDependency);

        // Add outgoing edge from id to idOfDependency
        m_outgoingEdges[id].insert(idOfDependency);
        
        // Add incoming edge to idOfDependency from id
        m_incomingEdges[idOfDependency].insert(id);
    }

    void AdjacencyListDirectedGraph::removeDependency(Identifier id, Identifier idOfDependency)
    {
        // Remove outgoing edge
        auto outIter = m_outgoingEdges.find(id);
        if (outIter != m_outgoingEdges.end())
        {
            outIter->second.erase(idOfDependency);
        }

        // Remove incoming edge
        auto inIter = m_incomingEdges.find(idOfDependency);
        if (inIter != m_incomingEdges.end())
        {
            inIter->second.erase(id);
        }
    }

    auto AdjacencyListDirectedGraph::isDirectlyDependingOn(Identifier id, Identifier dependencyInQuestion) const -> bool
    {
        auto iter = m_outgoingEdges.find(id);
        if (iter != m_outgoingEdges.end())
        {
            return iter->second.find(dependencyInQuestion) != iter->second.end();
        }
        return false;
    }

    auto AdjacencyListDirectedGraph::getSize() const -> std::size_t
    {
        return m_vertices.size();
    }

    bool AdjacencyListDirectedGraph::isInRange(Identifier id) const
    {
        return id >= 0;
    }

    void AdjacencyListDirectedGraph::removeVertex(Identifier id)
    {
        // Find and remove the vertex
        auto vertexIter = m_vertices.find(id);
        if (vertexIter == m_vertices.end())
        {
            return;
        }
        
        // Remove the vertex from the vertices set
        m_vertices.erase(vertexIter);

        // Find outgoing edges from this vertex and remove them
        auto outIter = m_outgoingEdges.find(id);
        if (outIter != m_outgoingEdges.end())
        {
            // For each outgoing edge, remove corresponding incoming edge
            for (auto dependency : outIter->second)
            {
                auto inIter = m_incomingEdges.find(dependency);
                if (inIter != m_incomingEdges.end())
                {
                    inIter->second.erase(id);
                }
            }
            
            // Remove all outgoing edges from this vertex
            m_outgoingEdges.erase(outIter);
        }

        // Find incoming edges to this vertex and remove them
        auto inIter = m_incomingEdges.find(id);
        if (inIter != m_incomingEdges.end())
        {
            // For each incoming edge, remove corresponding outgoing edge
            for (auto dependent : inIter->second)
            {
                auto outDepIter = m_outgoingEdges.find(dependent);
                if (outDepIter != m_outgoingEdges.end())
                {
                    outDepIter->second.erase(id);
                }
            }
            
            // Remove all incoming edges to this vertex
            m_incomingEdges.erase(inIter);
        }
    }

    auto AdjacencyListDirectedGraph::getVertices() const -> const DependencySet&
    {
        return m_vertices;
    }

    void AdjacencyListDirectedGraph::addVertex(Identifier id)
    {
        m_vertices.insert(id);
    }

    auto AdjacencyListDirectedGraph::hasPredecessors(Identifier id) const -> bool
    {
        auto iter = m_outgoingEdges.find(id);
        return (iter != m_outgoingEdges.end() && !iter->second.empty());
    }
} // namespace gladius::nodes::graph
