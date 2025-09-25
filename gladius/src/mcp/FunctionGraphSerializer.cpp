#include "FunctionGraphSerializer.h"
#include "../nodes/NodeBase.h"
#include "../nodes/Parameter.h"
#include "../nodes/Port.h"

#include <typeindex>

namespace gladius
{
    using nlohmann::json;

    static std::string paramValueToString(const nodes::VariantParameter & p)
    {
        // Prefer the existing toString() to preserve semantics (arguments show as parameter[x])
        try
        {
            return const_cast<nodes::VariantParameter &>(p).toString();
        }
        catch (...)
        {
            return "";
        }
    }

    const char * FunctionGraphSerializer::typeIndexToString(std::type_index idx)
    {
        using namespace nodes;
        if (idx == ParameterTypeIndex::Float)
            return "float";
        if (idx == ParameterTypeIndex::Float3)
            return "vec3";
        if (idx == ParameterTypeIndex::Matrix4)
            return "mat4";
        if (idx == ParameterTypeIndex::Int)
            return "int";
        if (idx == ParameterTypeIndex::String)
            return "string";
        if (idx == ParameterTypeIndex::ResourceId)
            return "resource_id";
        return "unknown";
    }

    json FunctionGraphSerializer::serialize(const nodes::Model & model)
    {
        json out;
        // Basic model info
        out["model"] = {
          {"resource_id", model.getResourceId()},
          // Model::getModelName() is non-const, so access via const_cast
          {"name", const_cast<nodes::Model &>(model).getModelName()},
        };
        if (auto dn = model.getDisplayName())
        {
            out["model"]["display_name"] = *dn;
        }
        else
        {
            out["model"]["display_name"] = nullptr;
        }

        // Collect nodes
        json jnodes = json::array();
        // We need to iterate internal node registry. Model exposes begin()/end() over m_nodes
        // which yields pairs of NodeId->unique_ptr<NodeBase>.
        for (auto it = const_cast<nodes::Model &>(model).begin();
             it != const_cast<nodes::Model &>(model).end();
             ++it)
        {
            const auto nodeId = it->first;
            const nodes::NodeBase * node = it->second.get();
            if (!node)
                continue;

            json jn;
            jn["id"] = node->getId();
            jn["order"] = node->getOrder();
            jn["name"] = node->name();
            jn["unique_name"] = node->getUniqueName();
            jn["display_name"] = node->getDisplayName();
            jn["category"] = static_cast<int>(node->getCategory());
            // NodeBase::screenPos() is non-const; access via const_cast
            auto pos = const_cast<nodes::NodeBase *>(node)->screenPos();
            jn["position"] = {pos.x, pos.y};

            // Parameters (inputs)
            json jparams = json::array();
            for (auto const & [pname, param] : node->constParameter())
            {
                json jp;
                jp["name"] = pname;
                jp["type"] = typeIndexToString(param.getTypeIndex());
                jp["size"] = param.getSize();
                jp["content_type"] = static_cast<int>(param.getContentType());
                jp["modifiable"] = param.isModifiable();
                jp["is_argument"] = param.isArgument();
                jp["value"] = paramValueToString(param);

                // Source wiring if present
                auto const & srcOpt = param.getConstSource();
                if (srcOpt.has_value())
                {
                    json js;
                    js["node_id"] = srcOpt->nodeId;
                    js["port_id"] = srcOpt->portId;
                    js["unique_name"] = srcOpt->uniqueName;
                    js["short_name"] = srcOpt->shortName;
                    js["type"] = typeIndexToString(srcOpt->type);
                    jp["source"] = js;
                }
                else
                {
                    jp["source"] = nullptr;
                }

                jparams.push_back(jp);
            }
            jn["parameters"] = jparams;

            // Outputs (ports)
            json jouts = json::array();
            for (auto const & [oname, port] : node->outputs())
            {
                json jo;
                jo["name"] = oname;
                jo["id"] = port.getId();
                jo["unique_name"] = port.getUniqueName();
                jo["short_name"] = port.getShortName();
                jo["type"] = typeIndexToString(port.getTypeIndex());
                jo["visible"] = port.isVisible();
                jo["is_used"] = port.isUsed();
                jouts.push_back(jo);
            }
            jn["outputs"] = jouts;

            jnodes.push_back(jn);
        }
        out["nodes"] = jnodes;

        // Links: derive from inputs that have a source
        json jlinks = json::array();
        for (auto it = const_cast<nodes::Model &>(model).begin();
             it != const_cast<nodes::Model &>(model).end();
             ++it)
        {
            const nodes::NodeBase * node = it->second.get();
            if (!node)
                continue;
            for (auto const & [pname, param] : node->constParameter())
            {
                auto const & srcOpt = param.getConstSource();
                if (srcOpt.has_value())
                {
                    json jl;
                    jl["from_node_id"] = srcOpt->nodeId;
                    jl["from_port_id"] = srcOpt->portId;
                    jl["from_port"] = srcOpt->shortName;
                    jl["to_node_id"] = node->getId();
                    jl["to_parameter"] = pname;
                    jl["type"] = typeIndexToString(param.getTypeIndex());
                    jlinks.push_back(jl);
                }
            }
        }
        out["links"] = jlinks;

        // Summaries
        out["counts"] = {{"nodes", jnodes.size()}, {"links", jlinks.size()}};

        return out;
    }

    json FunctionGraphSerializer::serializeMinimal(const nodes::Model & model)
    {
        json out;
        // Basic model info
        out["model"] = {
          {"resource_id", model.getResourceId()},
          {"name", const_cast<nodes::Model &>(model).getModelName()},
        };
        if (auto dn = model.getDisplayName())
            out["model"]["display_name"] = *dn;
        else
            out["model"]["display_name"] = nullptr;

        // Nodes (essential fields only)
        json jnodes = json::array();
        for (auto it = const_cast<nodes::Model &>(model).begin();
             it != const_cast<nodes::Model &>(model).end();
             ++it)
        {
            const nodes::NodeBase * node = it->second.get();
            if (!node)
                continue;

            json jn;
            jn["id"] = node->getId();
            jn["type"] = node->name();
            jn["display_name"] = node->getDisplayName();

            // Minimal parameters
            json jparams = json::array();
            for (auto const & [pname, param] : node->constParameter())
            {
                json jp;
                jp["name"] = pname;
                jp["type"] = typeIndexToString(param.getTypeIndex());
                jp["is_connected"] = param.getConstSource().has_value();
                if (auto const & srcOpt = param.getConstSource(); srcOpt.has_value())
                {
                    jp["source"] = {
                      {"node_id", srcOpt->nodeId},
                      {"port", srcOpt->shortName},
                    };
                }
                jparams.push_back(jp);
            }
            jn["parameters"] = jparams;

            // Minimal outputs
            json jouts = json::array();
            for (auto const & [oname, port] : node->outputs())
            {
                jouts.push_back({
                  {"name", oname},
                  {"type", typeIndexToString(port.getTypeIndex())},
                });
            }
            jn["outputs"] = jouts;

            jnodes.push_back(jn);
        }
        out["nodes"] = jnodes;

        // Links (derived from inputs)
        json jlinks = json::array();
        for (auto it = const_cast<nodes::Model &>(model).begin();
             it != const_cast<nodes::Model &>(model).end();
             ++it)
        {
            const nodes::NodeBase * node = it->second.get();
            if (!node)
                continue;
            for (auto const & [pname, param] : node->constParameter())
            {
                auto const & srcOpt = param.getConstSource();
                if (srcOpt.has_value())
                {
                    jlinks.push_back({
                      {"from_node_id", srcOpt->nodeId},
                      {"from_port", srcOpt->shortName},
                      {"to_node_id", node->getId()},
                      {"to_parameter", pname},
                      {"type", typeIndexToString(param.getTypeIndex())},
                    });
                }
            }
        }
        out["links"] = jlinks;
        out["counts"] = {{"nodes", jnodes.size()}, {"links", jlinks.size()}};
        return out;
    }
}
