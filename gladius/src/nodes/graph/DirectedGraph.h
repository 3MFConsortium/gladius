#pragma once
#include "IDirectedGraph.h"

namespace gladius::nodes::graph
{
    class DirectedGraph : public IDirectedGraph
    {
      public:
        explicit DirectedGraph(std::size_t size);
        void addDependency(Identifier id, Identifier idOfDependency) override;
        void removeDependency(Identifier id, Identifier idOfDependency) override;
        [[nodiscard]] auto isDirectlyDependingOn(Identifier id,
                                                 Identifier dependencyInQuestion) const
          -> bool override;

        [[nodiscard]] auto getSize() const -> std::size_t override;

        void removeVertex(Identifier id) override;

        [[nodiscard]] auto getVertices() const -> const DependencySet & override;
        void addVertex(Identifier id) override;

      private:
        std::vector<bool> m_graphData;
        std::size_t m_size;

        DependencySet m_vertices; // Possible performance improvement: We could try out a std::set

        using PredecessorList = std::vector<std::size_t>;
        std::vector<PredecessorList> m_predecessors;

        // Inherited via IDirectedGraph
        [[nodiscard]] auto hasPredecessors(Identifier id) const -> bool override;
    };
} // namespace gladius::nodes::graph
