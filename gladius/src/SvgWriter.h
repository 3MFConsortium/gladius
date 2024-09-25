#pragma once

#include "compute/ComputeCore.h"
#include "ContourExtractor.h"

#include <filesystem>
#include <fstream>

namespace gladius
{
    class SvgWriter
    {
      public:
        void saveCurrentLayer(const std::filesystem::path & fileName, ComputeCore & generator);

      private:
        void writeHeader();
        void writeLayer(const PolyLines & polyLines);
        void writePolyLine(const PolyLine & polyLine);
        [[nodiscard]] float roundToLayerThickness(float value) const;
        std::ofstream m_file;
        std::filesystem::path m_fileName;
        float m_layerThickness_mm = 0.05f;
        int modelId{0};
        float m_progress = 0.;
        float m_startHeight_mm = 0.f;
        float m_endHeight_mm = 0.f;
    };
}
