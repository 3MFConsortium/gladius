#pragma once

#include "ContourExtractor.h"
#include "compute/ComputeCore.h"

#include <filesystem>
#include <fstream>

namespace gladius
{
    class CliWriter
    {
      public:
        void saveCurrentLayer(const std::filesystem::path & fileName, ComputeCore & generator);
        void save(const std::filesystem::path & fileName, ComputeCore & generator);
        [[nodiscard]] std::filesystem::path getFilename() const;

        void beginExport(const std::filesystem::path & fileName, ComputeCore & generator);
        bool advanceExport(ComputeCore & generator);
        void finalizeExport();
        [[nodiscard]] float getProgress() const;

      private:
        void writeHeader();
        void writeLayer(const PolyLines & polyLines, double z_mm);
        void writePolyLine(const PolyLine & polyLine);
        [[nodiscard]] float roundToLayerThickness(float value) const;
        std::ofstream m_file;
        std::filesystem::path m_fileName;
        float m_layerThickness_mm = 0.01f;
        int modelId{0};
        float m_progress = 0.;
        float m_startHeight_mm = 0.f;
        float m_endHeight_mm = 0.f;
    };
}
