#pragma once
#include "io/ImageStackExporter.h"
#include "compute/ComputeCore.h"

#include <filesystem>

namespace gladius::ui
{
    class ImageStackExportDialog
    {
      public:
        void beginExport(std::filesystem::path stlFilename, gladius::ComputeCore & core);
        void render(gladius::ComputeCore & core);
        [[nodiscard]] bool isVisible() const;

      private:
        gladius::io::ImageStackExporter m_imageStackExporter;
        bool m_visible{false};
    };
} // namespace gladius::ui
