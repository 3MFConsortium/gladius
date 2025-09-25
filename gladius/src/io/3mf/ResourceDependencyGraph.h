#pragma once

#include "EventLogger.h"
#include "nodes/graph/AdjacencyListDirectedGraph.h"
#include "nodes/graph/IDirectedGraph.h"
#include <lib3mf_abi.hpp>
#include <lib3mf_implicit.hpp>
#include <memory>
#include <vector> // Added for std::vector

namespace gladius::io
{
    /**    Lib3MF_uint32 targetId
     * @brief Structure to hold the results of a resource removal check.
     */
    struct CanResourceBeRemovedResult
    {
        /** @brief True if the resource can be safely removed, false otherwise. */
        bool canBeRemoved;
        /** @brief List of resources that directly depend on the checked resource. */
        std::vector<Lib3MF::PResource> dependentResources;
        /** @brief List of build items that directly reference the checked resource. */
        std::vector<Lib3MF::PBuildItem> dependentBuildItems;
    };

    /**
     * @brief Class for building a dependency graph of all resources in a 3MF model
     *
     * This class analyzes a 3MF model and builds a directed graph representing dependencies
     * between different resources (LevelSets, Functions, MeshObjects, etc.)
     */
    class ResourceDependencyGraph
    {
      public:
        /**
         * @brief Constructor
         * @param model Smart pointer to a 3MF model
         * @param logger Shared logger for event handling
         */
        explicit ResourceDependencyGraph(Lib3MF::PModel model,
                                         gladius::events::SharedLogger logger);

        /**
         * @brief Destructor
         */
        ~ResourceDependencyGraph() = default;

        /**
         * @brief Build the dependency graph from the 3MF model
         *
         * Iterates over all resources in the model, adds them as vertices,
         * and determines the dependencies between them.
         */
        void buildGraph();

        /**
         * @brief Get the directed graph
         * @return Reference to the directed graph
         */
        [[nodiscard]] const nodes::graph::IDirectedGraph & getGraph() const;

        /**
         * @brief Get all resources required for a given resource.
         * @param resource The resource whose dependencies are to be queried.
         * @return Vector of Lib3MF::PResource representing all required resources.
         */
        [[nodiscard]] std::vector<Lib3MF::PResource>
        getAllRequiredResources(Lib3MF::PResource resource) const;

        /**
         * @brief Get all 3MF build items that reference the specified resource.
         * @param resource The resource whose build item references to find.
         * @return Vector of PBuildItem referencing the resource.
         */
        std::vector<Lib3MF::PBuildItem>
        findBuildItemsReferencingResource(Lib3MF::PResource resource) const;

        /**
         * @brief Checks if a resource can be safely removed.
         *
         * A resource can be removed if no other resource or build item directly depends on it.
         * @param resourceToBeRemoved The resource to check.
         * @return A struct containing the check result, dependent resources, and dependent build
         * items.
         */
        [[nodiscard]] CanResourceBeRemovedResult
        checkResourceRemoval(Lib3MF::PResource resourceToBeRemoved) const;

        /**
         * @brief Find all resources that are not required by any build item.
         *
         * A resource is considered unused if it and its dependencies are not referenced
         * directly or indirectly by any build item in the model.
         * @return Vector of resource pointers that can be safely removed.
         */
        [[nodiscard]] std::vector<Lib3MF::PResource> findUnusedResources() const;

        /**
         * @brief Get a resource by its ID
         * @param resourceId The ID of the resource to find
         * @return The resource if found, nullptr otherwise
         */
        [[nodiscard]] Lib3MF::PResource getResourceById(Lib3MF_uint32 resourceId) const;

      private:
        /**
         * @brief Process a LevelSet resource and add its dependencies to the graph
         * @param levelSet Pointer to the LevelSet resource
         */
        void processLevelSet(Lib3MF::PLevelSet levelSet);

        /**
         * @brief Process a Function resource and add its dependencies to the graph
         * @param function Pointer to the Function resource
         */
        void processFunction(Lib3MF::PFunction function);

        /**
         * @brief Process a ComponentsObject resource and add its dependencies to the graph
         * @param componentsObject Pointer to the ComponentsObject resource
         */
        void processComponentsObject(Lib3MF::PComponentsObject componentsObject);

        /**
         * @brief Process a MeshObject resource and add its dependencies to the graph
         * @param meshObject Pointer to the MeshObject resource
         */
        void processMeshObject(Lib3MF::PMeshObject meshObject);

        /**
         * @brief Process a VolumeData resource and add its dependencies to the graph
         * @param volumeData Pointer to the VolumeData resource
         */
        void processVolumeData(Lib3MF::PVolumeData volumeData);

        /**
         * @brief Process a FunctionReference and add its dependencies to the graph
         * @param functionRef Pointer to the FunctionReference
         * @param resourceId ID of the resource that contains this function reference
         */
        void processFunctionReference(Lib3MF::PFunctionReference functionRef,
                                      Lib3MF_uint32 resourceId);

        /** @brief Smart pointer to the 3MF model */
        Lib3MF::PModel m_model;

        /** @brief The directed graph representing resource dependencies */
        std::unique_ptr<nodes::graph::IDirectedGraph> m_graph;

        /** @brief Shared logger for event handling */
        gladius::events::SharedLogger m_logger;
    };
} // namespace gladius::io
