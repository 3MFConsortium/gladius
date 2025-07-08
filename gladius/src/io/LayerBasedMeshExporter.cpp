#include "LayerBasedMeshExporter.h"

#include <cmath>
#include <stdexcept>

namespace gladius::io
{
    void LayerBasedMeshExporter::beginExport(std::filesystem::path const & fileName,
                                             ComputeCore & generator)
    {
        m_fileName = fileName;

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

        initializeGrid(generator);
    }

    bool LayerBasedMeshExporter::advanceExport(ComputeCore & generator)
    {
        bool hasMoreLayers = processLayer(generator);

        m_progress =
          ((generator.getSliceHeight() - m_startHeight_mm) / (m_endHeight_mm - m_startHeight_mm));

        return hasMoreLayers;
    }

    double LayerBasedMeshExporter::getProgress() const
    {
        return m_progress;
    }

    void LayerBasedMeshExporter::setQualityLevel(size_t qualityLevel)
    {
        m_qualityLevel = qualityLevel;
    }

    void LayerBasedMeshExporter::initializeGrid(ComputeCore & generator)
    {
        auto const resX =
          generator.getResourceContext()->getDistanceMipMaps()[m_qualityLevel]->getWidth();
        auto const width_mm = generator.getResourceContext()->getClippingArea().z -
                              generator.getResourceContext()->getClippingArea().x;
        auto const voxelSize = width_mm / static_cast<float>(resX);

        m_grid = openvdb::FloatGrid::create(m_bandwidth_mm);
        m_grid->setGridClass(openvdb::GRID_LEVEL_SET);
        m_grid->setName("SDF computed by gladius");
        m_grid->setTransform(
          openvdb::math::Transform::createLinearTransform(static_cast<double>(voxelSize)));
    }

    bool LayerBasedMeshExporter::processLayer(ComputeCore & generator)
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

        return generator.getSliceHeight() < generator.getBoundingBox()->max.z + m_layerIncrement_mm;
    }

    void LayerBasedMeshExporter::setLayerIncrement(float increment_mm)
    {
        m_layerIncrement_mm = increment_mm;
        m_bandwidth_mm = m_layerIncrement_mm * 2.f;
    }

    double LayerBasedMeshExporter::alignToLayer(double value, double increment)
    {
        return std::floor(value / increment) * increment;
    }

} // namespace gladius::io
