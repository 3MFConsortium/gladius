#pragma once

#include "Model.h"
#include "Parameter.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    /**
     * @brief Checks if a Model satisfies the criteria to be used as a levelset
     * 
     * A Model is qualified for levelset if:
     * 1. It has a Begin node with only a float3 input parameter named "pos"
     * 2. It has an End node with at least one scalar (float) parameter
     * 
     * @param model The Model to check
     * @return true if the Model qualifies as a levelset, false otherwise
     */
    bool isQualifiedForLevelset(Model& model);

    /**
     * @brief Checks if a Model satisfies the criteria to be used as a volume color function
     * 
     * A Model is qualified for volume color if:
     * 1. It has a Begin node with a float3 input parameter named "pos"
     * 2. It has an End node with at least one float3 output parameter
     * 
     * @param model The Model to check
     * @return true if the Model qualifies as a volume color function, false otherwise
     */
    bool isQualifiedForVolumeColor(Model& model);
} // namespace gladius::nodes