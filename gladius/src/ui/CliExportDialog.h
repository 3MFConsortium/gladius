#pragma once
#include "../CliWriter.h"

#include <filesystem>

namespace gladius::ui
{
    class CliExportDialog
    {
      public:
        void beginExport(std::filesystem::path stlFilename, gladius::ComputeCore & core);
        void render(gladius::ComputeCore & core);
        [[nodiscard]] bool isVisible() const;

      private:
        gladius::CliWriter m_cliWriter;
        bool m_visible{false};
    };
} // namespace gladius::ui
