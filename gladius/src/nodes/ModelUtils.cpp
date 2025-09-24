#include "ModelUtils.h"

namespace gladius::nodes
{
    bool isQualifiedForLevelset(Model & model)
    {
        // Check for Begin node with float3 pos parameter
        const Begin * beginNode = model.getBeginNode();
        if (!beginNode)
        {
            return false;
        }

        // Check if Begin node has a "pos" output port of type float3
        const auto & outputs = beginNode->getOutputs();
        auto posPortIt = outputs.find(FieldNames::Pos);
        if (posPortIt == outputs.end() ||
            posPortIt->second.getTypeIndex() != ParameterTypeIndex::Float3)
        {
            return false;
        }

        // Check if the "pos" port is the only output port
        if (outputs.size() != 1)
        {
            return false;
        }

        // Check for End node with at least one float parameter
        End * endNode = model.getEndNode();
        if (!endNode)
        {
            return false;
        }

        // Check if End node has at least one float parameter
        const auto & params = endNode->parameter();
        bool hasScalarParam = false;

        for (const auto & [name, param] : params)
        {
            if (param.getTypeIndex() == ParameterTypeIndex::Float)
            {
                hasScalarParam = true;
                break;
            }
        }

        return hasScalarParam;
    }

    bool isQualifiedForVolumeColor(Model & model)
    {
        // Check for Begin node with float3 pos parameter
        const Begin * beginNode = model.getBeginNode();
        if (!beginNode)
        {
            return false;
        }

        // Check if Begin node has a "pos" input port of type float3
        const auto & inputs = beginNode->getOutputs();
        auto posPortIt = inputs.find(FieldNames::Pos);
        if (posPortIt == inputs.end() ||
            posPortIt->second.getTypeIndex() != ParameterTypeIndex::Float3)
        {
            return false;
        }

        // Check for End node with at least one float3 output parameter
        End * endNode = model.getEndNode();
        if (!endNode)
        {
            return false;
        }

        // Check if End node has at least one float3 output parameter
        const auto & params = endNode->parameter();
        bool hasScalarParam = false;

        for (const auto & [name, param] : params)
        {
            if (param.getTypeIndex() == ParameterTypeIndex::Float3)
            {
                hasScalarParam = true;
                break;
            }
        }

        return hasScalarParam;
    }
} // namespace gladius::nodes