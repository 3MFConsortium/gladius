#pragma once

#include "NodeBase.h"
#include <memory>
#include <string>

namespace gladius::nodes
{
    class NodeFactory
    {
      public:
        static std::unique_ptr<NodeBase> createNode(const std::string & nodeType);
    };
}
