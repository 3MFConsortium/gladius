#pragma once

#include "ResourceManager.h"
#include "io/3mf/ImageStack.h"
#include "nodes/types.h"

#include <openvdb/Grid.h>

namespace gladius
{
    class VdbResource  : public ResourceBase
    {
    public:
      explicit VdbResource(ResourceKey key, openvdb::GridBase::Ptr && grid);

      ~VdbResource() = default;

      nodes::float3 getGridSize() const;

    private:
      openvdb::GridBase::Ptr m_grid;

      void loadImpl() override;
    };
}
