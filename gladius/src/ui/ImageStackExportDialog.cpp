#include "ImageStackExportDialog.h"

#include "imgui.h"

namespace gladius::ui
{
    void ImageStackExportDialog::beginExport(std::filesystem::path stlFilename,
                                             gladius::ComputeCore & core)
    {
        m_visible = true;
        m_imageStackExporter.beginExport(stlFilename, core);
    }

    void ImageStackExportDialog::render(gladius::ComputeCore & core)
    {
        if (!m_visible)
        {
            return;
        }
        ImGui::Begin("ImageStack-Export", &m_visible);

        ImGui::TextUnformatted("Exporting to image stack");
        ImGui::ProgressBar(static_cast<float>(m_imageStackExporter.getProgress()));
        if (!m_imageStackExporter.advanceExport(core))
        {
            m_imageStackExporter.finalize();
            m_visible = false;
        }
        if (ImGui::Button("Cancel"))
        {
            m_visible = false;
        }
        ImGui::End();
    }
}