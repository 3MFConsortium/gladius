#include "AdjacencyListDirectedGraph.h"
#include "DirectedGraph.h"
#include "GraphAlgorithms.h"
#include <iostream>
#include <chrono>

/**
 * @brief Example program demonstrating the AdjacencyListDirectedGraph implementation
 * 
 * This program creates both a DirectedGraph (matrix-based) and an AdjacencyListDirectedGraph
 * and performs the same operations on both to compare them.
 */
int main()
{
    const std::size_t graphSize = 10000;  // Large graph size for demonstration
    const std::size_t edgeCount = 20000;  // Relatively sparse graph
    
    std::cout << "Creating directed graphs with " << graphSize << " vertices and "
              << edgeCount << " edges...\n";
    
    // Create both graph implementations
    auto matrixStartTime = std::chrono::high_resolution_clock::now();
    gladius::nodes::graph::DirectedGraph matrixGraph(graphSize);
    auto matrixEndTime = std::chrono::high_resolution_clock::now();
    
    auto listStartTime = std::chrono::high_resolution_clock::now();
    gladius::nodes::graph::AdjacencyListDirectedGraph listGraph(graphSize);
    auto listEndTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Matrix graph creation time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     matrixEndTime - matrixStartTime)
                     .count()
              << " microseconds\n";
              
    std::cout << "Adjacency list graph creation time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     listEndTime - listStartTime)
                     .count()
              << " microseconds\n";
    
    // Add vertices
    matrixStartTime = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < graphSize; ++i)
    {
        matrixGraph.addVertex(i);
    }
    matrixEndTime = std::chrono::high_resolution_clock::now();
    
    listStartTime = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < graphSize; ++i)
    {
        listGraph.addVertex(i);
    }
    listEndTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Adding " << graphSize << " vertices to matrix graph: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     matrixEndTime - matrixStartTime)
                     .count()
              << " microseconds\n";
    
    std::cout << "Adding " << graphSize << " vertices to adjacency list graph: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     listEndTime - listStartTime)
                     .count()
              << " microseconds\n";
    
    // Add random edges
    srand(42);  // Use fixed seed for reproducibility
    
    matrixStartTime = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < edgeCount; ++i)
    {
        gladius::nodes::graph::Identifier from = rand() % graphSize;
        gladius::nodes::graph::Identifier to = rand() % graphSize;
        if (from != to)  // Avoid self-loops
        {
            matrixGraph.addDependency(from, to);
        }
    }
    matrixEndTime = std::chrono::high_resolution_clock::now();
    
    // Reset the random seed to create identical edges
    srand(42);
    
    listStartTime = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < edgeCount; ++i)
    {
        gladius::nodes::graph::Identifier from = rand() % graphSize;
        gladius::nodes::graph::Identifier to = rand() % graphSize;
        if (from != to)  // Avoid self-loops
        {
            listGraph.addDependency(from, to);
        }
    }
    listEndTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Adding " << edgeCount << " edges to matrix graph: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     matrixEndTime - matrixStartTime)
                     .count()
              << " microseconds\n";
    
    std::cout << "Adding " << edgeCount << " edges to adjacency list graph: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     listEndTime - listStartTime)
                     .count()
              << " microseconds\n";
    
    // Query random edges
    const int numQueries = 10000;
    int matrixHits = 0;
    int listHits = 0;
    
    matrixStartTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numQueries; ++i)
    {
        gladius::nodes::graph::Identifier from = rand() % graphSize;
        gladius::nodes::graph::Identifier to = rand() % graphSize;
        if (matrixGraph.isDirectlyDependingOn(from, to))
        {
            matrixHits++;
        }
    }
    matrixEndTime = std::chrono::high_resolution_clock::now();
    
    // Reset seed for identical queries
    srand(42);
    
    listStartTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numQueries; ++i)
    {
        gladius::nodes::graph::Identifier from = rand() % graphSize;
        gladius::nodes::graph::Identifier to = rand() % graphSize;
        if (listGraph.isDirectlyDependingOn(from, to))
        {
            listHits++;
        }
    }
    listEndTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Querying " << numQueries << " edges in matrix graph ("
              << matrixHits << " hits): "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     matrixEndTime - matrixStartTime)
                     .count()
              << " microseconds\n";
    
    std::cout << "Querying " << numQueries << " edges in adjacency list graph ("
              << listHits << " hits): "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     listEndTime - listStartTime)
                     .count()
              << " microseconds\n";
    
    // Verify both implementations return the same results
    assert(matrixHits == listHits);
    
    // Memory usage comparison
    std::cout << "\nMemory usage comparison:\n";
    std::cout << "Matrix implementation: O(V²) - approximately "
              << sizeof(bool) * graphSize * graphSize / (1024.0 * 1024.0)
              << " MB for the adjacency matrix alone\n";
    
    std::cout << "Adjacency list implementation: O(V + E) - approximately "
              << (sizeof(gladius::nodes::graph::Identifier) * edgeCount * 2 + 
                  sizeof(void*) * graphSize * 2) / (1024.0 * 1024.0)
              << " MB for the adjacency lists\n";
    
    return 0;
}
