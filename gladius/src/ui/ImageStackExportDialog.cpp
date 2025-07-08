#include "ImageStackExportDialog.h"

namespace gladius::ui
{
    void ImageStackExportDialog::beginExport(std::filesystem::path const & filename,
                                             ComputeCore & core)
    {
        m_visible = true;
        m_exporter.beginExport(filename, core);
    }
}