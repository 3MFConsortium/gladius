#pragma once
#include "IDirectedGraph.h"
#include <unordered_map>
#include <vector>

namespace gladius::nodes::graph
{
    /**
     * @brief An implementation of IDirectedGraph using adjacency lists
     * 
     * This implementation is more memory efficient than DirectedGraph for sparse graphs
     * as it only stores the edges that exist rather than all possible edges.
     */
    class AdjacencyListDirectedGraph : public IDirectedGraph
    {
      public:
        /**
         * @brief Constructs a new empty Adjacency List Directed Graph
         */
        AdjacencyListDirectedGraph();
        
        /**
         * @brief Constructs a new Adjacency List Directed Graph
         * 
         * @param size Initial capacity for the graph (doesn't limit vertex count)
         */
        explicit AdjacencyListDirectedGraph(std::size_t size);

        /**
         * @brief Adds a dependency between two vertices
         * 
         * @param id The vertex that depends on idOfDependency
         * @param idOfDependency The vertex that id depends on
         */
        void addDependency(Identifier id, Identifier idOfDependency) override;

        /**
         * @brief Removes a dependency between two vertices
         * 
         * @param id The vertex that no longer depends on idOfDependency
         * @param idOfDependency The vertex that id no longer depends on
         */
        void removeDependency(Identifier id, Identifier idOfDependency) override;

        /**
         * @brief Checks if a vertex directly depends on another
         * 
         * @param id The vertex to check dependencies for
         * @param dependencyInQuestion The potential dependency to check
         * @return true if id directly depends on dependencyInQuestion
         * @return false otherwise
         */
        [[nodiscard]] auto isDirectlyDependingOn(Identifier id,
                                                 Identifier dependencyInQuestion) const
            -> bool override;

        /**
         * @brief Gets the maximum vertex identifier
         * 
         * @return The size of the graph
         */
        [[nodiscard]] auto getSize() const -> std::size_t override;

        /**
         * @brief Removes a vertex and all its dependencies
         * 
         * @param id The vertex to remove
         */
        void removeVertex(Identifier id) override;

        /**
         * @brief Gets all vertices in the graph
         * 
         * @return A const reference to the set of vertices
         */
        [[nodiscard]] auto getVertices() const -> const DependencySet & override;

        /**
         * @brief Adds a vertex to the graph
         * 
         * @param id The vertex to add
         */
        void addVertex(Identifier id) override;

      private:
        /**
         * @brief Checks if a vertex has predecessors
         * 
         * @param id The vertex to check
         * @return true if the vertex has predecessors
         * @return false otherwise
         */
        [[nodiscard]] auto hasPredecessors(Identifier id) const -> bool override;

        /// Set of all vertices in the graph
        DependencySet m_vertices;

        /// Adjacency list - maps each vertex to the set of vertices it depends on
        std::unordered_map<Identifier, DependencySet> m_outgoingEdges;

        /// Reverse adjacency list - maps each vertex to the set of vertices that depend on it
        std::unordered_map<Identifier, DependencySet> m_incomingEdges;
    };
} // namespace gladius::nodes::graph
