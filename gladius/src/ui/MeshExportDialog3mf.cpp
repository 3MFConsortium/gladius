#include "MeshExportDialog3mf.h"

namespace gladius::ui
{
    void MeshExportDialog3mf::beginExport(std::filesystem::path const & threeMfFilename,
                                          ComputeCore & core)
    {
        m_visible = true;
        m_exporter.setQualityLevel(1);
        m_exporter.beginExport(threeMfFilename, core);
    }

    std::string MeshExportDialog3mf::getWindowTitle() const
    {
        return "Export in progress";
    }

    std::string MeshExportDialog3mf::getExportMessage() const
    {
        return "Exporting to 3MF file";
    }

    io::IExporter & MeshExportDialog3mf::getExporter()
    {
        return m_exporter;
    }
}
