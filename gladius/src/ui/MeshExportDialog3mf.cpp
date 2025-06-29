#include "MeshExportDialog3mf.h"

#include "imgui.h"

namespace gladius::ui
{
    void MeshExportDialog3mf::beginExport(std::filesystem::path threeMfFilename, ComputeCore & core)
    {
        m_visible = true;
        m_exporter.setQualityLevel(1);
        m_exporter.beginExport(threeMfFilename, core);
    }

    void MeshExportDialog3mf::render(ComputeCore & core)
    {
        if (!m_visible)
        {
            return;
        }
        ImGui::Begin("Export in progress", &m_visible);

        ImGui::TextUnformatted("Exporting to 3MF file");
        ImGui::ProgressBar(static_cast<float>(m_exporter.getProgress()));
        if (!m_exporter.advanceExport(core))
        {
            m_exporter.finalize();
            m_visible = false;
        }
        if (ImGui::Button("Cancel"))
        {
            m_visible = false;
        }
        ImGui::End();
    }

    bool MeshExportDialog3mf::isVisible() const
    {
        return m_visible;
    }
}
