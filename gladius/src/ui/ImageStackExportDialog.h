#pragma once
#include "BaseExportDialog.h"
#include "io/ImageStackExporter.h"

#include <filesystem>

namespace gladius::ui
{
    /// @brief Export dialog for image stack exports with progress tracking
    class ImageStackExportDialog : public BaseExportDialog
    {
      public:
        ImageStackExportDialog()
            : m_exporter(nullptr)
        {
        }

        void beginExport(std::filesystem::path const & filename, ComputeCore & core) override;

      protected:
        std::string getWindowTitle() const override
        {
            return "ImageStack-Export";
        }
        std::string getExportMessage() const override
        {
            return "Exporting to image stack";
        }
        io::IExporter & getExporter() override
        {
            return m_exporter;
        }

      private:
        io::ImageStackExporter m_exporter;
    };
} // namespace gladius::ui
