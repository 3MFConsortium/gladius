#pragma once

#include "NodeBase.h"

#include "ClonableNode.h"

#include <concepts>

namespace gladius::nodes
{

    template <typename T>
    concept IsDerivedFromNodeBase = std::derived_from<T, NodeBase>;

    // Mixed in class adding specialized accessors
    template <IsDerivedFromNodeBase Base>
    class WithInputA : public Base
    {
      public:
        WithInputA(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        void setInputA(const Port & port)
        {
            NodeBase::m_parameter[FieldNames::A].setInputFromPort(port);
        }

        const VariantParameter & getInputA() const
        {
            return NodeBase::m_parameter.at(FieldNames::A);
        }
    };

    template <IsDerivedFromNodeBase Base>
    class WithInputB : public Base
    {
      public:
        WithInputB(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        void setInputB(const Port & port)
        {
            NodeBase::m_parameter[FieldNames::B].setInputFromPort(port);
        }

        const VariantParameter & getInputB() const
        {
            return NodeBase::m_parameter.at(FieldNames::B);
        }
    };

    template <IsDerivedFromNodeBase Base>
    class WithOutputResult : public Base
    {
      public:
        WithOutputResult(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        const Port & getOutputResult() const
        {
            return NodeBase::m_outputs.at(FieldNames::Result);
        }
    };

    template <IsDerivedFromNodeBase Base>
    using WithInputAB = WithInputB<WithInputA<Base>>;
    template <typename Base>
    using CloneableABtoResult = WithOutputResult<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoResult= WithOutputResult<WithInputA<ClonableNode<Base>>>;
}