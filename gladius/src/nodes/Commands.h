#pragma once
#include <stdexcept>
#include <string>
#include <typeindex>

#include "Model.h"
#include "nodesfwd.h"

namespace gladius
{
    template <typename NodeType>
    int getCommandId()
    {

        static nodes::NodeTypes nodeTypes;
        int id = 0;
        int commandId = -1;
        nodes::staticFor(nodeTypes, [&](auto, auto & node) {
            ++id;

            if (typeid(node) == typeid(NodeType))
            {
                commandId = id;
            }
        });
        if (commandId < 0)
        {
            throw std::runtime_error(std::string{typeid(NodeType).name()} +
                                     std::string{" is not a valid node type"});
        }
        return commandId;
    }
}
