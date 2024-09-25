#pragma once

#include <string>

#include "NodeBase.h"
#include "Visitor.h"
#include "nodesfwd.h"

namespace gladius::nodes
{

    template <typename Derived>
    class ClonableNode : public NodeBase
    {
      public:
        ClonableNode(const NodeName & baseName, NodeId internalId, Category category)
            : NodeBase(baseName, internalId, category)
        {
        }

        void accept(Visitor & visitor) override
        {
            visitor.visit(static_cast<Derived &>(*this));
        }
      private:
        ClonableNode * cloneImpl() const override
        {
            return new Derived(static_cast<const Derived &>(*this));
        }
    };
} // namespace gladius::nodes
