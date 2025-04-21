#include "ResourceDependencyGraph.h"
#include "nodes/graph/DirectedGraph.h"
#include "nodes/graph/GraphAlgorithms.h"
#include <exception>

namespace gladius::io
{
    ResourceDependencyGraph::ResourceDependencyGraph(Lib3MF::PModel model)
        : m_model(model), m_graph(std::make_unique<nodes::graph::DirectedGraph>(100))
    {
    }

    void ResourceDependencyGraph::buildGraph()
    {
        if (!m_model)
        {
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
                Lib3MF::PComponentsObject componentsObject = std::dynamic_pointer_cast<Lib3MF::CComponentsObject>(resource);
                if (componentsObject)
                {
                    processComponentsObject(componentsObject);
                    continue;
                }

                // MeshObject
                Lib3MF::PMeshObject meshObject = std::dynamic_pointer_cast<Lib3MF::CMeshObject>(resource);
                if (meshObject)
                {
                    processMeshObject(meshObject);
                    continue;
                }

                // VolumeData
                Lib3MF::PVolumeData volumeData = std::dynamic_pointer_cast<Lib3MF::CVolumeData>(resource);
                if (volumeData)
                {
                    processVolumeData(volumeData);
                    continue;
                }

                // Other resource types can be added as needed
            }
            catch (const std::exception& e)
            {
                // Log error and continue with next resource
                // We don't want to halt the entire process if one resource fails
            }
        }
    }

    const nodes::graph::IDirectedGraph& ResourceDependencyGraph::getGraph() const
    {
        return *m_graph;
    }

    std::vector<Lib3MF::PResource> ResourceDependencyGraph::getAllRequiredResources(Lib3MF::PResource resource) const
    {
        std::vector<Lib3MF::PResource> requiredResources;
        if (!resource || !m_model || !m_graph)
        {
            return requiredResources;
        }

        Lib3MF_uint32 resourceId = resource->GetResourceID();
        // Get all dependencies (resource IDs) for the given resource
        auto dependencies = gladius::nodes::graph::determineAllDependencies(*m_graph, resourceId);
        if (dependencies.empty())
        {
            return requiredResources;
        }

        // Map resource IDs back to PResource using the model's resource iterator
        Lib3MF::PResourceIterator resourceIterator = m_model->GetResources();
        while (resourceIterator->MoveNext())
        {
            Lib3MF::PResource res = resourceIterator->GetCurrent();
            if (dependencies.find(res->GetResourceID()) != dependencies.end())
            {
                requiredResources.push_back(res);
            }
        }
        return requiredResources;
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
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
        Lib3MF::PImplicitFunction implicitFunction = std::dynamic_pointer_cast<Lib3MF::CImplicitFunction>(function);
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
                    Lib3MF::PResourceIdNode resourceIdNode = std::dynamic_pointer_cast<Lib3MF::CResourceIdNode>(node);
                    if (resourceIdNode)
                    {
                        Lib3MF::PResource referencedResource = resourceIdNode->GetResource();
                        if (referencedResource)
                        {
                            Lib3MF_uint32 referencedResourceId = referencedResource->GetResourceID();
                            m_graph->addDependency(functionId, referencedResourceId);
                        }
                    }

                    // Handle FunctionCallNode which calls another function
                    Lib3MF::PFunctionCallNode functionCallNode = std::dynamic_pointer_cast<Lib3MF::CFunctionCallNode>(node);
                    if (functionCallNode)
                    {
                        try
                        {
                            // Get input port that contains the function ID reference
                            Lib3MF::PImplicitPort functionIDInput = functionCallNode->GetInputFunctionID();
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
                                        Lib3MF::PResourceIdNode resourceIdNode = std::dynamic_pointer_cast<Lib3MF::CResourceIdNode>(refNode);
                                        if (resourceIdNode && resourceIdNode->GetResource())
                                        {
                                            Lib3MF_uint32 referencedFunctionId = resourceIdNode->GetResource()->GetResourceID();
                                            m_graph->addDependency(functionId, referencedFunctionId);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        catch (const std::exception& e)
                        {
                            // Handle error getting function ID
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                // Handle error
            }
        }

        // Handle FunctionFromImage3D type
        Lib3MF::PFunctionFromImage3D functionFromImage = std::dynamic_pointer_cast<Lib3MF::CFunctionFromImage3D>(function);
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
            catch (const std::exception& e)
            {
                // Handle error
            }
        }
    }

    void ResourceDependencyGraph::processComponentsObject(Lib3MF::PComponentsObject componentsObject)
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
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
        catch (const std::exception& e)
        {
            // Handle error
        }
    }

    void ResourceDependencyGraph::processFunctionReference(Lib3MF::PFunctionReference functionRef, Lib3MF_uint32 resourceId)
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
        catch (const std::exception& e)
        {
            // Handle error
        }
    }
} // namespace gladius::io
