#pragma once

#include "../compute/ComputeCore.h"
#include "IExporter.h"
#include "vdb.h"

#include <filesystem>

namespace gladius::io
{
    /// @brief Base class for mesh exporters that process data layer by layer
    ///
    /// This class provides common functionality for exporters that:
    /// - Process 3D data slice by slice
    /// - Build OpenVDB grids from distance maps
    /// - Track progress through height layers
    /// - Manage quality levels and layer increments
    class LayerBasedMeshExporter : public IExporter
    {
      public:
        /// @brief Constructor
        LayerBasedMeshExporter() = default;

        /// @brief Virtual destructor
        virtual ~LayerBasedMeshExporter() = default;

        // IExporter interface
        void beginExport(std::filesystem::path const & fileName, ComputeCore & generator) override;
        bool advanceExport(ComputeCore & generator) override;
        void finalize() override = 0; // Pure virtual - each exporter finalizes differently
        [[nodiscard]] double getProgress() const override;

        /// @brief Set the quality level for export
        /// @param qualityLevel Quality level (0-3, where 3 is highest quality)
        void setQualityLevel(size_t qualityLevel);

      protected:
        /// @brief Initialize the OpenVDB grid with proper settings
        /// @param generator ComputeCore instance
        virtual void initializeGrid(ComputeCore & generator);

        /// @brief Process a single layer of the export
        /// @param generator ComputeCore instance
        /// @return true if more layers to process, false if finished
        virtual bool processLayer(ComputeCore & generator);

        /// @brief Set the layer increment and related bandwidth
        /// @param increment_mm Layer increment in millimeters
        void setLayerIncrement(float increment_mm);

        /// @brief Calculate aligned layer height
        /// @param value Current height value
        /// @param increment Layer increment
        /// @return Aligned height value
        static double alignToLayer(double value, double increment);

        // Common member variables for layer-based processing
        std::filesystem::path m_fileName;
        openvdb::FloatGrid::Ptr m_grid;
        double m_layerIncrement_mm = 0.1;
        float m_bandwidth_mm = m_layerIncrement_mm * 2.f;
        size_t m_qualityLevel = 3; // 3 = best quality, but high memory usage
        double m_progress = 0.;
        double m_startHeight_mm = 0.;
        double m_endHeight_mm = 0.;
        double m_currentHeight_mm = 0.;
    };

} // namespace gladius::io
