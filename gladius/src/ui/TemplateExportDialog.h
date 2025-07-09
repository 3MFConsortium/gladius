#pragma once

#include "../compute/ComputeCore.h"
#include "BaseExportDialog.h"

#include <filesystem>
#include <memory>
#include <string>

namespace gladius::ui
{
    /// @brief Template-based export dialog for exporters that implement a specific interface
    ///
    /// This template provides a simple way to create export dialogs for any exporter type
    /// that has beginExport, advanceExport, finalize, and getProgress methods.
    ///
    /// @tparam ExporterType The type of exporter this dialog will manage
    template <typename ExporterType>
    class TemplateExportDialog : public BaseExportDialog
    {
      public:
        /// @brief Constructor with custom window title and export message
        /// @param windowTitle Title for the dialog window
        /// @param exportMessage Message to display during export
        TemplateExportDialog(std::string windowTitle, std::string exportMessage)
            : m_windowTitle(std::move(windowTitle))
            , m_exportMessage(std::move(exportMessage))
        {
        }

        void beginExport(std::filesystem::path const & filename, ComputeCore & core) override
        {
            m_visible = true;
            m_exporter.beginExport(filename, core);
        }

      protected:
        [[nodiscard]] std::string getWindowTitle() const override
        {
            return m_windowTitle;
        }

        [[nodiscard]] std::string getExportMessage() const override
        {
            return m_exportMessage;
        }

        [[nodiscard]] io::IExporter & getExporter() override
        {
            return m_exporter;
        }

        /// @brief Get access to the underlying exporter for customization
        /// @return Reference to the exporter
        ExporterType & getTypedExporter()
        {
            return m_exporter;
        }

      private:
        ExporterType m_exporter;
        std::string m_windowTitle;
        std::string m_exportMessage;
    };

} // namespace gladius::ui
