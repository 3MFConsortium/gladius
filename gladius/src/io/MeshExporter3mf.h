#pragma once
#include "../EventLogger.h"
#include "../compute/ComputeCore.h"
#include "../io/3mf/MeshWriter3mf.h"
#include "../nodes/Assembly.h"
#include "LayerBasedMeshExporter.h"
#include "vdb.h"

#include <filesystem>

// Forward declaration
namespace gladius
{
    class Document;
}

namespace gladius::vdb
{
    class MeshExporter3mf : public gladius::io::LayerBasedMeshExporter
    {
      public:
        explicit MeshExporter3mf(events::SharedLogger logger = nullptr);

        // Override to store compute core reference
        void beginExport(std::filesystem::path const & fileName, ComputeCore & generator) override;

        // Overload to accept document for thumbnail generation
        void beginExport(std::filesystem::path const & fileName,
                         ComputeCore & generator,
                         Document const * document);

        // Override finalize to implement 3MF-specific finalization
        void finalize() override;

      private:
        events::SharedLogger m_logger;
        ComputeCore * m_computeCore = nullptr;
        Document const * m_sourceDocument = nullptr;
    };
}
