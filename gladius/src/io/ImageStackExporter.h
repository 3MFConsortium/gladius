#pragma once
#include "IExporter.h"

#include "EventLogger.h"

#include <filesystem>
#include <lib3mf_implicit.hpp>

#include <fstream>
#include <iostream>

namespace gladius::io
{

    class ImageStackExporter : public IExporter
    {
      public:
        explicit ImageStackExporter(events::SharedLogger logger);

        void beginExport(const std::filesystem::path & fileName, ComputeCore & generator) override;
        bool advanceExport(ComputeCore & generator) override;

        [[nodiscard]] double getProgress() const override;

        void finalize() override;

        void setQualityLevel(size_t qualityLevel);

      private:
        void setLayerIncrement(float increment_mm);

        Lib3MF_uint32 getColumnCountPng() const;
        Lib3MF_uint32 getRowCountPng() const;

        std::filesystem::path m_outputFilename{};

        float m_layerIncrement_mm = 0.1f;
        float m_bandwidth_mm = m_layerIncrement_mm * 2.f;
        size_t m_qualityLevel = 1; // 3 = best quality, but insane high memory usage
        double m_progress = 0.;
        float m_startHeight_mm = 0.f;
        float m_endHeight_mm = 0.f;

        unsigned int m_currentSlice = 0;

        Lib3MF::PWrapper m_wrapper{};
        Lib3MF::PModel m_model3mf;
        Lib3MF::PImageStack m_imageStack;
        Lib3MF_uint32 m_sheetcount = 0u;
        Lib3MF_uint32 m_columnCountWorld = 0u;
        Lib3MF_uint32 m_rowCountWorld = 0u;

        events::SharedLogger m_logger;
    };
}