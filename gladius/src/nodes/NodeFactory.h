#pragma once

#include "NodeBase.h"
#include <memory>
#include <string>
#include <vector>

namespace gladius::nodes
{
    class NodeFactory
    {
      public:
        static std::unique_ptr<NodeBase> createNode(const std::string & nodeType);
        static std::vector<std::string> getValidNodeTypes();
    };
}
