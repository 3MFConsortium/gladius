#pragma once

#include "../compute/ComputeCore.h"
#include "../io/IExporter.h"

#include <filesystem>
#include <memory>
#include <string>

namespace gladius::ui
{
    /// @brief Base class for all export dialogs providing common functionality
    ///
    /// This class implements the common pattern for export dialogs:
    /// - Visibility management
    /// - Basic rendering structure with progress bar and cancel button
    /// - Export lifecycle management
    class BaseExportDialog
    {
      public:
        /// @brief Begin export process with the given filename and core
        /// @param filename Path where the file should be exported
        /// @param core ComputeCore instance for processing
        virtual void beginExport(std::filesystem::path const & filename, ComputeCore & core) = 0;

        /// @brief Render the dialog UI
        /// @param core ComputeCore instance for processing
        virtual void render(ComputeCore & core);

        /// @brief Check if the dialog is currently visible
        /// @return true if dialog is visible, false otherwise
        [[nodiscard]] bool isVisible() const;

        /// @brief Hide the dialog
        virtual void hide();

        /// @brief Virtual destructor for proper inheritance
        virtual ~BaseExportDialog() = default;

      protected:
        /// @brief Get the window title for the dialog
        /// @return Window title string
        [[nodiscard]] virtual std::string getWindowTitle() const = 0;

        /// @brief Get the export message to display
        /// @return Export message string
        [[nodiscard]] virtual std::string getExportMessage() const = 0;

        /// @brief Get the underlying exporter
        /// @return Reference to the exporter
        [[nodiscard]] virtual io::IExporter & getExporter() = 0;

        /// @brief Called when export is completed successfully
        virtual void onExportCompleted();

        /// @brief Called when export is cancelled
        virtual void onExportCancelled();

        /// @brief Check if export process has finished
        /// @param core ComputeCore instance
        /// @return true if export finished, false if still in progress
        [[nodiscard]] virtual bool isExportFinished(ComputeCore & core);

        /// @brief Finalize the export process
        virtual void finalizeExport();

        bool m_visible{false};
    };

} // namespace gladius::ui
