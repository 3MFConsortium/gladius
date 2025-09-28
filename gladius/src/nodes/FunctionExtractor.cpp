#include "FunctionExtractor.h"

#include "NodeBase.h"
#include <queue>
#include <unordered_set>

namespace gladius::nodes
{
    using std::string;

    static Port * findOutputPortById(Model & model, PortId id)
    {
        return model.getPort(id);
    }

    static bool isBeginOrEnd(NodeBase const * n)
    {
        return dynamic_cast<Begin const *>(n) != nullptr || dynamic_cast<End const *>(n) != nullptr;
    }

    string FunctionExtractor::makeUnique(string base, std::unordered_set<string> & used)
    {
        if (base.empty())
            base = "arg";
        string name = base;
        int i = 1;
        while (used.count(name))
        {
            name = base + "_" + std::to_string(i++);
        }
        used.insert(name);
        return name;
    }

    bool FunctionExtractor::extractInto(Model & sourceModel,
                                        Model & newModel,
                                        std::set<NodeId> const & selection,
                                        Result & outResult)
    {
        outResult = {};
        if (selection.empty())
        {
            return false;
        }

        // Validate selection
        for (NodeId id : selection)
        {
            auto opt = sourceModel.getNode(id);
            if (!opt.has_value())
                return false;
            if (isBeginOrEnd(opt.value()))
                return false; // don't allow extracting markers
        }

        // Identify boundary inputs (edges from outside -> selected) and outputs (selected ->
        // outside)
        struct ExtIn
        {
            VariantParameter * targetParam{nullptr}; // in source
            std::string targetParamName;             // name within parent
            Port * externalPort{nullptr};            // source port from outside node
        };
        struct ExtOut
        {
            Port * srcPort{nullptr};                   // source port within selection
            std::vector<VariantParameter *> consumers; // target params outside selection
        };

        std::vector<ExtIn> extInputs;
        std::unordered_map<std::string, ExtOut> extOutputsByUniquePort; // key = uniqueName

        for (NodeId id : selection)
        {
            auto nodeOpt = sourceModel.getNode(id);
            if (!nodeOpt.has_value())
                continue;
            NodeBase * node = nodeOpt.value();

            // traverse parameters to find incoming links
            for (auto & [pname, param] : node->parameter())
            {
                auto src = param.getSource();
                if (!src.has_value())
                    continue;
                Port * port = findOutputPortById(sourceModel, src->portId);
                if (!port)
                    continue;
                NodeId srcNodeId = port->getParentId();
                bool srcInside = selection.count(srcNodeId) > 0;
                if (!srcInside)
                {
                    // external input to selection
                    extInputs.push_back(ExtIn{&param, pname, port});
                }
            }

            // traverse outputs to find outgoing links
            for (auto & [oname, oport] : node->getOutputs())
            {
                // Need to locate all consumers of this port
                // Iterate all model parameters to find targets
                std::vector<VariantParameter *> externalConsumers;
                for (auto & [inParamId, iptr] : sourceModel.getParameterRegistry())
                {
                    VariantParameter * v = dynamic_cast<VariantParameter *>(iptr);
                    if (!v)
                        continue;
                    auto s = v->getSource();
                    if (!s.has_value())
                        continue;
                    if (s->portId != oport.getId())
                        continue;
                    // consumer's parent inside selection?
                    if (selection.count(v->getParentId()) == 0)
                    {
                        externalConsumers.push_back(v);
                    }
                }
                if (!externalConsumers.empty())
                {
                    extOutputsByUniquePort[oport.getUniqueName()] =
                      ExtOut{&const_cast<Port &>(oport), externalConsumers};
                }
            }
        }

        // Prepare new model structure
        newModel.clear();
        newModel.createBeginEnd();

        // Build maps oldId->cloned pointer in new model
        std::unordered_map<NodeId, NodeBase *> cloneMap;

        // Clone nodes
        for (NodeId id : selection)
        {
            auto nodeOpt = sourceModel.getNode(id);
            if (!nodeOpt.has_value())
                continue;
            std::unique_ptr<NodeBase> cloned = nodeOpt.value()->clone();
            // reset unique name scope to new model; Model::insert will re-id and register IOs
            NodeBase * inserted = newModel.insert(std::move(cloned));
            cloneMap[id] = inserted;
        }

        // Recreate intra-selection links among cloned nodes
        for (NodeId id : selection)
        {
            auto origOpt = sourceModel.getNode(id);
            if (!origOpt.has_value())
                continue;
            NodeBase * orig = origOpt.value();
            NodeBase * clone = cloneMap[id];
            for (auto & [pname, param] : orig->parameter())
            {
                auto src = param.getSource();
                if (!src.has_value())
                    continue;
                Port * srcPort = sourceModel.getPort(src->portId);
                if (!srcPort)
                    continue;
                NodeId srcNodeId = srcPort->getParentId();
                if (selection.count(srcNodeId) == 0)
                    continue; // handled later via Begin
                // find cloned source port by short name on the cloned src node
                NodeBase * clonedSrcNode = cloneMap[srcNodeId];
                Port * clonedSrcPort = clonedSrcNode->findOutputPort(srcPort->getShortName());
                VariantParameter * clonedTarget = clone->getParameter(pname);
                if (clonedSrcPort && clonedTarget)
                {
                    newModel.addLink(clonedSrcPort->getId(), clonedTarget->getId(), true);
                }
            }
        }

        // Determine function inputs: group extInputs by externalPort uniqueName -> name
        std::unordered_map<std::string, std::string> extPortToArgName;
        std::unordered_set<string> usedArgNames{"pos"};

        for (auto const & ext : extInputs)
        {
            auto uname = ext.externalPort->getUniqueName();
            auto shortName = ext.externalPort->getShortName();
            if (!extPortToArgName.count(uname))
            {
                // propose argument name from short port name
                string base = shortName;
                if (base.empty())
                    base = "arg";
                auto argName = makeUnique(base, usedArgNames);
                extPortToArgName[uname] = argName;
                // create begin output
                newModel.getBeginNode()->addOutputPort(argName, ext.externalPort->getTypeIndex());
                newModel.registerOutputs(*newModel.getBeginNode());
            }
        }

        // Wire function inputs to cloned targets
        for (auto const & ext : extInputs)
        {
            auto uname = ext.externalPort->getUniqueName();
            auto argName = extPortToArgName[uname];
            // In cloned graph, find the cloned node of targetParam parent and connect from begin
            // arg
            NodeId targetParentId = ext.targetParam->getParentId();
            auto iterCloneNode = cloneMap.find(targetParentId);
            if (iterCloneNode == cloneMap.end())
                continue;
            NodeBase * clonedTargetNode = iterCloneNode->second;
            VariantParameter * clonedParam = clonedTargetNode->getParameter(ext.targetParamName);
            auto & beginOutputs = newModel.getBeginNode()->getOutputs();
            auto bo = beginOutputs.find(argName);
            if (clonedParam && bo != beginOutputs.end())
            {
                newModel.addLink(bo->second.getId(), clonedParam->getId(), true);
            }
        }

        // Determine function outputs
        std::unordered_set<string> usedOutNames;
        for (auto & [uport, info] : extOutputsByUniquePort)
        {
            auto shortName = info.srcPort->getShortName();
            string base = shortName.empty() ? string("out") : shortName;
            string outName = makeUnique(base, usedOutNames);
            outResult.outputNameMap[uport] = outName;
            // create end input for output
            VariantParameter vp = createVariantTypeFromTypeIndex(info.srcPort->getTypeIndex());
            newModel.addFunctionOutput(outName, vp);
            // connect cloned selected src to End
            // find cloned source node
            NodeId srcNodeId = info.srcPort->getParentId();
            auto itClone = cloneMap.find(srcNodeId);
            if (itClone == cloneMap.end())
                continue;
            NodeBase * clonedSrcNode = itClone->second;
            Port * clonedOut = clonedSrcNode->findOutputPort(info.srcPort->getShortName());
            auto & endParams = newModel.getEndNode()->parameter();
            auto ep = endParams.find(outName);
            if (clonedOut && ep != endParams.end())
            {
                newModel.addLink(clonedOut->getId(), ep->second.getId(), true);
            }
        }

        // Update node IDs and consistency
        newModel.getBeginNode()->updateNodeIds();
        newModel.getEndNode()->updateNodeIds();
        newModel.updateGraphAndOrderIfNeeded();
        newModel.updateTypes();

        // Insert FunctionCall into source and rewire
        auto * funcCall = sourceModel.create<FunctionCall>();
        outResult.functionCall = funcCall;

        // Update call signature from newModel
        funcCall->updateInputsAndOutputs(newModel);
        sourceModel.registerInputs(*funcCall);
        sourceModel.registerOutputs(*funcCall);

        // If destination model has a valid resource id, set it on the call now
        // so downstream updates can resolve the referenced model correctly.
        if (newModel.getResourceId() != 0)
        {
            funcCall->setFunctionId(newModel.getResourceId());
        }

        // Wire inputs: for each ext input mapping, connect from original external port to func arg
        for (auto const & ext : extInputs)
        {
            auto uname = ext.externalPort->getUniqueName();
            auto argName = extPortToArgName[uname];
            auto iter = funcCall->parameter().find(argName);
            if (iter == funcCall->parameter().end())
                continue;
            sourceModel.addLink(ext.externalPort->getId(), iter->second.getId(), true);
            outResult.inputNameMap[uname] = argName;
        }

        // Wire outputs: for each ext output, connect FunctionCall's output to original consumers
        for (auto & [uport, info] : extOutputsByUniquePort)
        {
            auto outName = outResult.outputNameMap[uport];
            auto oiter = funcCall->getOutputs().find(outName);
            if (oiter == funcCall->getOutputs().end())
                continue;
            for (auto * consumer : info.consumers)
            {
                // Replace original connection with function output
                sourceModel.addLink(oiter->second.getId(), consumer->getId(), true);
            }
        }

        // Finally, remove the selected nodes from the source model
        for (NodeId id : selection)
        {
            sourceModel.remove(id);
        }

        sourceModel.updateGraphAndOrderIfNeeded();
        sourceModel.updateTypes();

        return true;
    }
} // namespace gladius::nodes
