#include "io/3mf/ResourceDependencyGraph.h"
#include "nodes/graph/DirectedGraph.h"
#include "nodes/graph/GraphAlgorithms.h"
#include <gtest/gtest.h>
#include <lib3mf_abi.hpp>
#include <lib3mf_implicit.hpp>

namespace gladius_tests
{
    using namespace gladius;

    /**
     * @brief Test fixture for ResourceDependencyGraph tests
     */
    class ResourceDependencyGraphTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // Create a new 3MF model wrapper for each test
            m_wrapper = Lib3MF::CWrapper::loadLibrary();
            ASSERT_TRUE(m_wrapper) << "Failed to load Lib3MF library";

            // Create a model
            m_model = m_wrapper->CreateModel();
            ASSERT_TRUE(m_model) << "Failed to create 3MF model";
        }

        void TearDown() override
        {
            // Release resources
            m_model = nullptr;
            m_wrapper = nullptr;
        }

        // Creates a basic model with different types of resources for testing
        void createTestModel()
        {
            // Create a mesh object
            Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
            Lib3MF_uint32 meshObjectId = meshObject->GetResourceID();
            m_resourceIds.push_back(meshObjectId);

            // Create a second mesh object for components
            Lib3MF::PMeshObject componentMesh = m_model->AddMeshObject();
            Lib3MF_uint32 componentMeshId = componentMesh->GetResourceID();
            m_resourceIds.push_back(componentMeshId);

            // Create a components object that references the mesh
            Lib3MF::PComponentsObject componentsObject = m_model->AddComponentsObject();
            Lib3MF::PComponent component = componentsObject->AddComponent(componentMesh.get(), Lib3MF::sTransform());
            Lib3MF_uint32 componentsObjectId = componentsObject->GetResourceID();
            m_resourceIds.push_back(componentsObjectId);

            // Remember expected dependencies
            m_expectedDependencies[componentsObjectId].insert(componentMeshId);
        }

        // Helper method to check if a dependency exists in the graph
        bool hasDependency(const nodes::graph::IDirectedGraph& graph, Lib3MF_uint32 id, Lib3MF_uint32 dependencyId)
        {
            return graph.isDirectlyDependingOn(id, dependencyId);
        }

        Lib3MF::PWrapper m_wrapper;
        Lib3MF::PModel m_model;
        std::vector<Lib3MF_uint32> m_resourceIds;
        std::unordered_map<Lib3MF_uint32, std::unordered_set<Lib3MF_uint32>> m_expectedDependencies;
    };

    TEST_F(ResourceDependencyGraphTest, BuildGraph_EmptyModel_NoVerticesInGraph)
    {
        // Arrange
        io::ResourceDependencyGraph dependencyGraph(m_model);
        
        // Act
        dependencyGraph.buildGraph();
        
        // Assert
        const auto& graph = dependencyGraph.getGraph();
        EXPECT_EQ(graph.getVertices().size(), 0);
    }

    TEST_F(ResourceDependencyGraphTest, BuildGraph_ModelWithResources_VerticesAddedToGraph)
    {
        // Arrange
        createTestModel();
        io::ResourceDependencyGraph dependencyGraph(m_model);
        
        // Act
        dependencyGraph.buildGraph();
        
        // Assert
        const auto& graph = dependencyGraph.getGraph();
        EXPECT_EQ(graph.getVertices().size(), m_resourceIds.size());
        
        // Check that all resource IDs are in the graph
        for (const auto& id : m_resourceIds)
        {
            EXPECT_TRUE(graph.getVertices().find(id) != graph.getVertices().end())
                << "Resource ID " << id << " not found in graph vertices";
        }
    }

    TEST_F(ResourceDependencyGraphTest, BuildGraph_ComponentsObjectDependsOnMeshObject_DependencyExists)
    {
        // Arrange
        createTestModel();
        io::ResourceDependencyGraph dependencyGraph(m_model);
        
        // Act
        dependencyGraph.buildGraph();
        
        // Assert
        const auto& graph = dependencyGraph.getGraph();
        
        // Check all expected dependencies
        for (const auto& [id, dependencies] : m_expectedDependencies)
        {
            for (const auto& dependencyId : dependencies)
            {
                EXPECT_TRUE(hasDependency(graph, id, dependencyId))
                    << "Resource " << id << " should depend on " << dependencyId;
            }
        }
    }

    TEST_F(ResourceDependencyGraphTest, BuildGraph_NullModel_GraphRemainsEmpty)
    {
        // Arrange
        Lib3MF::PModel nullModel = nullptr;
        io::ResourceDependencyGraph dependencyGraph(nullModel);
        
        // Act
        dependencyGraph.buildGraph();
        
        // Assert
        const auto& graph = dependencyGraph.getGraph();
        EXPECT_EQ(graph.getVertices().size(), 0);
    }

    TEST_F(ResourceDependencyGraphTest, BuildGraph_ComplexDependencyChain_TransitiveDependenciesExist)
    {
        // This test would create a more complex chain of dependencies
        // For example: LevelSet -> Function -> VolumeData -> Function

        // Arrange - Setup a model with a chain of dependencies
        // This is a simplified version as creating a complete chain would require
        // more elaborate setup with Lib3MF resources
        
        // Create a mesh object
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        Lib3MF_uint32 meshId = meshObject->GetResourceID();
        
        // Create a components object that references the mesh
        Lib3MF::PComponentsObject componentsObject = m_model->AddComponentsObject();
        Lib3MF::PComponent component = componentsObject->AddComponent(meshObject.get(), Lib3MF::sTransform());
        Lib3MF_uint32 componentsId = componentsObject->GetResourceID();
        
        // Create another components object that references the first components object
        // This isn't exactly how 3MF works but it's a simplified representation for testing
        Lib3MF::PComponentsObject parentObject = m_model->AddComponentsObject();
        Lib3MF::PComponent parentComponent = parentObject->AddComponent(componentsObject.get(), Lib3MF::sTransform());
        Lib3MF_uint32 parentId = parentObject->GetResourceID();
        
        io::ResourceDependencyGraph dependencyGraph(m_model);
        
        // Act
        dependencyGraph.buildGraph();
        
        // Assert
        const auto& graph = dependencyGraph.getGraph();
        
        // Component depends on mesh
        EXPECT_TRUE(hasDependency(graph, componentsId, meshId))
            << "Components object should depend on mesh object";
            
        // Parent depends on component
        EXPECT_TRUE(hasDependency(graph, parentId, componentsId))
            << "Parent components object should depend on child components object";
            
        // Transitive dependency check (using GraphAlgorithms)
        EXPECT_TRUE(nodes::graph::isDependingOn(graph, parentId, meshId))
            << "Parent should transitively depend on mesh";
    }

    TEST_F(ResourceDependencyGraphTest, GraphAlgorithmsIntegration_ModelWithComponentAndMesh_GraphAlgorithmsReturnCorrectDependencies)
    {
        // Arrange
        createTestModel();
        io::ResourceDependencyGraph dependencyGraph(m_model);
        dependencyGraph.buildGraph();
        const auto& graph = dependencyGraph.getGraph();
        
        // Find a root node (component) and a leaf node (mesh)
        Lib3MF_uint32 rootId = 0;
        Lib3MF_uint32 leafId = 0;
        
        for (const auto& [id, dependencies] : m_expectedDependencies)
        {
            if (!dependencies.empty())
            {
                rootId = id;
                leafId = *dependencies.begin();
                break;
            }
        }
        
        ASSERT_NE(rootId, 0) << "Could not find a root node for testing";
        ASSERT_NE(leafId, 0) << "Could not find a leaf node for testing";
        
        // Act
        auto directDependencies = nodes::graph::determineDirectDependencies(graph, rootId);
        auto allDependencies = nodes::graph::determineAllDependencies(graph, rootId);
        auto successors = nodes::graph::determineSuccessor(graph, leafId);
        
        // Assert
        EXPECT_TRUE(directDependencies.find(leafId) != directDependencies.end())
            << "Leaf node should be a direct dependency of root node";
            
        EXPECT_TRUE(allDependencies.find(leafId) != allDependencies.end())
            << "Leaf node should be in all dependencies of root node";
            
        EXPECT_TRUE(std::find(successors.begin(), successors.end(), rootId) != successors.end())
            << "Root node should be a successor of leaf node";
    }
}
