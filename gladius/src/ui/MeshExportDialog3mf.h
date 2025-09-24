#pragma once
#include "../io/MeshExporter3mf.h"
#include "BaseExportDialog.h"

#include <filesystem>

// Forward declaration
namespace gladius
{
    class Document;
}

namespace gladius::ui
{
    class MeshExportDialog3mf : public BaseExportDialog
    {
      public:
        MeshExportDialog3mf()
            : m_exporter(nullptr)
        {
        }

        void beginExport(std::filesystem::path const & threeMfFilename,
                         ComputeCore & core) override;

        void beginExport(std::filesystem::path const & threeMfFilename,
                         ComputeCore & core,
                         Document const * document);

      protected:
        [[nodiscard]] std::string getWindowTitle() const override;
        [[nodiscard]] std::string getExportMessage() const override;
        [[nodiscard]] io::IExporter & getExporter() override;

      private:
        vdb::MeshExporter3mf m_exporter;
    };
} // namespace gladius::ui
