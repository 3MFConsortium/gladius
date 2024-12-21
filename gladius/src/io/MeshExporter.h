#pragma once
#include "../ImageRGBA.h"
#include "../Mesh.h"
#include "../compute/ComputeCore.h"
#include "../nodes/Assembly.h"
#include "vdb.h"

#include "IExporter.h"

#include <filesystem>

namespace gladius::vdb
{
    openvdb::FloatGrid::Ptr createGridFromSdf(gladius::PreComputedSdf & sdf, float bandwidth_mm = 0.1f);

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

    class MeshExporter : public gladius::io::IExporter
    {
      public:
        void beginExport(const std::filesystem::path & fileName, ComputeCore & generator) override;
        bool advanceExport(ComputeCore & generator) override;
        void finalize() override;
        [[nodiscard]] double getProgress() const override;

        void finalizeExportVdb();
        void finalizeExportNanoVdb();
        void finalizeExportSTL(ComputeCore & core) ;

        void setQualityLevel(size_t qualityLevel);

      private:
        void setLayerIncrement(float increment_mm);
        std::filesystem::path m_fileName;
        std::ofstream m_file;
        openvdb::FloatGrid::Ptr m_grid;
        double m_layerIncrement_mm = 0.1f;
        float m_bandwidth_mm = m_layerIncrement_mm * 2.f;
        size_t m_qualityLevel = 2; // 3 = best quality, but insane high memory usage
        double m_progress = 0.;
        double m_startHeight_mm = 0.;
        double m_endHeight_mm = 0.;
        double m_currentHeight_mm = 0.;
    };

}