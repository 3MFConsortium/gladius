#include "MeshExportDialog.h"

namespace gladius::ui
{
    void MeshExportDialog::beginExport(std::filesystem::path const & stlFilename,
                                       ComputeCore & core)
    {
        m_visible = true;
        m_computeCore = &core;
        m_exporter.setQualityLevel(1);
        m_exporter.beginExport(stlFilename, core);
    }

    std::string MeshExportDialog::getWindowTitle() const
    {
        return "Export in progress";
    }

    std::string MeshExportDialog::getExportMessage() const
    {
        return "Exporting to stl file";
    }

    io::IExporter & MeshExportDialog::getExporter()
    {
        return m_exporter;
    }

    void MeshExportDialog::finalizeExport()
    {
        if (m_computeCore)
        {
            m_exporter.finalizeExportSTL(*m_computeCore);
        }
        else
        {
            BaseExportDialog::finalizeExport();
        }
    }
}
