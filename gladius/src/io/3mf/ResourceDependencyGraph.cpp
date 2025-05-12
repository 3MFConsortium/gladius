#include "ResourceDependencyGraph.h"
#include "EventLogger.h"
#include "ResourceIdUtil.h"
#include "nodes/graph/AdjacencyListDirectedGraph.h"
#include "nodes/graph/GraphAlgorithms.h"
#include "nodes/graph/IDirectedGraph.h"
#include <exception>
#include <fmt/format.h>

namespace gladius::io
{
    ResourceDependencyGraph::ResourceDependencyGraph(Lib3MF::PModel model,
                                                     gladius::events::SharedLogger logger)
        : m_model(model)
        , m_graph(std::make_unique<nodes::graph::AdjacencyListDirectedGraph>())
        , m_logger(std::move(logger))
    {
        if (m_logger)
            m_logger->addEvent(
              {"Initialized ResourceDependencyGraph", gladius::events::Severity::Info});
    }

    void ResourceDependencyGraph::buildGraph()
    {
        if (m_logger)
            m_logger->addEvent(
              {"Building resource dependency graph", gladius::events::Severity::Info});

        if (!m_model)
        {
            if (m_logger)
                m_logger->addEvent(
                  {"No model available for dependency graph", gladius::events::Severity::Error});
            return;
        }

        // Retrieve all resources
        Lib3MF::PResourceIterator resourceIterator = m_model->GetResources();

        // First pass: add all resources as vertices
        while (resourceIterator->MoveNext())
        {
            Lib3MF::PResource resource = resourceIterator->GetCurrent();
            m_graph->addVertex(resource->GetResourceID());
        }

        // Reset the iterator
        resourceIterator = m_model->GetResources();

        // Second pass: process each resource and add dependencies
        while (resourceIterator->MoveNext())
        {
            Lib3MF::PResource resource = resourceIterator->GetCurrent();
            Lib3MF_uint32 resourceId = resource->GetResourceID();

            // Process based on resource type
            if (resource->GetResourceID() == 0)
            {
                continue;
            }

            try
            {
                // Attempt to cast to different resource types and process accordingly
                // LevelSet
                Lib3MF::PLevelSet levelSet = std::dynamic_pointer_cast<Lib3MF::CLevelSet>(resource);
                if (levelSet)
                {
                    processLevelSet(levelSet);
                    continue;
                }

                // Function
                Lib3MF::PFunction function = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                if (function)
                {
                    processFunction(function);
                    continue;
                }

                // ComponentsObject
                Lib3MF::PComponentsObject componentsObject =
                  std::dynamic_pointer_cast<Lib3MF::CComponentsObject>(resource);
                if (componentsObject)
                {
                    processComponentsObject(componentsObject);
                    continue;
                }

                // MeshObject
                Lib3MF::PMeshObject meshObject =
                  std::dynamic_pointer_cast<Lib3MF::CMeshObject>(resource);
                if (meshObject)
                {
                    processMeshObject(meshObject);
                    continue;
                }

                // VolumeData
                Lib3MF::PVolumeData volumeData =
                  std::dynamic_pointer_cast<Lib3MF::CVolumeData>(resource);
                if (volumeData)
                {
                    processVolumeData(volumeData);
                    continue;
                }

                // Other resource types can be added as needed
            }
            catch (const std::exception & e)
            {
                if (m_logger)
                    m_logger->addEvent(
                      {fmt::format("Error processing resource {}: {}", resourceId, e.what()),
                       gladius::events::Severity::Error});
            }
        }

        if (m_logger)
            m_logger->addEvent(
              {"Completed building resource dependency graph", gladius::events::Severity::Info});
    }

    const nodes::graph::IDirectedGraph & ResourceDependencyGraph::getGraph() const
    {
        return *m_graph;
    }

    std::vector<Lib3MF::PResource>
    ResourceDependencyGraph::getAllRequiredResources(Lib3MF::PResource resource) const
    {
        std::vector<Lib3MF::PResource> requiredResources;
        if (!resource || !m_model || !m_graph)
        {
            return requiredResources;
        }

        Lib3MF_uint32 resourceId = resource->GetResourceID();
        auto dependencies = gladius::nodes::graph::determineAllDependencies(*m_graph, resourceId);
        requiredResources.reserve(dependencies.size());

        for (auto depId : dependencies)
        {
            try
            {
                Lib3MF::PResource res = m_model->GetResourceByID(depId);
                if (res)
                {
                    requiredResources.push_back(res);
                }
            }
            catch (const std::exception &)
            {
                // Skip missing resource
            }
        }
        return requiredResources;
    }

    std::vector<Lib3MF::PBuildItem>
    ResourceDependencyGraph::findBuildItemsReferencingResource(Lib3MF::PResource resource) const
    {
        std::vector<Lib3MF::PBuildItem> matchingItems;
        if (!resource || !m_model)
        {
            return matchingItems;
        }
        Lib3MF_uint32 targetId = resource->GetResourceID();
        Lib3MF::PBuildItemIterator buildItemIterator = m_model->GetBuildItems();
        while (buildItemIterator->MoveNext())
        {
            Lib3MF::PBuildItem buildItem = buildItemIterator->GetCurrent();
            if (buildItem && buildItem->GetObjectResourceID() == targetId)
            {
                matchingItems.push_back(buildItem);
            }
        }
        return matchingItems;
    }

    CanResourceBeRemovedResult
    ResourceDependencyGraph::checkResourceRemoval(Lib3MF::PResource resourceToBeRemoved) const
    {
        CanResourceBeRemovedResult result;
        result.canBeRemoved = true; // Assume removable until proven otherwise

        if (!resourceToBeRemoved || !m_model || !m_graph)
        {
            result.canBeRemoved = false; // Cannot determine without valid input
            return result;
        }

        Lib3MF_uint32 resourceIdToRemove = resourceToBeRemoved->GetResourceID();

        // 1. Check for dependent resources
        Lib3MF::PResourceIterator resourceIterator = m_model->GetResources();
        while (resourceIterator->MoveNext())
        {
            Lib3MF::PResource currentResource = resourceIterator->GetCurrent();
            if (currentResource && currentResource->GetResourceID() != resourceIdToRemove)
            {
                Lib3MF_uint32 currentResourceId = currentResource->GetResourceID();
                // Check if currentResource directly depends on resourceIdToRemove
                if (m_graph->isDirectlyDependingOn(currentResourceId, resourceIdToRemove))
                {
                    result.dependentResources.push_back(currentResource);
                    result.canBeRemoved = false;
                }
            }
        }

        // 2. Check for dependent build items
        result.dependentBuildItems = findBuildItemsReferencingResource(resourceToBeRemoved);
        if (!result.dependentBuildItems.empty())
        {
            result.canBeRemoved = false;
        }

        return result;
    }

    void ResourceDependencyGraph::processLevelSet(Lib3MF::PLevelSet levelSet)
    {
        if (!levelSet)
        {
            return;
        }

        Lib3MF_uint32 levelSetId = levelSet->GetResourceID();

        // LevelSet depends on its function
        try
        {
            Lib3MF::PFunction function = levelSet->GetFunction();
            if (function)
            {
                Lib3MF_uint32 functionId = function->GetResourceID();
                m_graph->addDependency(levelSetId, functionId);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent(
                  {fmt::format(
                     "Error processing LevelSet function dependency {}: {}", levelSetId, e.what()),
                   gladius::events::Severity::Error});
        }

        // LevelSet may depend on a mesh
        try
        {
            Lib3MF::PMeshObject mesh = levelSet->GetMesh();
            if (mesh)
            {
                Lib3MF_uint32 meshId = mesh->GetResourceID();
                m_graph->addDependency(levelSetId, meshId);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent({fmt::format("Error processing LevelSet mesh dependency {}: {}",
                                                levelSetId,
                                                e.what()),
                                    gladius::events::Severity::Error});
        }

        // LevelSet may have volume data
        try
        {
            Lib3MF::PVolumeData volumeData = levelSet->GetVolumeData();
            if (volumeData)
            {
                Lib3MF_uint32 volumeDataId = volumeData->GetResourceID();
                m_graph->addDependency(levelSetId, volumeDataId);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent(
                  {fmt::format("Error processing LevelSet volume data dependency {}: {}",
                               levelSetId,
                               e.what()),
                   gladius::events::Severity::Error});
        }
    }

    void ResourceDependencyGraph::processFunction(Lib3MF::PFunction function)
    {
        if (!function)
        {
            return;
        }

        Lib3MF_uint32 functionId = function->GetResourceID();

        // Handle ImplicitFunction type (may reference other functions)
        Lib3MF::PImplicitFunction implicitFunction =
          std::dynamic_pointer_cast<Lib3MF::CImplicitFunction>(function);
        if (implicitFunction)
        {
            // Process implicit function nodes and their connections
            // This would require recursively traversing the function graph
            try
            {
                Lib3MF::PNodeIterator nodeIterator = implicitFunction->GetNodes();
                while (nodeIterator->MoveNext())
                {
                    Lib3MF::PImplicitNode node = nodeIterator->GetCurrent();

                    // Handle ResourceIdNode which references another resource
                    Lib3MF::PResourceIdNode resourceIdNode =
                      std::dynamic_pointer_cast<Lib3MF::CResourceIdNode>(node);
                    if (resourceIdNode)
                    {
                        try
                        {
                            Lib3MF::PResource referencedResource = resourceIdNode->GetResource();
                            if (referencedResource)
                            {
                                Lib3MF_uint32 referencedResourceId =
                                  referencedResource->GetResourceID();
                                m_graph->addDependency(functionId, referencedResourceId);
                            }
                        }
                        catch (const std::exception & e)
                        {
                            if (m_logger)
                                m_logger->addEvent(
                                  {fmt::format(
                                     "Error retrieving resource from ResourceIdNode {}: {}",
                                     node->GetIdentifier(),
                                     e.what()),
                                   gladius::events::Severity::Error});
                        }
                        catch (...)
                        {
                            if (m_logger)
                                m_logger->addEvent(
                                  {fmt::format(
                                     "Unknown error retrieving resource from ResourceIdNode {}",
                                     node->GetIdentifier()),
                                   gladius::events::Severity::Error});
                        }
                    }

                    // Handle FunctionCallNode which calls another function
                    Lib3MF::PFunctionCallNode functionCallNode =
                      std::dynamic_pointer_cast<Lib3MF::CFunctionCallNode>(node);
                    if (functionCallNode)
                    {
                        try
                        {
                            // Get input port that contains the function ID reference
                            Lib3MF::PImplicitPort functionIDInput =
                              functionCallNode->GetInputFunctionID();
                            if (functionIDInput && !functionIDInput->GetReference().empty())
                            {
                                // Try to find the referenced resource node through the reference
                                std::string refName = functionIDInput->GetReference();
                                Lib3MF::PNodeIterator nodeIter = implicitFunction->GetNodes();
                                while (nodeIter->MoveNext())
                                {
                                    Lib3MF::PImplicitNode refNode = nodeIter->GetCurrent();
                                    if (refNode->GetIdentifier() + ".Value" == refName)
                                    {
                                        // Check if it's a ResourceIdNode and get the resource
                                        Lib3MF::PResourceIdNode resourceIdNode =
                                          std::dynamic_pointer_cast<Lib3MF::CResourceIdNode>(
                                            refNode);
                                        if (resourceIdNode && resourceIdNode->GetResource())
                                        {
                                            Lib3MF_uint32 referencedFunctionId =
                                              resourceIdNode->GetResource()->GetResourceID();
                                            m_graph->addDependency(functionId,
                                                                   referencedFunctionId);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        catch (const std::exception & e)
                        {
                            if (m_logger)
                                m_logger->addEvent(
                                  {fmt::format(
                                     "Error processing FunctionCallNode in function {}: {}",
                                     functionId,
                                     e.what()),
                                   gladius::events::Severity::Error});
                        }
                    }
                }
            }
            catch (const std::exception & e)
            {
                if (m_logger)
                    m_logger->addEvent({fmt::format("Error processing ImplicitFunction {}: {}",
                                                    functionId,
                                                    e.what()),
                                        gladius::events::Severity::Error});
            }
        }

        // Handle FunctionFromImage3D type
        Lib3MF::PFunctionFromImage3D functionFromImage =
          std::dynamic_pointer_cast<Lib3MF::CFunctionFromImage3D>(function);
        if (functionFromImage)
        {
            try
            {
                Lib3MF::PImage3D image = functionFromImage->GetImage3D();
                if (image)
                {
                    Lib3MF_uint32 imageId = image->GetResourceID();
                    m_graph->addDependency(functionId, imageId);
                }
            }
            catch (const std::exception & e)
            {
                if (m_logger)
                    m_logger->addEvent({fmt::format("Error processing FunctionFromImage3D {}: {}",
                                                    functionId,
                                                    e.what()),
                                        gladius::events::Severity::Error});
            }
        }
    }

    void
    ResourceDependencyGraph::processComponentsObject(Lib3MF::PComponentsObject componentsObject)
    {
        if (!componentsObject)
        {
            return;
        }

        Lib3MF_uint32 componentsObjectId = componentsObject->GetResourceID();

        // Process each component
        try
        {
            Lib3MF_uint32 componentCount = componentsObject->GetComponentCount();
            for (Lib3MF_uint32 i = 0; i < componentCount; i++)
            {
                Lib3MF::PComponent component = componentsObject->GetComponent(i);
                if (component)
                {
                    Lib3MF_uint32 objectResourceId = component->GetObjectResourceID();
                    m_graph->addDependency(componentsObjectId, objectResourceId);
                }
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent({fmt::format("Error processing ComponentsObject {}: {}",
                                                componentsObjectId,
                                                e.what()),
                                    gladius::events::Severity::Error});
        }
    }

    void ResourceDependencyGraph::processMeshObject(Lib3MF::PMeshObject meshObject)
    {
        if (!meshObject)
        {
            return;
        }

        Lib3MF_uint32 meshObjectId = meshObject->GetResourceID();

        // Check for object level property resources
        try
        {
            Lib3MF_uint32 propertyResourceId = 0;
            Lib3MF_uint32 propertyId = 0;

            if (meshObject->GetObjectLevelProperty(propertyResourceId, propertyId))
            {
                m_graph->addDependency(meshObjectId, propertyResourceId);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent({fmt::format("Error processing MeshObject property {}: {}",
                                                meshObjectId,
                                                e.what()),
                                    gladius::events::Severity::Error});
        }

        // Check for beam lattice and its properties
        try
        {
            Lib3MF::PBeamLattice beamLattice = meshObject->BeamLattice();
            if (beamLattice)
            {
                Lib3MF::eBeamLatticeClipMode clipMode;
                Lib3MF_uint32 clipResourceId;
                beamLattice->GetClipping(clipMode, clipResourceId);

                if (clipResourceId > 0)
                {
                    m_graph->addDependency(meshObjectId, clipResourceId);
                }

                Lib3MF_uint32 representationResourceId;
                if (beamLattice->GetRepresentation(representationResourceId))
                {
                    m_graph->addDependency(meshObjectId, representationResourceId);
                }
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent({fmt::format("Error processing MeshObject BeamLattice {}: {}",
                                                meshObjectId,
                                                e.what()),
                                    gladius::events::Severity::Error});
        }

        // Check for volume data
        try
        {
            Lib3MF::PVolumeData volumeData = meshObject->GetVolumeData();
            if (volumeData)
            {
                Lib3MF_uint32 volumeDataId = volumeData->GetResourceID();
                m_graph->addDependency(meshObjectId, volumeDataId);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent({fmt::format("Error processing MeshObject VolumeData {}: {}",
                                                meshObjectId,
                                                e.what()),
                                    gladius::events::Severity::Error});
        }
    }

    void ResourceDependencyGraph::processVolumeData(Lib3MF::PVolumeData volumeData)
    {
        if (!volumeData)
        {
            return;
        }

        Lib3MF_uint32 volumeDataId = volumeData->GetResourceID();

        // Process color function
        try
        {
            Lib3MF::PVolumeDataColor colorData = volumeData->GetColor();
            if (colorData)
            {
                processFunctionReference(colorData, volumeDataId);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent(
                  {fmt::format("Error processing VolumeData color {}: {}", volumeDataId, e.what()),
                   gladius::events::Severity::Error});
        }

        // Process composite material data
        try
        {
            Lib3MF::PVolumeDataComposite composite = volumeData->GetComposite();
            if (composite)
            {
                // Add dependency on base material group
                Lib3MF::PBaseMaterialGroup materialGroup = composite->GetBaseMaterialGroup();
                if (materialGroup)
                {
                    Lib3MF_uint32 materialGroupId = materialGroup->GetResourceID();
                    m_graph->addDependency(volumeDataId, materialGroupId);
                }

                // Process all material mappings
                Lib3MF_uint32 mappingCount = composite->GetMaterialMappingCount();
                for (Lib3MF_uint32 i = 0; i < mappingCount; i++)
                {
                    Lib3MF::PMaterialMapping mapping = composite->GetMaterialMapping(i);
                    if (mapping)
                    {
                        processFunctionReference(mapping, volumeDataId);
                    }
                }
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent({fmt::format("Error processing VolumeData composite {}: {}",
                                                volumeDataId,
                                                e.what()),
                                    gladius::events::Severity::Error});
        }

        // Process property functions
        try
        {
            Lib3MF_uint32 propertyCount = volumeData->GetPropertyCount();
            for (Lib3MF_uint32 i = 0; i < propertyCount; i++)
            {
                Lib3MF::PVolumeDataProperty property = volumeData->GetProperty(i);
                if (property)
                {
                    processFunctionReference(property, volumeDataId);
                }
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent({fmt::format("Error processing VolumeData properties {}: {}",
                                                volumeDataId,
                                                e.what()),
                                    gladius::events::Severity::Error});
        }
    }

    void ResourceDependencyGraph::processFunctionReference(Lib3MF::PFunctionReference functionRef,
                                                           Lib3MF_uint32 resourceId)
    {
        if (!functionRef)
        {
            return;
        }

        try
        {
            Lib3MF_uint32 functionResourceId = functionRef->GetFunctionResourceID();
            if (functionResourceId > 0)
            {
                m_graph->addDependency(resourceId, functionResourceId);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
                m_logger->addEvent(
                  {fmt::format("Error processing FunctionReference for resource {}: {}",
                               resourceId,
                               e.what()),
                   gladius::events::Severity::Error});
        }
    }

    std::vector<Lib3MF::PResource> ResourceDependencyGraph::findUnusedResources() const
    {
        std::vector<Lib3MF::PResource> unusedResources;
        if (!m_model || !m_graph)
        {
            if (m_logger)
                m_logger->addEvent({"Cannot find unused resources: invalid model or graph",
                                    gladius::events::Severity::Error});
            return unusedResources;
        }

        // Get all available resources in the model
        std::unordered_map<Lib3MF_uint32, Lib3MF::PResource> allResources;
        Lib3MF::PResourceIterator resourceIterator = m_model->GetResources();
        while (resourceIterator->MoveNext())
        {
            Lib3MF::PResource resource = resourceIterator->GetCurrent();
            if (resource)
            {
                Lib3MF_uint32 resourceId = resource->GetResourceID();
                if (resourceId > 0)
                {
                    allResources[resourceId] = resource;
                }
            }
        }

        if (allResources.empty())
        {
            return unusedResources;
        }

        // Track which resources are required
        std::unordered_set<Lib3MF_uint32> requiredResourceIds;

        // Iterate through all build items and find their required resources
        Lib3MF::PBuildItemIterator buildItemIterator = m_model->GetBuildItems();
        bool hasBuildItems = false;

        while (buildItemIterator->MoveNext())
        {
            hasBuildItems = true;
            Lib3MF::PBuildItem buildItem = buildItemIterator->GetCurrent();
            if (!buildItem)
                continue;

            // Get the resource directly referenced by this build item
            Lib3MF_uint32 objectResourceId = buildItem->GetObjectResourceID();
            if (objectResourceId == 0)
                continue;

            // Add this resource and all its dependencies to required resources
            requiredResourceIds.insert(objectResourceId);

            // Use the existing graph to find all dependencies of this resource
            auto allDependencies =
              gladius::nodes::graph::determineAllDependencies(*m_graph, objectResourceId);
            requiredResourceIds.insert(allDependencies.begin(), allDependencies.end());
        }

        // If there are no build items, all resources are technically unused,
        // but we might want to keep them (as they could be referenced in future build items)
        if (!hasBuildItems)
        {
            if (m_logger)
                m_logger->addEvent(
                  {"No build items found in model", gladius::events::Severity::Info});
            return unusedResources;
        }

        // Collect all resources that are not in the required set
        unusedResources.reserve(allResources.size() - requiredResourceIds.size());
        for (const auto & [resourceId, resource] : allResources)
        {
            if (requiredResourceIds.find(resourceId) == requiredResourceIds.end())
            {
                unusedResources.push_back(resource);
            }
        }

        if (m_logger && !unusedResources.empty())
        {
            m_logger->addEvent({fmt::format("Found {} unused resources", unusedResources.size()),
                                gladius::events::Severity::Info});
        }

        return unusedResources;
    }

    Lib3MF::PResource ResourceDependencyGraph::getResourceById(Lib3MF_uint32 resourceId) const
    {
        if (!m_model || resourceId == 0)
        {
            return nullptr;
        }

        try
        {

            return m_model->GetResourceByID(resourceIdToUniqueResourceId(m_model, resourceId));
        }
        catch (const std::exception &)
        {
            // Resource not found or other error
            return nullptr;
        }
    }
} // namespace gladius::io
