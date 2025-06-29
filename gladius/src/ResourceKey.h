#pragma once

#include <optional>
#include <filesystem>
#include <fmt/format.h>
#include "types.h"

namespace gladius
{

    template <typename T, typename... Rest>
    void hash_combine(std::size_t & seed, const T & v, const Rest &... rest)
    {
        seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        (hash_combine(seed, rest), ...);
    }

    class ResourceKey
    {
      public:
        explicit ResourceKey(std::filesystem::path const & filename)
            : m_filename(filename)
        {
        }

        explicit ResourceKey(ResourceId resourceId)
            : m_resourceId(resourceId)
        {
        }

        bool operator==(ResourceKey const & other) const
        {
            return this->getHash() == other.getHash();
        }

        auto getFilename() const
        {
            return m_filename;
        }

        auto getResourceId() const
        {
            return m_resourceId;
        }

        std::size_t getHash() const
        {
            size_t hashValue = 0;
            if (m_filename.has_value())
            {
                hash_combine(hashValue, m_filename.value());
            }
            if (m_resourceId.has_value())
            {
                hash_combine(hashValue, m_resourceId.value());
            }

            if (m_textHash.has_value())
            {
                hash_combine(hashValue, m_textHash.value());
            }

            return hashValue;
        }

        std::string getDisplayName() const
        {
            if (m_displayName.has_value() && !m_displayName.value().empty())
            {
                return m_displayName.value();
            }
            if (m_filename.has_value())
            {
                return m_filename.value().string();
            }

            if (m_resourceId.has_value())
            {
                return fmt::format("3mf resource {}", m_resourceId.value());
            }
            if (m_textHash.has_value())
            {
                return fmt::format("Text: #{}", m_textHash.value());
            }
            return {};
        }

        void setDisplayName(std::string displayName)
        {
            m_displayName = std::move(displayName);
        }

      private:
        std::optional<std::filesystem::path> m_filename{};
        std::optional<ResourceId> m_resourceId{};
        std::optional<size_t> m_textHash{};
        std::optional<std::string> m_displayName;
    };

}

namespace std
{
    template <>
    struct hash<gladius::ResourceKey>
    {
        size_t operator()(gladius::ResourceKey const & key) const noexcept
        {
            return key.getHash();
        }
    };
}