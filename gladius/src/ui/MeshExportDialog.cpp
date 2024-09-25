#include "MeshExportDialog.h"

 
#include "imgui.h"

namespace gladius::ui
{
    void MeshExportDialog::beginExport(std::filesystem::path stlFilename, ComputeCore & core)
    {
        m_visible = true;
        m_exporter.setQualityLevel(2);
        m_exporter.beginExport(stlFilename, core);
    }

    void MeshExportDialog::render(ComputeCore & core)
    {
        if (!m_visible)
        {
            return;
        }
        ImGui::Begin("Export in progress", &m_visible);

        ImGui::TextUnformatted("Exporting to stl file");
        ImGui::ProgressBar(static_cast<float>(m_exporter.getProgress()));
        if (!m_exporter.advanceExport(core))
        {
            m_exporter.finalizeExportSTL(core);
            m_visible = false;
        }
        if (ImGui::Button("Cancel"))
        {
            m_visible = false;
        }
        ImGui::End();
    }

    bool MeshExportDialog::isVisible() const
    {
        return m_visible;
    }
}
