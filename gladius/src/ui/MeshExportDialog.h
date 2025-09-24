#pragma once
#include "BaseExportDialog.h"
#include "io/MeshExporter.h"

#include <filesystem>

namespace gladius::ui
{
    class MeshExportDialog : public BaseExportDialog
    {
      public:
        void beginExport(std::filesystem::path const & stlFilename, ComputeCore & core) override;

      protected:
        [[nodiscard]] std::string getWindowTitle() const override;
        [[nodiscard]] std::string getExportMessage() const override;
        [[nodiscard]] io::IExporter & getExporter() override;

        void finalizeExport() override;

      private:
        vdb::MeshExporter m_exporter;
        ComputeCore * m_computeCore = nullptr;
    };
} // namespace gladius::ui
