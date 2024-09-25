#pragma once

#include "Components.h"
#include "Object.h"

#include "types.h"

#include <fmt/format.h>
#include <string>
#include <vector>

namespace gladius::nodes
{

    class BuildItem
    {
      public:
        BuildItem() = default;

        BuildItem(ResourceId id, Matrix4x4 const & transform, std::string const & partNumber)
            : m_id(id)
            , m_transform(transform)
            , m_partNumber(partNumber)
        {
            m_name = fmt::format("BuildItem_{}", id);
        }

        [[nodiscard]] Matrix4x4 const & getTransform() const
        {
            return m_transform;
        }

        [[nodiscard]] ResourceId getId() const
        {
            return m_id;
        }

        [[nodiscard]] std::string const & getName() const
        {
            return m_name;
        }

        [[nodiscard]] std::string const & getPartNumber() const
        {
            return m_partNumber;
        }

        Components::iterator addComponent(Component && component)
        {
            m_components.emplace_back(std::move(component));
            return std::prev(m_components.end());
        }

        [[nodiscard]] Components const & getComponents() const
        {
            return m_components;
        }

      private:
        Matrix4x4 m_transform;
        // Object m_object;
        ResourceId m_id{};
        std::string m_name;
        std::string m_partNumber;

        Components m_components;
    };

    using BuildItems = std::vector<BuildItem>;

} // namespace gladius::nodes