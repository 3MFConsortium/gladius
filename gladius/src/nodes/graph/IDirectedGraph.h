#pragma once

#include <unordered_set>
#include <vector>

namespace gladius::nodes::graph
{
    using Identifier = int;
    using DependencySet = std::unordered_set<Identifier>;
    using VertexList = std::vector<Identifier>;

    class IDirectedGraph
    {
      public:
        explicit IDirectedGraph(std::size_t const /*unused*/){};
        virtual ~IDirectedGraph() = default;
        virtual void addDependency(Identifier id, Identifier idOfDependency) = 0;
        virtual void removeDependency(Identifier id, Identifier idOfDependency) = 0;
        [[nodiscard]] virtual auto isDirectlyDependingOn(Identifier id,
                                                         Identifier dependencyInQuestion) const
          -> bool = 0;

        [[nodiscard]] virtual auto getSize() const -> std::size_t = 0;
        [[nodiscard]] virtual bool isInRange(Identifier id) const = 0;
        virtual void removeVertex(Identifier id) = 0;
        virtual void addVertex(Identifier id) = 0;
        [[nodiscard]] virtual auto hasPredecessors(Identifier id) const -> bool = 0;

        [[nodiscard]] virtual auto getVertices() const -> const DependencySet & = 0;
    };
} // namespace gladius::nodes::graph
