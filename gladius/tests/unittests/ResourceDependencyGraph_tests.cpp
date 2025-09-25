#include "io/3mf/ResourceDependencyGraph.h"
#include "nodes/graph/DirectedGraph.h"
#include "nodes/graph/GraphAlgorithms.h"
#include <gtest/gtest.h>
#include "io/3mf/Lib3mfLoader.h"
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
            m_wrapper = gladius::io::loadLib3mfScoped();
            ASSERT_TRUE(m_wrapper) << "Failed to load Lib3MF library";

            // Create a model
            m_model = m_wrapper->CreateModel();
            ASSERT_TRUE(m_model) << "Failed to create 3MF model";

            m_logger = gladius::events::SharedLogger();
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
        //logger
        gladius::events::SharedLogger m_logger;
    };

    TEST_F(ResourceDependencyGraphTest, BuildGraph_EmptyModel_NoVerticesInGraph)
    {
        // Arrange
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        
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
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        
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
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        
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
        io::ResourceDependencyGraph dependencyGraph(nullModel, m_logger);
        
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
        
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        
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
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
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

    TEST_F(ResourceDependencyGraphTest, GetAllRequiredResources_WithComponentObject_ReturnsDependencies)
    {
        // Arrange
        createTestModel();
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Find the components object
        Lib3MF::PResourceIterator resourceIterator = m_model->GetResources();
        Lib3MF::PResource componentsResource = nullptr;
        while (resourceIterator->MoveNext())
        {
            Lib3MF::PResource res = resourceIterator->GetCurrent();
            if (std::dynamic_pointer_cast<Lib3MF::CComponentsObject>(res))
            {
                componentsResource = res;
                break;
            }
        }
        ASSERT_TRUE(componentsResource) << "No components object found in test model";

        // Act
        auto requiredResources = dependencyGraph.getAllRequiredResources(componentsResource);

        // Assert
        // Should contain the mesh referenced by the component
        bool foundMesh = false;
        for (const auto& res : requiredResources)
        {
            if (std::dynamic_pointer_cast<Lib3MF::CMeshObject>(res))
            {
                foundMesh = true;
                break;
            }
        }
        EXPECT_TRUE(foundMesh) << "Required resources should include the mesh object referenced by the component object";
    }

    TEST_F(ResourceDependencyGraphTest, GetAllRequiredResources_WithNullResource_ReturnsEmpty)
    {
        // Arrange
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto requiredResources = dependencyGraph.getAllRequiredResources(nullptr);

        // Assert
        EXPECT_TRUE(requiredResources.empty()) << "Should return empty vector for null resource input";
    }

    TEST_F(ResourceDependencyGraphTest, GetAllRequiredResources_WithNoDependencies_ReturnsEmpty)
    {
        // Arrange
        m_model = m_wrapper->CreateModel();
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto requiredResources = dependencyGraph.getAllRequiredResources(meshObject);

        // Assert
        EXPECT_TRUE(requiredResources.empty()) << "Mesh object with no dependencies should return empty vector";
    }

    TEST_F(ResourceDependencyGraphTest, FindBuildItemsReferencingResource_WithBuildItem_ReturnsMatchingItem)
    {
        // Arrange: create a mesh object and add a build item referencing it
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        Lib3MF::PObject objectResource = std::static_pointer_cast<Lib3MF::CObject>(meshObject);
        Lib3MF::sTransform transform = {}; // initialize identity matrix
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 3; ++j) {
                transform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        Lib3MF::PBuildItem buildItem = m_model->AddBuildItem(objectResource, transform);

        // Act
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();
        auto items = dependencyGraph.findBuildItemsReferencingResource(meshObject);

        // Assert: should find the one build item referencing the mesh object
        ASSERT_EQ(items.size(), 1u) << "Should find one build item referencing the mesh object";
        EXPECT_EQ(items[0]->GetObjectResourceID(), meshObject->GetResourceID());
    }

    TEST_F(ResourceDependencyGraphTest, FindBuildItemsReferencingResource_WithNoBuildItems_ReturnsEmpty)
    {
        // Arrange: new model with a mesh but no build items
        m_model = m_wrapper->CreateModel();
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();
        // Act
        auto items = dependencyGraph.findBuildItemsReferencingResource(meshObject);

        // Assert
        EXPECT_TRUE(items.empty()) << "No build items exist, should return empty vector";
    }

    TEST_F(ResourceDependencyGraphTest, FindBuildItemsReferencingResource_WithNullResource_ReturnsEmpty)
    {
        // Arrange
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto items = dependencyGraph.findBuildItemsReferencingResource(nullptr);

        // Assert
        EXPECT_TRUE(items.empty()) << "Null resource should yield empty build items";
    }

    TEST_F(ResourceDependencyGraphTest, CheckResourceRemoval_NullResource_ReturnsFalse)
    {
        // Arrange
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto result = dependencyGraph.checkResourceRemoval(nullptr);

        // Assert
        EXPECT_FALSE(result.canBeRemoved) << "Null resource should not be removable";
        EXPECT_TRUE(result.dependentResources.empty()) << "dependentResources should be empty for null resource";
        EXPECT_TRUE(result.dependentBuildItems.empty()) << "dependentBuildItems should be empty for null resource";
    }

    TEST_F(ResourceDependencyGraphTest, CheckResourceRemoval_NoDependencies_ReturnsTrue)
    {
        // Arrange
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto result = dependencyGraph.checkResourceRemoval(meshObject);

        // Assert
        EXPECT_TRUE(result.canBeRemoved) << "Mesh object with no dependencies should be removable";
        EXPECT_TRUE(result.dependentResources.empty()) << "dependentResources should be empty when no dependencies";
        EXPECT_TRUE(result.dependentBuildItems.empty()) << "dependentBuildItems should be empty when no build items";
    }

    TEST_F(ResourceDependencyGraphTest, CheckResourceRemoval_WithDependentResource_ReturnsFalse)
    {
        // Arrange
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        Lib3MF::PComponentsObject componentsObject = m_model->AddComponentsObject();
        componentsObject->AddComponent(meshObject.get(), Lib3MF::sTransform());
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();
        Lib3MF_uint32 compId = componentsObject->GetResourceID();

        // Act
        auto result = dependencyGraph.checkResourceRemoval(meshObject);

        // Assert
        EXPECT_FALSE(result.canBeRemoved) << "Mesh object with dependent resource should not be removable";
        ASSERT_EQ(result.dependentResources.size(), 1u);
        EXPECT_EQ(result.dependentResources[0]->GetResourceID(), compId);
        EXPECT_TRUE(result.dependentBuildItems.empty());
    }

    TEST_F(ResourceDependencyGraphTest, CheckResourceRemoval_WithDependentBuildItem_ReturnsFalse)
    {
        // Arrange
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        // Create a build item referencing the mesh object
        Lib3MF::PBuildItem buildItem = m_model->AddBuildItem(
            std::static_pointer_cast<Lib3MF::CObject>(meshObject),
            Lib3MF::sTransform());
        
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();
        Lib3MF_uint32 meshId = meshObject->GetResourceID();

        // Act
        auto result = dependencyGraph.checkResourceRemoval(meshObject);

        // Assert
        EXPECT_FALSE(result.canBeRemoved) << "Mesh object with dependent build item should not be removable";
        EXPECT_TRUE(result.dependentResources.empty());
        ASSERT_EQ(result.dependentBuildItems.size(), 1u);
        EXPECT_EQ(result.dependentBuildItems[0]->GetObjectResourceID(), meshId);
    }

    TEST_F(ResourceDependencyGraphTest, FindUnusedResources_WithBuildItems_ReturnsCorrectResources)
    {
        // Arrange
        // 1. Create a mesh object that will be referenced by a build item
        Lib3MF::PMeshObject usedMeshObject = m_model->AddMeshObject();
        Lib3MF_uint32 usedMeshId = usedMeshObject->GetResourceID();
        
        // 2. Create another mesh object that will NOT be referenced by any build item
        Lib3MF::PMeshObject unusedMeshObject = m_model->AddMeshObject();
        Lib3MF_uint32 unusedMeshId = unusedMeshObject->GetResourceID();
        
        // 3. Create a components object that references the unused mesh (to test dependencies)
        Lib3MF::PComponentsObject componentsObject = m_model->AddComponentsObject();
        componentsObject->AddComponent(unusedMeshObject.get(), Lib3MF::sTransform());
        Lib3MF_uint32 componentsId = componentsObject->GetResourceID();
        
        // 4. Add a build item that references the used mesh
        Lib3MF::PObject objectResource = std::static_pointer_cast<Lib3MF::CObject>(usedMeshObject);
        Lib3MF::sTransform transform = {}; // Initialize identity matrix
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 3; ++j) {
                transform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        Lib3MF::PBuildItem buildItem = m_model->AddBuildItem(objectResource, transform);
        
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto unusedResources = dependencyGraph.findUnusedResources();

        // Assert
        ASSERT_EQ(unusedResources.size(), 2u) << "Should find two unused resources";
        
        // Check that the unused resources are the ones we expect
        bool foundUnusedMesh = false;
        bool foundComponentsObject = false;
        
        for (const auto& res : unusedResources)
        {
            Lib3MF_uint32 resId = res->GetResourceID();
            
            if (resId == unusedMeshId) {
                foundUnusedMesh = true;
            }
            else if (resId == componentsId) {
                foundComponentsObject = true;
            }
        }
        
        EXPECT_TRUE(foundUnusedMesh) << "The unused mesh object should be in the results";
        EXPECT_TRUE(foundComponentsObject) << "The components object should be in the results";
    }
    
    TEST_F(ResourceDependencyGraphTest, FindUnusedResources_WithTransitiveDependencies_ReturnsCorrectResources)
    {
        // Arrange
        // Create a more complex dependency chain:
        // usedMesh <- buildItem (used)
        // unusedMesh -> componentsObject -> levelSet (all unused)
        
        // 1. Create used mesh (will be referenced by build item)
        Lib3MF::PMeshObject usedMeshObject = m_model->AddMeshObject();
        Lib3MF_uint32 usedMeshId = usedMeshObject->GetResourceID();
        
        // 2. Create unused mesh
        Lib3MF::PMeshObject unusedMeshObject = m_model->AddMeshObject();
        Lib3MF_uint32 unusedMeshId = unusedMeshObject->GetResourceID();
        
        // 3. Create components object that references the unused mesh
        Lib3MF::PComponentsObject componentsObject = m_model->AddComponentsObject();
        componentsObject->AddComponent(unusedMeshObject.get(), Lib3MF::sTransform());
        Lib3MF_uint32 componentsId = componentsObject->GetResourceID();
        
        // 4. Create a function (simplified, just for ID)
        Lib3MF::PImplicitFunction function = m_model->AddImplicitFunction();
        Lib3MF_uint32 functionId = function->GetResourceID();
        
        // 5. Create a LevelSet that depends on the function
        Lib3MF::PLevelSet levelSet = m_model->AddLevelSet();
        levelSet->SetFunction(function.get());
        levelSet->SetMesh(unusedMeshObject.get()); // Must use a MeshObject, not ComponentsObject
        Lib3MF_uint32 levelSetId = levelSet->GetResourceID();
        
        // 6. Add a build item referencing the used mesh
        Lib3MF::PBuildItem buildItem = m_model->AddBuildItem(usedMeshObject.get(), Lib3MF::sTransform());
        
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto unusedResources = dependencyGraph.findUnusedResources();

        // Assert
        ASSERT_EQ(unusedResources.size(), 4u) << "Should find four unused resources";
        
        // Check that the unused resources are the ones we expect
        std::unordered_set<Lib3MF_uint32> expectedUnusedIds = {
            unusedMeshId, componentsId, functionId, levelSetId
        };
        
        std::unordered_set<Lib3MF_uint32> actualUnusedIds;
        for (const auto& res : unusedResources) {
            actualUnusedIds.insert(res->GetResourceID());
        }
        
        EXPECT_EQ(actualUnusedIds, expectedUnusedIds) << "The unused resources don't match expected IDs";
    }
    
    TEST_F(ResourceDependencyGraphTest, FindUnusedResources_NoBuildItems_ReturnsEmptySet)
    {
        // Arrange - Create resources but no build items
        Lib3MF::PMeshObject meshObject = m_model->AddMeshObject();
        io::ResourceDependencyGraph dependencyGraph(m_model, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto unusedResources = dependencyGraph.findUnusedResources();

        // Assert
        EXPECT_TRUE(unusedResources.empty()) 
            << "With no build items, should return empty set (as all resources could potentially be used)";
    }
    
    TEST_F(ResourceDependencyGraphTest, FindUnusedResources_NullModel_ReturnsEmptySet)
    {
        // Arrange
        Lib3MF::PModel nullModel = nullptr;
        io::ResourceDependencyGraph dependencyGraph(nullModel, m_logger);
        dependencyGraph.buildGraph();

        // Act
        auto unusedResources = dependencyGraph.findUnusedResources();

        // Assert
        EXPECT_TRUE(unusedResources.empty()) << "With null model, should return empty set";
    }
}
