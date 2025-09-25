#include "VdbResource.h"

#include "io/VdbImporter.h"

namespace gladius
{
    VdbResource::VdbResource(ResourceKey key, openvdb::GridBase::Ptr && grid)
        : ResourceBase(std::move(key))
        , m_grid(std::move(grid))
    {
        ResourceBase::load();
    }

    nodes::float3 VdbResource::getGridSize() const
    {

        if (m_grid)
        {
            auto bbox = m_grid->evalActiveVoxelBoundingBox();

            return nodes::float3{static_cast<float>(bbox.max().x() - bbox.min().x()),
                                 static_cast<float>(bbox.max().y() - bbox.min().y()),
                                 static_cast<float>(bbox.max().z() - bbox.min().z())};
        }

        return nodes::float3();
    }

    void VdbResource::loadImpl()
    {
        if (!m_grid)
        {
            return;
        }

        m_payloadData.meta.clear();

        PrimitiveMeta metaData{};
        metaData.primitiveType = SDF_VDB;
        metaData.start = static_cast<int>(m_payloadData.data.size());

        vdb::importFromGrid<float>(m_grid, m_payloadData, 1.0f);

        metaData.end = static_cast<int>(m_payloadData.data.size());
        m_payloadData.meta.push_back(metaData);
    }
}