#include "CliExportDialog.h"
 
#include "imgui.h"

namespace gladius::ui
{
    void CliExportDialog::beginExport(std::filesystem::path stlFilename,
                                       gladius::ComputeCore & core)
    {
        m_visible = true;
        m_cliWriter.beginExport(stlFilename, core);
    }

    void CliExportDialog::render(gladius::ComputeCore & core)
    {
        if (!m_visible)
        {
            return;
        }
        ImGui::Begin("CLI-Export", &m_visible);

        ImGui::TextUnformatted("Exporting to cli file");
        ImGui::ProgressBar(m_cliWriter.getProgress());
        if (!m_cliWriter.advanceExport(core))
        {
            m_cliWriter.finalizeExport();

#ifdef WIN32
            ShellExecuteW(
              nullptr, L"open", m_cliWriter.getFilename().c_str(), nullptr, nullptr, SW_SHOW);
#endif
            m_visible = false;
        }
        if (ImGui::Button("Cancel"))
        {
            m_visible = false;
        }
        ImGui::End();
    }

    bool CliExportDialog::isVisible() const
    {
        return m_visible;
    }
}
