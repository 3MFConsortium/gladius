#pragma once

#include "ResourceManager.h"
#include "io/VdbImporter.h"

namespace gladius
{
    class MeshResource : public ResourceBase
    {
      public:
        MeshResource(ResourceKey key, vdb::TriangleMesh && mesh)
            : ResourceBase(std::move(key))
            , m_mesh(std::move(mesh)) {

            };

        vdb::TriangleMesh const & getMesh() const
        {
            return m_mesh;
        }

      private:
        vdb::TriangleMesh m_mesh;

        void loadImpl() override;
    };
}