#include "../ImageRGBA.h"

#include "compute/ComputeCore.h"
#include "vdb.h"
#include <filesystem>
#include <fstream>

#include "MeshExporter.h"

namespace gladius::vdb
{
    openvdb::FloatGrid::Ptr createGridFromSdf(PreComputedSdf & sdf, float bandwidth_mm)
    {
        using namespace openvdb;
        sdf.read();
        FloatGrid::Ptr grid = FloatGrid::create();
        FloatGrid::Accessor accessor = grid->getAccessor();

        for (int z = 0; z < static_cast<int>(sdf.getHeight()); ++z)
        {
            for (int y = 0; y < static_cast<int>(sdf.getDepth()); ++y)
            {
                for (int x = 0; x < static_cast<int>(sdf.getWidth()); ++x)
                {
                    Coord xyz(x, y, z);
                    const auto value = sdf.getValue(x, y, z);
                    if (fabs(value) < bandwidth_mm)
                    {
                        accessor.setValue(xyz, value);
                    }
                    else if (value < -bandwidth_mm)
                    {
                        accessor.setValue(xyz, -bandwidth_mm); // fill up the inner part
                    }
                }
            }
            grid->pruneGrid();
        }
        grid->setGridClass(GRID_LEVEL_SET);
        grid->setName("SDF computed by gladius");
        return grid;
    }

    void writeTriangle(std::ostream & output,
                       openvdb::Vec3s & point1,
                       openvdb::Vec3s & point2,
                       openvdb::Vec3s & point3)
    {
        openvdb::Vec3s a = (point2 - point1);
        openvdb::Vec3s b = (point3 - point1);
        a.normalize();
        b.normalize();

        openvdb::Vec3s normal = (a.cross(b));
        normal.normalize();
        const uint16_t attribute = 0;
        assert(sizeof(normal[0]) == sizeof(float));

        for (int i = 0; i < 3; ++i)
        {
            output.write(reinterpret_cast<const char *>(&(normal[i])), sizeof(float));
        }

        for (int i = 0; i < 3; ++i)
        {
            output.write(reinterpret_cast<const char *>(&(point1[i])), sizeof(float));
        }

        for (int i = 0; i < 3; ++i)
        {
            output.write(reinterpret_cast<const char *>(&(point2[i])), sizeof(float));
        }

        for (int i = 0; i < 3; ++i)
        {
            output.write(reinterpret_cast<const char *>(&(point3[i])), sizeof(float));
        }

        output.write(reinterpret_cast<const char *>(&attribute), sizeof(attribute));
    }

    void exportGridToSTL(openvdb::FloatGrid::Ptr grid, const std::filesystem::path & filename)
    {
        if (grid->getGridClass() == openvdb::GRID_LEVEL_SET && grid->isType<openvdb::FloatGrid>())
        {
            using openvdb::Index64;
            openvdb::FloatGrid::Ptr floatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(grid);

            openvdb::tools::VolumeToMesh mesher(0., 1., true);
            mesher(*floatGrid);

            std::ofstream outputFile(filename, std::ios::binary | std::ios::out);

            // write header
            const char header[80] = "Made with gladius";

            size_t numberTriangles = 0;
            for (Index64 polyIndex = 0; polyIndex < mesher.polygonPoolListSize(); ++polyIndex)
            {
                numberTriangles += mesher.polygonPoolList()[polyIndex].numTriangles();
                numberTriangles += mesher.polygonPoolList()[polyIndex].numQuads() * 2;
            }

            outputFile.write(header, 80);
            outputFile.write(reinterpret_cast<char *>(&numberTriangles), 4);

            unsigned long trianglesWritten = 0;
            for (Index64 polyIndex = 0; polyIndex < mesher.polygonPoolListSize(); ++polyIndex)
            {
                const openvdb::tools::PolygonPool & polygons = mesher.polygonPoolList()[polyIndex];
                const auto numTriangles = polygons.numTriangles();
                for (Index64 triangleIndex = 0; triangleIndex < numTriangles; ++triangleIndex)
                {
                    const openvdb::Vec3I & triangle = polygons.triangle(triangleIndex);

                    openvdb::Vec3s & point1 = mesher.pointList()[triangle[0]];
                    openvdb::Vec3s & point2 = mesher.pointList()[triangle[1]];
                    openvdb::Vec3s & point3 = mesher.pointList()[triangle[2]];

                    writeTriangle(outputFile,
                                  point3,
                                  point2,
                                  point1); // NOLINT(readability-suspicious-call-argument)
                    ++trianglesWritten;
                }

                const auto numQuads = polygons.numQuads();
                for (Index64 quadIndex = 0; quadIndex < numQuads; ++quadIndex)
                {
                    const openvdb::Vec4I & quad = polygons.quad(quadIndex);
                    assert(quad[0] < mesher.pointListSize());
                    assert(quad[1] < mesher.pointListSize());
                    assert(quad[2] < mesher.pointListSize());
                    assert(quad[3] < mesher.pointListSize());

                    openvdb::Vec3s & point1 = mesher.pointList()[quad[0]];
                    openvdb::Vec3s & point2 = mesher.pointList()[quad[1]];
                    openvdb::Vec3s & point3 = mesher.pointList()[quad[2]];
                    openvdb::Vec3s & point4 = mesher.pointList()[quad[3]];

                    writeTriangle(outputFile,
                                  point3,
                                  point2,
                                  point1); // NOLINT(readability-suspicious-call-argument)
                    ++trianglesWritten;
                    writeTriangle(outputFile,
                                  point1,
                                  point4,
                                  point3); // NOLINT(readability-suspicious-call-argument)
                    ++trianglesWritten;
                }
            }
            std::cout << "triangles written: " << trianglesWritten << "\n";
            outputFile.close();
        }
    }

    void exportMeshToSTL(Mesh & mesh, const std::filesystem::path & filename)
    {
        std::ofstream outputFile(filename, std::ios::binary | std::ios::out);

        // write header
        const char header[80] = "Made with gladius";
        outputFile.write(header, 80);

        size_t numberTriangles = mesh.getNumberOfFaces();
        outputFile.write(reinterpret_cast<char *>(&numberTriangles), 4);

        for (size_t i = 0; i < mesh.getNumberOfFaces(); ++i)
        {
            auto face = mesh.getFace(i);
            auto a =
              openvdb::Vec3s{face.vertices[0].x(), face.vertices[0].y(), face.vertices[0].z()};

            auto b =
              openvdb::Vec3s{face.vertices[1].x(), face.vertices[1].y(), face.vertices[1].z()};

            auto c =
              openvdb::Vec3s{face.vertices[2].x(), face.vertices[2].y(), face.vertices[2].z()};

            writeTriangle(outputFile, a, b, c);
        }
        outputFile.close();
    }

    void exportSdfAsSTL(PreComputedSdf & sdf, const std::filesystem::path & filename)
    {
        const auto grid = createGridFromSdf(sdf, 1.f);
        exportGridToSTL(grid, filename);
    }

    void addTriangleToMesh(Mesh & mesh,
                           openvdb::Vec3s const & point1,
                           openvdb::Vec3s const & point2,
                           openvdb::Vec3s const & point3)
    {
        openvdb::Vec3s a = (point2 - point1);
        openvdb::Vec3s b = (point3 - point1);
        a.normalize();
        b.normalize();

        auto constexpr toVector3 = [](openvdb::Vec3s vec)
        { return Vector3(vec.x(), vec.y(), vec.z()); };

        openvdb::Vec3s normal = (a.cross(b));
        normal.normalize();

        mesh.addFace(
          Face{toVector3(normal), {toVector3(point1), toVector3(point2), toVector3(point3)}});
    }

    Mesh generatePreviewMesh(ComputeCore & generator, nodes::Assembly & assembly)
    {
        if (!generator.updateBBox())
        {
            throw std::runtime_error(
              "Computing bounding box failed. The model has probably not been compiled yet");
        }

        const auto bbox = generator.getBoundingBox();
        if (!bbox)
        {
            throw std::runtime_error("Mesh generation failed, bounding box is empty");
        }

        auto const matrixSize = 128;

        generator.setPreCompSdfSize(matrixSize);

        generator.precomputeSdfForBBox(*bbox);

        openvdb::Vec3d voxelSize{(bbox->max.x - bbox->min.x) / static_cast<double>(matrixSize),
                                 (bbox->max.y - bbox->min.y) / static_cast<double>(matrixSize),
                                 (bbox->max.z - bbox->min.z) / static_cast<double>(matrixSize)};

        auto const bandwidth =
          std::max(voxelSize.x(), std::max(voxelSize.y(), voxelSize.z())) * 2.f;
        auto grid =
          createGridFromSdf(generator.getResourceContext().getPrecompSdfBuffer(), bandwidth);

        if (!grid)
        {
            throw std::runtime_error("Mesh generation failed, voxel grid is empty");
        }

        const openvdb::Coord gridDim = grid->evalActiveVoxelDim();

        if ((gridDim.x() == 0) || (gridDim.y() == 0) || (gridDim.z() == 0))
        {
            throw std::runtime_error("Cannot generate mesh from empty distance matrix");
        }

        openvdb::math::Transform::Ptr transformation =
          openvdb::math::Transform::createLinearTransform();

        transformation->preScale(voxelSize);
        transformation->postTranslate(openvdb::Vec3d{static_cast<double>(bbox->min.x),
                                                     static_cast<double>(bbox->min.y),
                                                     static_cast<double>(bbox->min.z)});
        grid->setTransform(transformation);

        auto mesh = gridToMesh(grid, generator.getComputeContext());
        generator.computeVertexNormals(mesh);
        grid.reset();
        generator.getResourceContext().releasePreComputedSdf();
        return mesh;
    }

    Mesh gridToMesh(openvdb::FloatGrid::Ptr grid, ComputeContext & computeContext)
    {
        Mesh mesh(computeContext);

        if (grid->getGridClass() == openvdb::GRID_LEVEL_SET && grid->isType<openvdb::FloatGrid>())
        {
            using openvdb::Index64;
            openvdb::FloatGrid::Ptr floatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(grid);

            openvdb::tools::VolumeToMesh mesher(0., 0.5, true);
            mesher(*floatGrid);

            unsigned long trianglesWritten{};
            for (Index64 polyIndex = 0; polyIndex < mesher.polygonPoolListSize(); ++polyIndex)
            {
                const openvdb::tools::PolygonPool & polygons = mesher.polygonPoolList()[polyIndex];
                const auto numTriangles = polygons.numTriangles();
                for (Index64 triangleIndex = 0; triangleIndex < numTriangles; ++triangleIndex)
                {
                    const openvdb::Vec3I & triangle = polygons.triangle(triangleIndex);

                    openvdb::Vec3s const & point1 = mesher.pointList()[triangle[0]];
                    openvdb::Vec3s const & point2 = mesher.pointList()[triangle[1]];
                    openvdb::Vec3s const & point3 = mesher.pointList()[triangle[2]];

                    // points are reordered on purpose
                    addTriangleToMesh( // NOLINT(readability-suspicious-call-argument)
                      mesh,
                      point3,
                      point2,
                      point1); // NOLINT(readability-suspicious-call-argument)

                    ++trianglesWritten;
                }

                const auto numQuads = polygons.numQuads();
                for (Index64 quadIndex = 0; quadIndex < numQuads; ++quadIndex)
                {
                    const openvdb::Vec4I & quad = polygons.quad(quadIndex);
                    assert(quad[0] < mesher.pointListSize());
                    assert(quad[1] < mesher.pointListSize());
                    assert(quad[2] < mesher.pointListSize());
                    assert(quad[3] < mesher.pointListSize());

                    openvdb::Vec3s const & point1 = mesher.pointList()[quad[0]];
                    openvdb::Vec3s const & point2 = mesher.pointList()[quad[1]];
                    openvdb::Vec3s const & point3 = mesher.pointList()[quad[2]];
                    openvdb::Vec3s const & point4 = mesher.pointList()[quad[3]];

                    addTriangleToMesh(mesh,
                                      point3,
                                      point2,
                                      point1); // NOLINT(readability-suspicious-call-argument)
                                               // points are reordered on purpose
                    addTriangleToMesh(mesh,
                                      point1,
                                      point4,
                                      point3); // NOLINT(readability-suspicious-call-argument)
                                               // points are reordered on purpose
                }
            }
        }

        return mesh;
    }

    double alignToLayer(double value, double increment)
    {
        return std::floor(value / increment) * increment;
    }

    void MeshExporter::beginExport(const std::filesystem::path & fileName, ComputeCore & generator)
    {
        m_fileName = fileName;
        if (!generator.updateBBox())
        {
            throw std::runtime_error(
              "Computing bounding box failed. The model has probably not been compiled yet");
        }
        const auto bb = generator.getBoundingBox();
        generator.updateClippingAreaWithPadding();

        m_startHeight_mm = alignToLayer(bb->min.z - m_layerIncrement_mm, m_layerIncrement_mm);
        m_endHeight_mm = alignToLayer(bb->max.z + m_layerIncrement_mm, m_layerIncrement_mm);
        m_currentHeight_mm = m_startHeight_mm;
        m_currentHeight_mm = alignToLayer(m_currentHeight_mm, m_layerIncrement_mm);

        const auto z = static_cast<int>(std::floor(m_currentHeight_mm));

        generator.getResourceContext().requestDistanceMaps();

        const auto resX =
          generator.getResourceContext().getDistanceMipMaps()[m_qualityLevel]->getWidth();
        const auto width_mm = generator.getResourceContext().getClippingArea().z -
                              generator.getResourceContext().getClippingArea().x;
        const auto voxelSize = width_mm / static_cast<float>(resX);
        setLayerIncrement(voxelSize);
        generator.setSliceHeight(m_currentHeight_mm);

        std::cout << "Voxel size = " << voxelSize << "\n";

        m_grid = openvdb::FloatGrid::create(m_bandwidth_mm);
        m_grid->setGridClass(openvdb::GRID_LEVEL_SET);
        m_grid->setName("SDF computed by gladius");
        m_grid->setTransform(
          openvdb::math::Transform::createLinearTransform(static_cast<double>(voxelSize)));
    }

    bool MeshExporter::advanceExport(ComputeCore & generator)
    {
        generator.generateSdfSlice();

        auto & distmap = *(generator.getResourceContext().getDistanceMipMaps()[m_qualityLevel]);
        distmap.read();

        openvdb::FloatGrid::Accessor accessor = m_grid->getAccessor();

        const auto z = static_cast<int>(std::floor(m_currentHeight_mm / m_layerIncrement_mm));

        m_currentHeight_mm = alignToLayer(m_currentHeight_mm, m_layerIncrement_mm);

        for (int y = 0; y < static_cast<int>(distmap.getHeight()); ++y)
        {
            for (int x = 0; x < static_cast<int>(distmap.getWidth()); ++x)
            {
                openvdb::Coord xyz(x, y, z);
                const auto value =
                  std::clamp(distmap.getValue(x, y).x, -m_bandwidth_mm, m_bandwidth_mm);

                accessor.setValue(xyz, value);
            }
        }

        m_grid->pruneGrid();
        m_currentHeight_mm += m_layerIncrement_mm;
        m_currentHeight_mm = alignToLayer(m_currentHeight_mm, m_layerIncrement_mm);

        generator.setSliceHeight(m_currentHeight_mm);

        m_progress =
          ((generator.getSliceHeight() - m_startHeight_mm) / (m_endHeight_mm - m_startHeight_mm));
        return generator.getSliceHeight() < generator.getBoundingBox()->max.z + m_layerIncrement_mm;
    }

    void MeshExporter::finalize()
    {
        m_grid.reset();
    }

    void MeshExporter::finalizeExportVdb()
    {
        openvdb::io::File file(m_fileName.string());
        openvdb::GridPtrVec grids;
        m_grid->pruneGrid();
        grids.push_back(m_grid);

        file.write(grids);
        file.close();
        m_grid.reset();
    }

    void MeshExporter::finalizeExportSTL(ComputeCore & core)
    {
        auto mesh = gridToMesh(m_grid, core.getComputeContext());
        // core.adoptVertexOfMeshToSurface(mesh.getVertices());
        exportMeshToSTL(mesh, m_fileName);
        m_grid.reset();
    }

    void MeshExporter::finalizeExportNanoVdb()
    {
        try
        {
            auto handle = nanovdb::openToNanoVDB(m_grid);
            std::vector<nanovdb::GridHandle<>> handles;
            handles.push_back(std::move(handle));
            nanovdb::io::writeGrids<nanovdb::HostBuffer, std::vector>(m_fileName.string(), handles);
        }
        catch (const std::exception & e)
        {
            std::cerr << "Writing of nanovdb failed with the following exception:" << e.what()
                      << "\n";
        }
        m_grid.reset();
    }

    double MeshExporter::getProgress() const
    {
        return m_progress;
    }

    void MeshExporter::setQualityLevel(size_t qualityLevel)
    {
        m_qualityLevel = qualityLevel;
    }

    void MeshExporter::setLayerIncrement(float increment_mm)
    {
        m_layerIncrement_mm = increment_mm;
        m_bandwidth_mm = m_layerIncrement_mm * 2.f;
    }
}
