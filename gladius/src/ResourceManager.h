#pragma once
#include "ImageRGBA.h"
#include "Primitives.h"
#include "ResourceKey.h"
#include "types.h"

#include <fmt/format.h>
#include <openvdb/Grid.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace gladius
{
    namespace vdb
    {
        struct TriangleMesh;
    }

    namespace io
    {
        class ImageStack;
    }

    class Mesh;
    class ResourceContext;
    class ImageStackOCLBuffer;
    class BeamLatticeResource;

    using TextureBuffer = ImageImpl<cl_float4>;

    struct ImageObject
    {
        std::string name;
        std::shared_ptr<TextureBuffer> texture;
    };

    class IResource
    {
      public:
        virtual ~IResource() = default;

        /**
         * \brief
         * \return true if a resource has been loaded and rewiring the buffer might be
         * necessary
         */
        virtual bool load() = 0;
        virtual void write(Primitives & primitives) = 0;

        virtual std::filesystem::path getFilename() = 0;
        virtual int getStartIndex() const = 0;
        virtual int getEndIndex() const = 0;

        virtual bool isInUse() const = 0;
        virtual void setInUse(bool inUse) = 0;

      private:
    };

    using ResourceMap = std::unordered_map<ResourceKey, std::unique_ptr<IResource>>;

    class ResourceBase : public IResource
    {
      public:
        explicit ResourceBase(ResourceKey key)
            : m_filename(key.getFilename().value_or(std::filesystem::path{}))
        {
        }

        std::filesystem::path getFilename() override
        {
            return m_filename;
        }

        int getStartIndex() const override
        {
            return m_startIndex;
        }

        int getEndIndex() const override
        {
            return m_endIndex;
        }

        bool load() override
        {
            if (m_alreadyLoaded)
            {
                return false;
            }
            loadImpl();
            m_alreadyLoaded = true;
            return true;
        }

        void write(Primitives & primitives) override
        {
            m_startIndex = static_cast<int>(primitives.primitives.getSize());
            primitives.add(m_payloadData);
            m_endIndex = static_cast<int>(primitives.primitives.getSize());
        }

        bool isInUse() const override
        {
            return m_inUse;
        }

        void setInUse(bool inUse) override
        {
            m_inUse = inUse;
        }

      protected:
        int m_startIndex{};
        int m_endIndex{};
        PrimitiveBuffer m_payloadData;
        virtual void loadImpl()
        {
        }

      private:
        std::filesystem::path m_filename;
        bool m_alreadyLoaded = false;
        bool m_inUse = false;
    };
    class ResourceManager
    {
      public:
        explicit ResourceManager(SharedResources resourceContext,
                                 std::filesystem::path assemblyDir);

        void addResource(const std::filesystem::path & filename);
        void addResource(ResourceKey key, vdb::TriangleMesh && mesh);
        void addResource(ResourceKey key, openvdb::GridBase::Ptr && grid);
        void addResource(ResourceKey key, io::ImageStack && stack);
        void addResource(ResourceKey key, std::unique_ptr<BeamLatticeResource> && resource);

        /**
         * \brief Loads all resources that have not been load yet
         * \note non blocking
         */
        void loadResources();

        /**
         * \brief Writes the primitive buffers if necessary
         * \param primitives The output target
         */
        void writeResources(Primitives & primitives);

        void clear();

        IResource & getResource(ResourceKey const & key) const;
        IResource * getResourcePtr(ResourceKey const & key);

        ResourceMap const & getResourceMap() const;

        bool hasResource(ResourceKey const & key) const;

        void deleteResource(ResourceKey const & key);

      private:
        void increaseImageNumber();
        std::map<std::filesystem::path, ImageObject> m_textures;
        int m_nameCounter = 0;
        SharedResources m_resourceContext{nullptr};
        std::filesystem::path m_assemblyDir;

        ResourceMap m_resources;
        bool m_bufferChanged = false;
    };
}
