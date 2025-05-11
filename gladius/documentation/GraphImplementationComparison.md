# DirectedGraph vs AdjacencyListDirectedGraph - Implementation Comparison

## Introduction

This document compares two implementations of the `IDirectedGraph` interface:
1. `DirectedGraph` - Using an adjacency matrix representation
2. `AdjacencyListDirectedGraph` - Using an adjacency list representation

## Implementation Details

### DirectedGraph (Adjacency Matrix)

The `DirectedGraph` implementation uses a flat vector (`std::vector<bool>`) to store an adjacency matrix. The matrix is indexed by `id * size + dependencyId` where `id` is the dependent node and `dependencyId` is the dependency.

**Memory Complexity:** O(V²), where V is the number of vertices

**Time Complexity:**
- `addDependency`: O(1)
- `removeDependency`: O(1) 
- `isDirectlyDependingOn`: O(1)
- `removeVertex`: O(V)
- `hasPredecessors`: O(V) (iterating through predecessor list)

**Pros:**
- Constant time edge queries
- Simple implementation
- Fast edge insertion and deletion

**Cons:**
- Quadratic memory usage (inefficient for sparse graphs)
- Wastes memory when the graph is sparse (most real-world graphs are sparse)

### AdjacencyListDirectedGraph

The `AdjacencyListDirectedGraph` implementation uses hash maps (`std::unordered_map<Identifier, DependencySet>`) to store outgoing and incoming edges for each vertex.

**Memory Complexity:** O(V + E), where V is the number of vertices and E is the number of edges

**Time Complexity:**
- `addDependency`: O(1) average
- `removeDependency`: O(1) average
- `isDirectlyDependingOn`: O(1) average
- `removeVertex`: O(E) in worst case (if the vertex has many connections)
- `hasPredecessors`: O(1) (direct lookup of outgoing edges)

**Pros:**
- Much more memory-efficient for sparse graphs
- Memory usage scales with the actual number of edges
- Faster iteration over a vertex's neighbors

**Cons:**
- Slightly more complex implementation
- Hash table lookups have a small constant overhead

## Performance Comparison

### Memory Usage

For a graph with V vertices and E edges:

- **DirectedGraph**: Uses approximately `V² * sizeof(bool)` bytes for the adjacency matrix, plus overhead for the vertex set and predecessor lists.

- **AdjacencyListDirectedGraph**: Uses approximately `E * sizeof(Identifier) * 2 + V * sizeof(pointer) * 2` bytes for the adjacency lists, plus overhead for the vertex set.

For sparse graphs where E << V², the adjacency list implementation is significantly more memory-efficient.

### Time Performance

For most operations, both implementations have similar time complexity. However:

- For very large sparse graphs, the adjacency list implementation can be faster overall due to better cache locality when iterating over a vertex's neighbors.

- For dense graphs where E ≈ V², the matrix implementation might have a slight edge due to simpler indexing.

## Use Case Recommendations

- Use **DirectedGraph** when:
  - The graph is small (less than a few thousand vertices)
  - The graph is dense (many edges between vertices)
  - Memory is not a concern
  - Edge lookup speed is critical

- Use **AdjacencyListDirectedGraph** when:
  - The graph is large (thousands or more vertices)
  - The graph is sparse (few edges compared to potential total)
  - Memory efficiency is important
  - You need to frequently iterate over a vertex's neighbors

## Conclusion

The `AdjacencyListDirectedGraph` implementation provides a more memory-efficient alternative to the existing `DirectedGraph` class, particularly for large sparse graphs. Both implementations fully satisfy the `IDirectedGraph` interface, making them interchangeable in the codebase.

For the Gladius project, where many graphs may represent complex dependency relationships that are inherently sparse, the adjacency list implementation could provide significant memory savings.
