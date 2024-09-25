#include "ResourceManager.h"

#include "ImageStackResource.h"
#include "ResourceContext.h"
#include "StlResource.h"
#include "VdbImporter.h"
#include "VdbResource.h"

#include <fmt/format.h>
#include <lodepng.h>

#include "MeshResource.h"

namespace gladius
{
    void StlResource::loadImpl()
    {
        m_payloadData.meta.clear();
        m_payloadData.data.clear();
        vdb::VdbImporter reader;
        reader.loadStl(getFilename());
        reader.writeMesh(m_payloadData);
    }

    ResourceManager::ResourceManager(ResourceContext * resourceContext,
                                     std::filesystem::path assemblyDir)
        : m_resourceContext(resourceContext)
        , m_assemblyDir(std::move(assemblyDir))
    {
    }

    void ResourceManager::addResource(std::filesystem::path const & filename)
    {
        if (m_resources.find(ResourceKey{filename}) != m_resources.end())
        {
            return;
        }

        if (!is_regular_file(filename))
        {
            throw std::runtime_error(
              fmt::format("Loading {} failed, the file does not exist", filename.string()));
        }

        if (filename.extension() == ".stl" || filename.extension() == ".STL")
        {
            m_resources[ResourceKey{filename}] =
              std::make_unique<StlResource>(ResourceKey{filename});
        }
    }

    void ResourceManager::addResource(ResourceKey key, vdb::TriangleMesh && mesh)
    {
        m_resources[key] = std::make_unique<MeshResource>(key, std::move(mesh));
    }

    void ResourceManager::addResource(ResourceKey key, openvdb::GridBase::Ptr && grid)
    {
        m_resources[key] = std::make_unique<VdbResource>(key, std::move(grid));
    }

    void ResourceManager::addResource(ResourceKey key, io::ImageStack && stack)
    {
        m_resources[key] = std::make_unique<ImageStackResource>(key, std::move(stack));
    }

    void ResourceManager::loadResources()
    {
        for (auto & [filename, res] : m_resources)
        {
            if (res->isInUse())
            {
                m_bufferChanged |= res->load();
            }
        }
    }

    void ResourceManager::writeResources(Primitives & primitives)
    {
        for (auto & [filename, res] : m_resources)
        {
            res->write(primitives);
        }
        if (primitives.data.getSize() > 0)
        {
            primitives.write();
        }
    }

    void ResourceManager::clear()
    {
        m_textures.clear();
        m_resourceContext->clearImageStacks();
        m_nameCounter = 0;
    }

    void ResourceManager::increaseImageNumber()
    {
        ++m_nameCounter;
    }

    IResource & ResourceManager::getResource(ResourceKey const & key) const
    {
        return *m_resources.at(key);
    }

    IResource * ResourceManager::getResourcePtr(ResourceKey const & key)
    {
        auto iter = m_resources.find(key);
        if (iter == m_resources.end())
        {
            return nullptr;
        }
        return iter->second.get();
    }

    ResourceMap const & ResourceManager::getResourceMap() const
    {
        return m_resources;
    }

    bool ResourceManager::hasResource(ResourceKey const & key) const
    {
        return m_resources.find(key) != m_resources.end();
    }
}
