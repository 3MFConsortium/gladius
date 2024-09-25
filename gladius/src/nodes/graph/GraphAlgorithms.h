#pragma once

#include "IDirectedGraph.h"
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace gladius::nodes::graph
{
    using Depth = int;

    struct BfsItem
    {
        Identifier identifier{};
        int depth{};
    };

    using DepthMap = std::unordered_map<Identifier, Depth>;

    auto determineDirectDependencies(const IDirectedGraph & graph, Identifier id) -> DependencySet;

    auto determineAllDependencies(const IDirectedGraph & graph, Identifier id) -> DependencySet;

    auto determineSuccessor(const IDirectedGraph & graph, Identifier predecessor) -> VertexList;

    auto graphToString(const IDirectedGraph & graph) -> std::string;

    auto graphToGraphVizStr(const IDirectedGraph & graph) -> std::string;

    auto isDependingOn(const IDirectedGraph & graph, Identifier id, Identifier dependencyInQuestion)
      -> bool;

    auto addDependencyIfConflictFree(IDirectedGraph & graph,
                                     Identifier id,
                                     Identifier idOfDependency) -> bool;

    auto topologicalSort(const IDirectedGraph & graph) -> VertexList;

    auto determineDepth(const IDirectedGraph & graph, Identifier start) -> DepthMap;

    auto inDegreeZeroVertices(const IDirectedGraph & graph) -> VertexList;

    auto isCyclic(const IDirectedGraph & graph) -> bool;
} // namespace gladius::nodes::graph
