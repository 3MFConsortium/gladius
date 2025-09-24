#pragma once
#include "nodesfwd.h"

#include <typeindex>

namespace gladius::nodes
{
    class Port
    {
      public:
        virtual ~Port() = default;
        [[nodiscard]] virtual auto getDescription() const -> std::string;

        void setUniqueName(const std::string & name);
        void setTypeIndex(std::type_index typeIndex);
        [[nodiscard]] auto getUniqueName() const -> PortName const &;
        [[nodiscard]] auto getShortName() const -> PortName const &;
        void setShortName(const std::string & name);
        void setId(PortId id);
        [[nodiscard]] auto getId() const -> PortId;
        [[nodiscard]] auto getParentId() const -> NodeId;
        void hide();
        void show();
        [[nodiscard]] bool isVisible() const;
        [[nodiscard]] std::type_index getTypeIndex() const;
        void setParent(NodeBase * parent);
        [[nodiscard]] auto getParent() const -> NodeBase *;

        void setIsUsed(bool isUsed);
        [[nodiscard]] bool isUsed() const;

      private:
        std::string m_uniqueName{};
        std::string m_shortName{}; // "e.g. FieldNames::Shape, FieldNames::Pos etc.
        PortId m_id{};
        NodeId m_parentId{};
        NodeBase * m_parent{nullptr};
        bool m_visible{true};
        std::type_index m_typeIndex = std::type_index(typeid(float));
        bool m_isused{false};
    };
} // namespace gladius::nodes
