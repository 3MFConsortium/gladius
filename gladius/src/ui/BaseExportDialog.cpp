#include "BaseExportDialog.h"

#include "imgui.h"

namespace gladius::ui
{
    void BaseExportDialog::render(ComputeCore & core)
    {
        if (!m_visible)
        {
            return;
        }

        ImGui::Begin(getWindowTitle().c_str(), &m_visible);

        ImGui::TextUnformatted(getExportMessage().c_str());
        ImGui::ProgressBar(static_cast<float>(getExporter().getProgress()));

        if (isExportFinished(core))
        {
            finalizeExport();
            onExportCompleted();
            m_visible = false;
        }

        if (ImGui::Button("Cancel"))
        {
            onExportCancelled();
            m_visible = false;
        }

        ImGui::End();
    }

    bool BaseExportDialog::isVisible() const
    {
        return m_visible;
    }

    void BaseExportDialog::hide()
    {
        m_visible = false;
    }

    void BaseExportDialog::onExportCompleted()
    {
        // Default implementation - derived classes can override
    }

    void BaseExportDialog::onExportCancelled()
    {
        // Default implementation - derived classes can override
    }

    bool BaseExportDialog::isExportFinished(ComputeCore & core)
    {
        return !getExporter().advanceExport(core);
    }

    void BaseExportDialog::finalizeExport()
    {
        getExporter().finalize();
    }

} // namespace gladius::ui
