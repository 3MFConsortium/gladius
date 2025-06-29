#pragma once
#include "../io/MeshExporter3mf.h"

#include <filesystem>

namespace gladius::ui
{
    class MeshExportDialog3mf
    {
      public:
        void beginExport(std::filesystem::path threeMfFilename, ComputeCore & core);
        void render(ComputeCore & core);
        [[nodiscard]] bool isVisible() const;

      private:
        vdb::MeshExporter3mf m_exporter;
        bool m_visible{false};
    };
} // namespace gladius::ui
