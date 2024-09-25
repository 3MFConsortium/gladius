#include "Port.h"
#include "NodeBase.h"

namespace gladius::nodes
{
    auto Port::getDescription() const -> std::string
    {
        return "noname";
    }

    void Port::setUniqueName(const std::string & name)
    {
        m_uniqueName = name;
    }

    void Port::setTypeIndex(std::type_index typeIndex)
    {
        m_typeIndex = typeIndex;
    }

    [[nodiscard]] auto Port::getUniqueName() const -> PortName const &
    {
        return m_uniqueName;
    }

    [[nodiscard]] auto Port::getShortName() const -> PortName const &
    {
        return m_shortName;
    }

    void Port::setShortName(const std::string & name)
    {
        m_shortName = name;
    }

    void Port::setId(PortId id)
    {
        m_id = id;
    }

    [[nodiscard]] auto Port::getId() const -> PortId
    {
        return m_id;
    }

    [[nodiscard]] auto Port::getParentId() const -> NodeId
    {
        return m_parentId;
    }

    void Port::hide()
    {
        m_visible = false;
    }

    void Port::show()
    {
        m_visible = true;
    }

    [[nodiscard]] bool Port::isVisible() const
    {
        return m_visible;
    }

    [[nodiscard]] std::type_index Port::getTypeIndex() const
    {
        return m_typeIndex;
    }

    void Port::setParent(NodeBase * parent)
    {   
        if (parent == nullptr)
        {
            throw std::runtime_error("Port::setParent: parent is nullptr");
        }
        m_parent = parent;
        m_parentId = parent->getId();
    }

    [[nodiscard]] auto Port::getParent() const -> NodeBase *
    {
        return m_parent;
    }

    void Port::setIsUsed(bool isUsed)
    {
        m_isused = isUsed;
    }

    [[nodiscard]] bool Port::isUsed() const
    {
        return m_isused;
    }
} // namespace gladius::nodes
