#include "NodeBase.h"
#include "Model.h"

namespace gladius::nodes
{
    /// Equal Operator for InputTypeMap
    std::type_index const AnyTypeIndex = std::type_index(typeid(void));

    bool operator==(const InputTypeMap & lhs, const InputTypeMap & rhs)
    {
        return lhs.size() == rhs.size() && std::equal(lhs.begin(),
                                                      lhs.end(),
                                                      rhs.begin(),
                                                      [](const auto & lhsPair, const auto & rhsPair)
                                                      {
                                                          return lhsPair.second == rhsPair.second ||
                                                                 rhsPair.second == AnyTypeIndex ||
                                                                 lhsPair.second == AnyTypeIndex;
                                                      });
    }

    bool NodeBase::updateTypes(nodes::Model & model)
    {

        if (m_typeRules.empty())
        {
            return true;
        }

        if (m_typeRules.size() == 1)
        {
            applyTypeRule(m_typeRules.front());
            return true;
        }

        std::type_index type = ParameterTypeIndex::Float;
        InputTypeMap inputTypeMap;
        for (auto & parameter : m_parameter)
        {
            auto source = parameter.second.getSource();

            if (source.has_value())
            {
                auto sourceNode = model.getNode(source->nodeId);
                if (!sourceNode.has_value())
                {
                    continue;
                }

                try
                {
                    auto sourcePort = model.getPortRegistry().at(source->portId);
                    if (sourcePort)
                    {
                        type = sourcePort->getTypeIndex();
                    }
                }
                catch (const std::exception & e)
                {
                    std::cerr << e.what() << '\n';
                }

                inputTypeMap.insert({parameter.first, type});
            }
            inputTypeMap.insert({parameter.first, AnyTypeIndex});
        }

    
        // find the TypeRule for the current input type in m_typeRules
        auto ruleIter = std::find_if(m_typeRules.begin(),
                                     m_typeRules.end(),
                                     [&inputTypeMap](const TypeRule & rule)
                                     {
                                        if (rule.input.size() != inputTypeMap.size())   // this happens for nodes that can use scalar or vector types
                                        {
                                            // find the rule where at least one type matches
                                            return std::any_of(inputTypeMap.begin(),
                                                               inputTypeMap.end(),
                                                               [&rule](const auto & inputPair)
                                                               {
                                                                   return rule.input.find(inputPair.first) != rule.input.end() &&
                                                                          (rule.input.at(inputPair.first) == inputPair.second);
                                                               });
                                        }
                                        return rule.input == inputTypeMap;
                                    });

        // if we have a match, apply the output types
        if (ruleIter != m_typeRules.end())
        {
            applyTypeRule(*ruleIter);
            return true;
        }
        return false;
    };

} // namespace gladius::nodes
