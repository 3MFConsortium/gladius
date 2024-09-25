#include "Parameter.h"

namespace gladius::nodes
{
    VariantParameter createVariantTypeFromTypeIndex(std::type_index typeIndex)
    {
        if (typeIndex == ParameterTypeIndex::Float)
        {
            return VariantParameter(0.f);
        }
        else if (typeIndex == ParameterTypeIndex::Float3)
        {
            return VariantParameter(float3{});
        }
        else if (typeIndex == ParameterTypeIndex::Matrix4)
        {
            return VariantParameter(Matrix4x4{});
        }
        else if (typeIndex == ParameterTypeIndex::Int)
        {
            return VariantParameter(0);
        }
        else if (typeIndex == ParameterTypeIndex::String)
        {
            return VariantParameter(std::string{});
        }
        else if (typeIndex == ParameterTypeIndex::ResourceId)
        {
            return VariantParameter(ResourceId{});
        }
        return VariantParameter(0);
    }

    std::string to_string(const float3 & val)
    {
        return std::to_string(val.x) + "f, " + //
               std::to_string(val.y) + "f, " + //
               std::to_string(val.z) + "f";
    }

    std::string var_to_string(const VariantType & val)
    {
        if (const auto pval = std::get_if<float>(&val))
        {
            return std::to_string(*pval);
        }

        if (const auto pval = std::get_if<float3>(&val))
        {
            return to_string(*pval);
        }

        if (const auto pval = std::get_if<std::string>(&val))
        {
            return (*pval);
        }

        return "unknown type";
    }
} // namespace gladius::nodes
