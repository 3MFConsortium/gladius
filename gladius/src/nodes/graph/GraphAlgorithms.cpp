#include "GraphAlgorithms.h"
#include "graph/IDirectedGraph.h"
#include "Profiling.h"

#include <list>
#include <queue>
#include <sstream>
#include <stack>
#include <algorithm>

namespace gladius::nodes::graph
{
    auto determineDirectDependencies(const IDirectedGraph & graph, Identifier id) -> DependencySet
    {
        ProfileFunction;
        DependencySet dependencies;
        if (id < 0 || id > static_cast<int>(graph.getSize()))
        {
            return DependencySet();
        }

        for (auto dep : graph.getVertices())
        {
            auto const value = graph.isDirectlyDependingOn(id, dep);
            if (value && dep != id)
            {
                dependencies.insert(dep);
            }
        }
        return dependencies;
    }

    auto determineAllDependencies(const IDirectedGraph & graph, Identifier id) -> DependencySet
    {
        ProfileFunction;
        if (!graph.isInRange(id))
        {
            return DependencySet();
        }
        auto dependencies = DependencySet{id};

        // Breath First Search
        std::vector<bool> visited(graph.getSize(), false);
        std::queue<Identifier> nodesToVisit;

        nodesToVisit.push(id);
        visited[id] = true;

        while (!nodesToVisit.empty())
        {
            auto nextNode = nodesToVisit.front();
            nodesToVisit.pop();
            dependencies.insert(nextNode);
            for (auto dep : graph.getVertices())
            {
                if (!visited[dep])
                {
                    if (graph.isDirectlyDependingOn(nextNode, dep))
                    {
                        nodesToVisit.push(dep);
                        visited[dep] = true;
                    }
                }
            }
        }

        dependencies.erase(id);
        return dependencies;
    }

    auto graphToString(const IDirectedGraph & graph) -> std::string
    {
        std::stringstream output;
        output << "\n";
        auto const delimiter = "\t";
        output << delimiter << delimiter;
        for (auto col = 0; col < static_cast<Identifier>(graph.getSize()); ++col)
        {
            output << col << delimiter;
        }
        output << "\n" << std::string(120, '_') << "\n";

        for (auto row = 0; row < static_cast<Identifier>(graph.getSize()); ++row)
        {
            output << row << delimiter << "|" << delimiter;
            for (auto col = 0; col < static_cast<Identifier>(graph.getSize()); ++col)
            {
                output << (graph.isDirectlyDependingOn(Identifier(col), Identifier(row)) ? "X"
                                                                                         : " ")
                       << delimiter;
            }
            output << "\n";
        }

        return output.str();
    }

    auto graphToGraphVizStr(const IDirectedGraph & graph) -> std::string
    {
        std::stringstream output;
        output << "digraph G {\n";
        for (auto vertex : graph.getVertices())
        {
            auto dependencies = determineDirectDependencies(graph, vertex);
            for (auto dep : dependencies)
            {
                output << "\t \"" << dep << "\" -> \"" << vertex << "\"\n";
            }
        }
        output << "}\n";
        return output.str();
    }

    auto IsDependingOnImpl(const IDirectedGraph & graph,
                           Identifier id,
                           Identifier dependencyInQuestion) -> bool
    {
        ProfileFunction;

        if (graph.getSize() == 0)
        {
            return false;
        }
        
        if (id < 0 || dependencyInQuestion < 0)
        {
            return false;
        }
        if (graph.isDirectlyDependingOn(id, dependencyInQuestion))
        {
            return true;
        }

      
        std::vector<bool> visited(graph.getSize(), false);
        std::queue<Identifier> nodesToVisit;

        nodesToVisit.push(id);
        visited[id] = true;

        while (!nodesToVisit.empty())
        {
            auto const nextNode = nodesToVisit.front();
            nodesToVisit.pop();

            if (graph.isDirectlyDependingOn(nextNode, dependencyInQuestion))
            {
                return true;
            }

            for (auto dep = 0; dep < static_cast<Identifier>(graph.getSize()); ++dep)
            {
                if (!visited[dep])
                {
                    if (graph.isDirectlyDependingOn(nextNode, dep))
                    {
                        nodesToVisit.push(dep);
                        visited[dep] = true;
                    }
                }
            }
        }
        return false;
    }

    auto isDependingOn(const IDirectedGraph & graph, Identifier id, Identifier dependencyInQuestion)
      -> bool
    {
        ProfileFunction;
        if (id < 0 || dependencyInQuestion < 0)
        {
            return false;
        }

        if (id == dependencyInQuestion)
        {
            return false;
        }
        return IsDependingOnImpl(graph, id, dependencyInQuestion);
    }

    auto addDependencyIfConflictFree(IDirectedGraph & graph,
                                     Identifier id,
                                     Identifier idOfDependency) -> bool
    {
        ProfileFunction;
        if (id < 0 || idOfDependency < 0)
        {
            return false;
        }
        if (isDependingOn(graph, idOfDependency, id))
        {
            return false;
        }

        graph.addDependency(id, idOfDependency);
        return true;
    }

    auto topologicalSort(const IDirectedGraph & graph) -> VertexList
    {
        ProfileFunction;
        // tsort based on DFS
        enum class NodeType
        {
            CHILD,
            PARENT
        };

        // Stack to keep track of the nodes to visit
        std::stack<std::pair<NodeType, Identifier>> nodesToVisit;

        // Vector of visited nodes
        std::vector<bool> visited(graph.getSize(), false);

        // List of vertices in topological order
        VertexList topologicalOrder;

        // Loop through all the vertices of the graph
        for (auto id = 0; id < static_cast<Identifier>(graph.getSize()); ++id)
        {
            // If the current vertex is not visited, add it as a child to the stack
            if (!visited[id])
            {
                nodesToVisit.push({NodeType::CHILD, id});
            }

            // While there are still nodes in the stack to visit
            while (!nodesToVisit.empty())
            {
                // Get the type and id of the top node in the stack
                auto [nodeType, nodeId] = nodesToVisit.top();
                nodesToVisit.pop();

                // Mark the current node as visited
                visited[nodeId] = true;

                // If the node type is parent and it is not already in the topological order,
                // add it to the topological order
                if (nodeType == NodeType::PARENT &&
                    std::find(topologicalOrder.begin(), topologicalOrder.end(), nodeId) ==
                      topologicalOrder.end())
                {
                    topologicalOrder.push_back(nodeId);
                }
                else if (nodeType == NodeType::CHILD)
                {
                    // Add the node to the stack as a parent
                    nodesToVisit.push({NodeType::PARENT, nodeId});

                    // Check every adjacent node to the current node
                    for (auto dep = 0; dep < static_cast<Identifier>(graph.getSize()); ++dep)
                    {
                        // If adjacent node is not visited already and is dependent on the current
                        // one, add it to the stack as a child to visit
                        if (!visited[dep])
                        {
                            if (graph.isDirectlyDependingOn(nodeId, dep))
                            {
                                nodesToVisit.push({NodeType::CHILD, dep});
                            }
                        }
                    }
                }
            }
        }

        // Return the list of vertices in topological order
        return topologicalOrder;
    }

    auto determineDepth(const IDirectedGraph & graph, Identifier start) -> DepthMap
    {
        ProfileFunction;
        DepthMap result;
        result.reserve(graph.getSize());

        std::list<BfsItem> nodesToVisit;
        std::vector<bool> visited(graph.getSize(), false);
        visited[start] = true;

        auto const constexpr depth = 0;
        auto currentNode = BfsItem{start, depth};
        nodesToVisit.push_back(currentNode);

        while (!nodesToVisit.empty())
        {
            currentNode = nodesToVisit.front();
            result[currentNode.identifier] =
              std::max(currentNode.depth, result[currentNode.identifier]);
            nodesToVisit.pop_front();

            auto const successorList = determineSuccessor(graph, currentNode.identifier);
            for (auto successorId : successorList)
            {
                nodesToVisit.push_back({successorId, currentNode.depth + 1});
                visited[successorId] = true;
            }
        }
        return result;
    }

    auto inDegreeZeroVertices(const IDirectedGraph & graph) -> VertexList
    {
        VertexList InDegreeZeroVertices;
        for (auto id : graph.getVertices())
        {
            if (!graph.hasPredecessors(id))
            {
                InDegreeZeroVertices.push_back(id);
            }
        }
        return InDegreeZeroVertices;
    }

    auto determineSuccessor(const IDirectedGraph & graph, Identifier predecessor) -> VertexList
    {
        VertexList successor;
        for (auto id : graph.getVertices())
        {
            if (graph.isDirectlyDependingOn(id, predecessor))
            {
                successor.push_back(id);
            }
        }
        return successor;
    }

    auto isCyclic(const IDirectedGraph & graph) -> bool
    {
        for (auto vertex : graph.getVertices())
        {
            if (IsDependingOnImpl(graph, vertex, vertex))
            {
                return true;
            }
        }
        return false;
    }
} // namespace gladius::nodes::graph
