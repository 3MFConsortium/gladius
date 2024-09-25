#pragma once
#include <MeshExporter.h>

#include <filesystem>

namespace gladius::ui
{
    class MeshExportDialog
    {
      public:
        void beginExport(std::filesystem::path stlFilename, ComputeCore & core);
        void render(ComputeCore & core);
        [[nodiscard]] bool isVisible() const;

      private:
        vdb::MeshExporter m_exporter;
        bool m_visible{false};
    };
} // namespace gladius::ui
