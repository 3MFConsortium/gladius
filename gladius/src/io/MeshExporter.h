#pragma once
#include "../ImageRGBA.h"
#include "../Mesh.h"
#include "../compute/ComputeCore.h"
#include "../nodes/Assembly.h"
#include "LayerBasedMeshExporter.h"
#include "vdb.h"

#include <filesystem>

namespace gladius::vdb
{
    openvdb::FloatGrid::Ptr createGridFromSdf(gladius::PreComputedSdf & sdf,
                                              float bandwidth_mm = 0.1f);

    void writeTriangle(std::ostream & output,
                       openvdb::Vec3s & point1,
                       openvdb::Vec3s & point2,
                       openvdb::Vec3s & point3);

    void exportGridToSTL(openvdb::FloatGrid::Ptr grid, std::filesystem::path const & filename);

    void exportSdfAsSTL(gladius::PreComputedSdf & sdf, std::filesystem::path const & filename);

    void addTriangleToMesh(gladius::Mesh & mesh,
                           openvdb::Vec3s const & point1,
                           openvdb::Vec3s const & point2,
                           openvdb::Vec3s const & point3);

    gladius::Mesh generatePreviewMesh(ComputeCore & generator, nodes::Assembly & assembly);
    gladius::Mesh gridToMesh(openvdb::FloatGrid::Ptr grid, ComputeContext & computeContext);
    void exportMeshToSTL(Mesh & mesh, const std::filesystem::path & filename);
    double alignToLayer(double value, double increment);

    class MeshExporter : public gladius::io::LayerBasedMeshExporter
    {
      public:
        // Override to store compute core reference
        void beginExport(std::filesystem::path const & fileName, ComputeCore & generator) override;

        // Override finalize to implement base finalization
        void finalize() override;

        void finalizeExportVdb();
        void finalizeExportNanoVdb();
        void finalizeExportSTL(ComputeCore & core);

      private:
        std::ofstream m_file;
        ComputeCore * m_computeCore = nullptr;
    };

}