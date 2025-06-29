#pragma once
#include "../EventLogger.h"
#include "../compute/ComputeCore.h"
#include "../io/3mf/MeshWriter3mf.h"
#include "../nodes/Assembly.h"
#include "IExporter.h"
#include "vdb.h"

#include <filesystem>

namespace gladius::vdb
{
    class MeshExporter3mf : public gladius::io::IExporter
    {
      public:
        explicit MeshExporter3mf(events::SharedLogger logger = nullptr);

        void beginExport(std::filesystem::path const & fileName, ComputeCore & generator) override;
        bool advanceExport(ComputeCore & generator) override;
        void finalize() override;
        [[nodiscard]] double getProgress() const override;

        void setQualityLevel(size_t qualityLevel);

      private:
        void setLayerIncrement(float increment_mm);

        std::filesystem::path m_fileName;
        double m_layerIncrement_mm = 0.1;
        float m_bandwidth_mm = m_layerIncrement_mm * 2.f;
        size_t m_qualityLevel = 3; // 3 = best quality, but insane high memory usage
        double m_progress = 0.;
        double m_startHeight_mm = 0.;
        double m_endHeight_mm = 0.;
        double m_currentHeight_mm = 0.;

        openvdb::FloatGrid::Ptr m_grid;
        events::SharedLogger m_logger;
        ComputeCore * m_computeCore = nullptr;
    };
}
