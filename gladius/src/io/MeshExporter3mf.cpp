#include "MeshExporter3mf.h"

#include "../Document.h"
#include "../compute/ComputeCore.h"
#include "3mf/MeshWriter3mf.h"
#include "MeshExporter.h"
#include "vdb.h"

#include <fmt/format.h>

namespace gladius::vdb
{
    MeshExporter3mf::MeshExporter3mf(events::SharedLogger logger)
        : m_logger(std::move(logger))
    {
    }

    void MeshExporter3mf::beginExport(std::filesystem::path const & fileName,
                                      ComputeCore & generator)
    {
        m_computeCore = &generator;
        m_sourceDocument = nullptr;
        LayerBasedMeshExporter::beginExport(fileName, generator);
    }

    void MeshExporter3mf::beginExport(std::filesystem::path const & fileName,
                                      ComputeCore & generator,
                                      Document const * document)
    {
        m_computeCore = &generator;
        m_sourceDocument = document;
        LayerBasedMeshExporter::beginExport(fileName, generator);
    }

    void MeshExporter3mf::finalize()
    {
        if (!m_computeCore || !m_grid)
        {
            return;
        }

        try
        {
            // Convert grid to mesh using the existing function
            auto mesh = gridToMesh(m_grid, *m_computeCore->getComputeContext());

            // Export the mesh using MeshWriter3mf
            gladius::io::MeshWriter3mf writer(m_logger);
            std::string meshName = "Mesh";
            writer.exportMesh(m_fileName, mesh, meshName, m_sourceDocument, true);

            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Successfully exported 3MF mesh to {}", m_fileName.string()),
                   events::Severity::Info});
            }
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Failed to export 3MF mesh: {}", e.what()),
                                    events::Severity::Error});
            }
            throw;
        }

        m_grid.reset();
    }

} // namespace gladius::vdb
