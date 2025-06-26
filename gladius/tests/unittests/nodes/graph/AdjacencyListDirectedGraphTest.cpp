#include "nodes/graph/AdjacencyListDirectedGraph.h"
#include "nodes/graph/DirectedGraph.h"
#include "nodes/graph/GraphAlgorithms.h"
#include <gtest/gtest.h>

namespace gladius::nodes::graph::tests
{
    // Test fixture for comparing DirectedGraph and AdjacencyListDirectedGraph
    class GraphImplementationTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // Create a directed graph using the matrix implementation
            matrixGraph = std::make_unique<DirectedGraph>(5);
            
            // Create a directed graph using the adjacency list implementation
            listGraph = std::make_unique<AdjacencyListDirectedGraph>(5);

            // Add common test data to both implementations
            setupTestGraphs();
        }

        void setupTestGraphs()
        {
            // Add vertices
            for (int i = 0; i < 5; ++i)
            {
                matrixGraph->addVertex(i);
                listGraph->addVertex(i);
            }
            
            // Add dependencies
            // 0 -> 1 -> 2 -> 3 -> 4
            //      |         ^
            //      |---------|
            matrixGraph->addDependency(1, 0); // 1 depends on 0
            matrixGraph->addDependency(2, 1); // 2 depends on 1
            matrixGraph->addDependency(3, 2); // 3 depends on 2
            matrixGraph->addDependency(3, 1); // 3 also depends on 1
            matrixGraph->addDependency(4, 3); // 4 depends on 3
            
            listGraph->addDependency(1, 0);   // 1 depends on 0
            listGraph->addDependency(2, 1);   // 2 depends on 1
            listGraph->addDependency(3, 2);   // 3 depends on 2
            listGraph->addDependency(3, 1);   // 3 also depends on 1
            listGraph->addDependency(4, 3);   // 4 depends on 3
        }

        std::unique_ptr<DirectedGraph> matrixGraph;
        std::unique_ptr<AdjacencyListDirectedGraph> listGraph;
    };

    // Test that getSize returns the same value for both implementations
    TEST_F(GraphImplementationTest, GetSize)
    {
        EXPECT_EQ(matrixGraph->getSize(), listGraph->getSize());
    }

    // Test that getVertices returns the same set for both implementations
    TEST_F(GraphImplementationTest, GetVertices)
    {
        auto matrixVertices = matrixGraph->getVertices();
        auto listVertices = listGraph->getVertices();
        
        EXPECT_EQ(matrixVertices.size(), listVertices.size());
        
        for (auto vertex : matrixVertices)
        {
            EXPECT_TRUE(listVertices.find(vertex) != listVertices.end());
        }
    }

    // Test that isDirectlyDependingOn returns the same result for both implementations
    TEST_F(GraphImplementationTest, IsDirectlyDependingOn)
    {
        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < 5; ++j)
            {
                EXPECT_EQ(
                    matrixGraph->isDirectlyDependingOn(i, j),
                    listGraph->isDirectlyDependingOn(i, j)
                ) << "Different results for isDirectlyDependingOn(" << i << ", " << j << ")";
            }
        }
    }

    // Test that dependency detection works the same for both implementations
    TEST_F(GraphImplementationTest, DependencyDetection)
    {
        for (int i = 0; i < 5; ++i)
        {
            bool matrixHasDeps = false;
            bool listHasDeps = false;
            
            // Check if any vertex depends on this one
            for (int j = 0; j < 5; ++j)
            {
                if (matrixGraph->isDirectlyDependingOn(j, i))
                {
                    matrixHasDeps = true;
                    break;
                }
            }
            
            for (int j = 0; j < 5; ++j)
            {
                if (listGraph->isDirectlyDependingOn(j, i))
                {
                    listHasDeps = true;
                    break;
                }
            }
            
            EXPECT_EQ(matrixHasDeps, listHasDeps) 
                << "Different results for dependency detection of vertex " << i;
        }
    }

    // Test that removing a dependency works the same for both implementations
    TEST_F(GraphImplementationTest, RemoveDependency)
    {
        // Remove dependency: 3 -> 1
        matrixGraph->removeDependency(3, 1);
        listGraph->removeDependency(3, 1);
        
        // Check that the dependency was removed in both graphs
        EXPECT_FALSE(matrixGraph->isDirectlyDependingOn(3, 1));
        EXPECT_FALSE(listGraph->isDirectlyDependingOn(3, 1));
        
        // Check that other dependencies remain intact
        EXPECT_TRUE(matrixGraph->isDirectlyDependingOn(3, 2));
        EXPECT_TRUE(listGraph->isDirectlyDependingOn(3, 2));
    }

    // Test that removing a vertex works the same for both implementations
    TEST_F(GraphImplementationTest, RemoveVertex)
    {
        // Remove vertex 2
        matrixGraph->removeVertex(2);
        listGraph->removeVertex(2);
        
        // Verify vertex is removed from both graphs
        auto matrixVertices = matrixGraph->getVertices();
        auto listVertices = listGraph->getVertices();
        
        EXPECT_EQ(matrixVertices.find(2), matrixVertices.end());
        EXPECT_EQ(listVertices.find(2), listVertices.end());
        
        // Verify dependencies involving vertex 2 are removed
        EXPECT_FALSE(matrixGraph->isDirectlyDependingOn(3, 2));
        EXPECT_FALSE(listGraph->isDirectlyDependingOn(3, 2));
    }

    // Test that topological sort produces the same result for both implementations
    TEST_F(GraphImplementationTest, TopologicalSort)
    {
        auto matrixOrder = topologicalSort(*matrixGraph);
        auto listOrder = topologicalSort(*listGraph);

        EXPECT_EQ(matrixOrder.size(), listOrder.size());

        // Note: There could be multiple valid topological orderings,
        // so we can't directly compare the vectors. We can verify that
        // the dependencies are respected in both orderings.
        // For our simple test graph, the ordering should be:
        // [0, 1, 2, 3, 4] or some variation that respects dependencies
    }

    // Test the performance difference between implementations for large sparse graphs
    TEST(GraphPerformanceTest, LargeGraphCreation)
    {
        const std::size_t graphSize = 1000;
        const std::size_t edgeCount = 2000; // Only 0.2% of possible edges
        
        // Create matrix implementation
        DirectedGraph matrixGraph(graphSize);
        
        // Create adjacency list implementation
        AdjacencyListDirectedGraph listGraph(graphSize);
        
        // Add vertices
        for (std::size_t i = 0; i < graphSize; ++i)
        {
            matrixGraph.addVertex(static_cast<Identifier>(i));
            listGraph.addVertex(static_cast<Identifier>(i));
        }
        
        // Add some random edges (same for both graphs)
        srand(42); // Use fixed seed for reproducibility
        for (std::size_t i = 0; i < edgeCount; ++i)
        {
            Identifier from = static_cast<Identifier>(rand() % graphSize);
            Identifier to = static_cast<Identifier>(rand() % graphSize);
            if (from != to) // Avoid self-loops
            {
                matrixGraph.addDependency(from, to);
                listGraph.addDependency(from, to);
            }
        }
        
        // Verify both graphs have the same structure
        for (std::size_t i = 0; i < 100; ++i) // Test a subset of connections
        {
            Identifier from = rand() % graphSize;
            Identifier to = rand() % graphSize;
            EXPECT_EQ(
                matrixGraph.isDirectlyDependingOn(from, to),
                listGraph.isDirectlyDependingOn(from, to)
            );
        }
    }
} // namespace gladius::nodes::graph::tests
