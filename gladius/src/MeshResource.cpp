#include "MeshResource.h"
#include "io/VdbImporter.h"

namespace gladius
{
    void MeshResource::loadImpl()
    {
        m_payloadData.meta.clear();
        m_payloadData.data.clear();
        vdb::VdbImporter const reader;
        reader.writeMesh(m_mesh, m_payloadData);
    }
}
