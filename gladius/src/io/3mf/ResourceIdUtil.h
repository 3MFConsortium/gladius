#pragma once

#include <lib3mf_types.hpp>
#include <lib3mf_implicit.hpp>
#include "types.h"

namespace gladius::io
{
    /**
     * @brief Converts a lib3mf ModelResourceID to a gladius ResourceId
     * 
     * In the Gladius application, lib3mf ModelResourceIDs are used directly as gladius ResourceIds.
     * This utility function makes the conversion explicit and centralized.
     * 
     * @param modelResourceId The lib3mf ModelResourceID to convert
     * @return ResourceId The converted gladius ResourceId
     */
    inline ResourceId modelResourceIdToResourceId(Lib3MF_uint32 modelResourceId)
    {
        return static_cast<ResourceId>(modelResourceId);
    }

    /**
     * @brief Converts a gladius ResourceId to a lib3mf ModelResourceID
     * 
     * This is the inverse operation of modelResourceIdToResourceId.
     * 
     * @param resourceId The gladius ResourceId to convert
     * @return Lib3MF_uint32 The converted lib3mf ModelResourceID
     */
    inline Lib3MF_uint32 resourceIdToModelResourceId(ResourceId resourceId)
    {
        return static_cast<Lib3MF_uint32>(resourceId);
    }

    /**
     * @brief Sets a lib3mf transform to identity matrix
     *
     * A utility function to initialize a 3MF transform as an identity matrix.
     * The identity matrix has 1's on the diagonal and 0's elsewhere.
     *
     * @param transform Reference to the Lib3MF::sTransform to be set to identity
     */
    inline void setTransformToIdentity(Lib3MF::sTransform& transform)
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                transform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    /**
     * @brief Converts a lib3mf UniqueResourceID to a gladius ResourceId
     * 
     * This conversion requires access to the Model to find the resource
     * by its UniqueResourceID and get its ModelResourceID.
     * 
     * @param model The lib3mf Model containing the resource
     * @param uniqueResourceId The lib3mf UniqueResourceID to convert
     * @return ResourceId The converted gladius ResourceId
     */
    inline ResourceId uniqueResourceIdToResourceId(Lib3MF::PModel model, Lib3MF_uint32 uniqueResourceId)
    {
        try
        {
            // Get the resource with the given UniqueResourceID
            auto resource = model->GetResourceByID(uniqueResourceId);
            
            // Return the ModelResourceID (which corresponds to gladius ResourceId)
            return modelResourceIdToResourceId(resource->GetModelResourceID());
        }
        catch (const std::exception&)
        {
            // Return an invalid ResourceId if the resource doesn't exist
            return 0;
        }
    }

    /**
     * @brief Converts a gladius ResourceId to a lib3mf UniqueResourceID
     * 
     * This conversion requires access to the Model to find the resource
     * by its ModelResourceID and get its UniqueResourceID.
     * 
     * @param model The lib3mf Model containing the resource
     * @param resourceId The gladius ResourceId to convert
     * @return Lib3MF_uint32 The converted lib3mf UniqueResourceID, or 0 if not found
     */
    inline Lib3MF_uint32 resourceIdToUniqueResourceId(Lib3MF::PModel model, ResourceId resourceId)
    {
        // Convert the ResourceId to a ModelResourceID
        Lib3MF_uint32 modelResourceId = resourceIdToModelResourceId(resourceId);
        
        // Get all resources from the model and find the one with matching ModelResourceID
        auto resourceIterator = model->GetResources();
        
        // Iterate through all resources
        while (resourceIterator->MoveNext())
        {
            auto resource = resourceIterator->GetCurrent();
            if (resource->GetModelResourceID() == modelResourceId)
            {
                return resource->GetUniqueResourceID();
            }
        }
        
        // Return 0 if no resource with the given ModelResourceID was found
        return 0;
    }
}