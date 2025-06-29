#include "MeshExporter3mf.h"

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
        m_fileName = fileName;
        m_computeCore = &generator;

        if (!generator.updateBBox())
        {
            throw std::runtime_error(
              "Computing bounding box failed. The model has probably not been compiled yet");
        }

        auto const bb = generator.getBoundingBox();
        generator.updateClippingAreaWithPadding();

        m_startHeight_mm = alignToLayer(bb->min.z - m_layerIncrement_mm, m_layerIncrement_mm);
        m_endHeight_mm = alignToLayer(bb->max.z + m_layerIncrement_mm, m_layerIncrement_mm);
        m_currentHeight_mm = m_startHeight_mm;

        m_currentHeight_mm = alignToLayer(m_currentHeight_mm, m_layerIncrement_mm);

        generator.getResourceContext()->requestDistanceMaps();

        auto const resX =
          generator.getResourceContext()->getDistanceMipMaps()[m_qualityLevel]->getWidth();
        auto const width_mm = generator.getResourceContext()->getClippingArea().z -
                              generator.getResourceContext()->getClippingArea().x;
        auto const voxelSize = width_mm / static_cast<float>(resX);
        setLayerIncrement(voxelSize);
        generator.setSliceHeight(m_currentHeight_mm);

        m_grid = openvdb::FloatGrid::create(m_bandwidth_mm);
        m_grid->setGridClass(openvdb::GRID_LEVEL_SET);
        m_grid->setName("SDF computed by gladius");
        m_grid->setTransform(
          openvdb::math::Transform::createLinearTransform(static_cast<double>(voxelSize)));
    }

    bool MeshExporter3mf::advanceExport(ComputeCore & generator)
    {
        generator.generateSdfSlice();

        auto & distmap = *(generator.getResourceContext()->getDistanceMipMaps()[m_qualityLevel]);
        distmap.read();

        openvdb::FloatGrid::Accessor accessor = m_grid->getAccessor();

        auto const z = static_cast<int>(std::floor(m_currentHeight_mm / m_layerIncrement_mm));

        m_currentHeight_mm = alignToLayer(m_currentHeight_mm, m_layerIncrement_mm);

        for (int y = 0; y < static_cast<int>(distmap.getHeight()); ++y)
        {
            for (int x = 0; x < static_cast<int>(distmap.getWidth()); ++x)
            {
                openvdb::Coord xyz(x, y, z);
                auto const value =
                  std::clamp<float>(distmap.getValue(x, y).x, -m_bandwidth_mm, m_bandwidth_mm);

                accessor.setValue(xyz, value);
            }
        }

        m_grid->pruneGrid();
        m_currentHeight_mm += m_layerIncrement_mm;
        m_currentHeight_mm = alignToLayer(m_currentHeight_mm, m_layerIncrement_mm);

        generator.setSliceHeight(m_currentHeight_mm);

        m_progress =
          ((generator.getSliceHeight() - m_startHeight_mm) / (m_endHeight_mm - m_startHeight_mm));
        return generator.getSliceHeight() < generator.getBoundingBox()->max.z + m_layerIncrement_mm;
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
            std::string meshName = "Gladius_Mesh";
            writer.exportMesh(m_fileName, mesh, meshName, nullptr);

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

    double MeshExporter3mf::getProgress() const
    {
        return m_progress;
    }

    void MeshExporter3mf::setQualityLevel(size_t qualityLevel)
    {
        m_qualityLevel = qualityLevel;
    }

    void MeshExporter3mf::setLayerIncrement(float increment_mm)
    {
        m_layerIncrement_mm = increment_mm;
        m_bandwidth_mm = m_layerIncrement_mm * 2.f;
    }

} // namespace gladius::vdb
