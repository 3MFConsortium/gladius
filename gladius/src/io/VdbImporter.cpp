#include "VdbImporter.h"

#include "src/Document.h"
#include "nodes/Assembly.h"
#include "nodes/Model.h"
#include "src/Primitives.h"
#include "src/ResourceContext.h"
#include "vdb.h"

#include <cstring>
#include <fmt/format.h>

#include "src/exceptions.h"
#include "src/types.h"
#include "src/Profiling.h"




namespace gladius::vdb
{
   

    size_t TriangleMesh::polygonCount() const
    {
        return indices.size();
    }

    size_t TriangleMesh::pointCount() const
    {
        return vertices.size();
    }

    size_t TriangleMesh::vertexCount(size_t n)
    {
        return 3;
    }

    void TriangleMesh::getIndexSpacePoint(size_t faceIndex, size_t v, openvdb::Vec3d & pos) const
    {
        auto const & index = indices.at(faceIndex);
        auto const & vertex = vertices.at(index[v]);

        pos = {vertex.x(), vertex.y(), vertex.z()};
    }

    void VdbImporter::load(const std::filesystem::path & vdbFilename,
                           PrimitiveBuffer & primitives) const
    {
        ProfileFunction
        try
        {
            
            const openvdb::GridBase::Ptr sdfGrid = load(vdbFilename);         
            importFromGrid<float>(sdfGrid, primitives, 1.f);
        }
        catch (const std::exception & e)
        {
            std::cerr << "Failed to load nanovdb file. An exception occurred:" << e.what() << "\n";
        }
    }

    openvdb::GridBase::Ptr VdbImporter::load(std::filesystem::path const & vdbFilename) const
    {
        ProfileFunction
        try
        {
            openvdb::initialize();
            openvdb::io::File vdbFile(vdbFilename.string());
            vdbFile.open();
            return vdbFile.getGrids()->front();            
        }
        catch (const std::exception & e)
        {
            std::cerr << "Failed to load nanovdb file. An exception occurred:" << e.what() << "\n";
        }
        return {};
    }

    void VdbImporter::loadStl(const std::filesystem::path & stlFilename)
    {
        ProfileFunction
        m_mesh = loadStlAsMesh(stlFilename);
    }

    void VdbImporter::writeMesh(gladius::PrimitiveBuffer & primitives) const
    {
        writeMesh(m_mesh, primitives);
    }

    
    void VdbImporter::writeMesh(TriangleMesh const& mesh, gladius::PrimitiveBuffer& primitives)
    {
        writeFlatMesh(mesh, primitives);

        float constexpr scaling = 5.f;
        auto upScaledMesh = mesh;
        upScaledMesh.applyScaling(scaling);

        ImportSettings nearFieldSdfConfig;
        nearFieldSdfConfig.m_voxelSize_mm = 1.f / scaling;
        nearFieldSdfConfig.m_halfBandwidth_mm = 8.f * nearFieldSdfConfig.m_voxelSize_mm * scaling;
        nearFieldSdfConfig.representation = Representation::NearDistanceField;
        importFromMesh(upScaledMesh, primitives, nearFieldSdfConfig);

        ImportSettings faceIndexVoxelConfigFar;
        faceIndexVoxelConfigFar.m_voxelSize_mm = 1.f;
        faceIndexVoxelConfigFar.m_halfBandwidth_mm = 150.f;
        faceIndexVoxelConfigFar.representation = Representation::FaceIndex;
        importFromMesh(mesh, primitives, faceIndexVoxelConfigFar);

        ImportSettings faceIndexVoxelConfigNear;
        faceIndexVoxelConfigNear.m_voxelSize_mm = 1.f / scaling;
        faceIndexVoxelConfigNear.m_halfBandwidth_mm = 10.f * scaling;
        faceIndexVoxelConfigNear.representation = Representation::FaceIndex;
        importFromMesh(upScaledMesh, primitives, faceIndexVoxelConfigNear);
    }

    void VdbImporter::importFromMesh(TriangleMesh const & mesh,
                                     PrimitiveBuffer & primitives,
                                     ImportSettings & config)
    {
        ProfileFunction
        const auto transform = openvdb::math::Transform::createLinearTransform(1.);
        float const scaling = 1.f / config.m_voxelSize_mm;

        if (config.representation == Representation::NearDistanceField)
        {
            auto const levelSet = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
              *transform, mesh.vertices, mesh.indices, config.m_halfBandwidth_mm);

            openvdb::tools::changeBackground(levelSet->tree(), std::numeric_limits<float>::max());
            levelSet->pruneGrid();
            importFromGrid<float>(levelSet, primitives, scaling);
            levelSet->clear();
        }

        openvdb::Int32Grid::Ptr faceIndexGrid = std::make_shared<openvdb::Int32Grid>();
        faceIndexGrid->setTransform(transform);
        auto const levelSet = openvdb::tools::meshToVolume<openvdb::FloatGrid, TriangleMesh>(
          mesh,
          *transform,
          static_cast<double>(config.m_halfBandwidth_mm),
          static_cast<double>(config.m_halfBandwidth_mm),
          0,
          faceIndexGrid.get());

        openvdb::tools::changeBackground(levelSet->tree(), config.m_halfBandwidth_mm);

        switch (config.representation)
        {
        case Representation::FaceIndex:
        {
            openvdb::tools::changeBackground(faceIndexGrid->tree(), -1);
            faceIndexGrid->setTransform(transform);
            faceIndexGrid->pruneGrid();
            importFromGrid<openvdb::Int32>(faceIndexGrid, primitives, scaling);
        }
        break;
        case Representation::Binary:
        {
            const auto binaryMatrix = openvdb::tools::interiorMask(*levelSet, 0.);
            openvdb::tools::changeBackground(binaryMatrix->tree(), false);
            binaryMatrix->pruneGrid();
            importFromGrid<bool>(binaryMatrix, primitives, scaling);
        }
        break;
        default:;
        }
    }

    TriangleMesh VdbImporter::loadStlAsMesh(std::filesystem::path const & stlFilename) const
    {
        ProfileFunction
        TriangleMesh mesh;

        std::ifstream stlFile(stlFilename.c_str(), std::ios::in | std::ios::binary);
        if (!stlFile)
        {
            throw std::runtime_error(fmt::format("Cannot open {}", stlFilename.string()));
        }

        char header[80] = "";
        char buf4[4] = "";

        if (stlFile)
        {
            try
            {
                auto const fileSize = std::filesystem::file_size(stlFilename);

                stlFile.read(header, 80);

                stlFile.read(buf4, 4);
                uint32_t numberOfTriangles = *reinterpret_cast<uint32_t *>(buf4);

                if (numberOfTriangles > fileSize)
                {
                    throw NoValidBinaryStlFile(
                      fmt::format("{} is not a valid binary stl file", stlFilename.string()));
                }

                long index = 0;
                for (decltype(numberOfTriangles) i = 0; i < numberOfTriangles; ++i)
                {
                    for (auto edge = 0; edge < 4; ++edge)
                    {
                        stlFile.read(buf4, 4);
                        float const x = *reinterpret_cast<float *>(buf4);

                        stlFile.read(buf4, 4);
                        float const y = *reinterpret_cast<float *>(buf4);

                        stlFile.read(buf4, 4);
                        float const z = *reinterpret_cast<float *>(buf4);

                        openvdb::Vec3s vertex(x, y, z);

                        if (edge > 0)
                        {
                            mesh.vertices.push_back(vertex);
                        }
                    }

                    auto indexVector = openvdb::Vec3I(index, index + 1, index + 2);
                    index += 3;
                    mesh.indices.push_back(indexVector);

                    // attribute
                    char buf2[2] = "";
                    stlFile.read(buf2, 2);
                }
                if (mesh.vertices.empty())
                {
                    throw std::runtime_error(
                      fmt::format("{} did not contain any triangles.", stlFilename.string()));
                }
            }
            catch (std::ifstream::failure const & e)
            {
                std::cerr << "Exception opening/reading/closing file\n" << e.what() << std::endl;
            }
            stlFile.close();
        }

        return mesh;
    }

    void VdbImporter::writeFlatMesh(TriangleMesh const & mesh, PrimitiveBuffer & primitives)
    {
        ProfileFunction
        PrimitiveMeta metaData{};
        metaData.primitiveType = SDF_MESH_TRIANGLES;
        metaData.start = static_cast<int>(primitives.data.size());

        auto pushVector = [&](gladius::Vector3 const & vector)
        {
            primitives.data.push_back(vector.x());
            primitives.data.push_back(vector.y());
            primitives.data.push_back(vector.z());
        };

        for (auto vertexIter = mesh.vertices.cbegin(); vertexIter != mesh.vertices.cend();
             ++vertexIter)
        {
            auto const a = Vector3{vertexIter->x(), vertexIter->y(), vertexIter->z()};
            ++vertexIter;
            auto const b = Vector3{vertexIter->x(), vertexIter->y(), vertexIter->z()};
            ++vertexIter;
            auto const c = Vector3{vertexIter->x(), vertexIter->y(), vertexIter->z()};

            pushVector(a);
            pushVector(b);
            pushVector(c);
        }

        metaData.end = static_cast<int>(primitives.data.size());
        primitives.meta.push_back(metaData);
    }
    
    TriangleMesh fromBoundingBox(openvdb::Vec3s const & min, openvdb::Vec3s const & max)
    {
        TriangleMesh mesh;
        mesh.vertices.push_back(min);
        mesh.vertices.push_back(openvdb::Vec3s{max.x(), min.y(), min.z()});
        mesh.vertices.push_back(openvdb::Vec3s{min.x(), max.y(), min.z()});
        mesh.vertices.push_back(openvdb::Vec3s{max.x(), max.y(), min.z()});
        mesh.vertices.push_back(openvdb::Vec3s{min.x(), min.y(), max.z()});
        mesh.vertices.push_back(openvdb::Vec3s{max.x(), min.y(), max.z()});
        mesh.vertices.push_back(openvdb::Vec3s{min.x(), max.y(), max.z()});
        mesh.vertices.push_back(max);

        mesh.indices.push_back(openvdb::Vec3I(0, 1, 2));
        mesh.indices.push_back(openvdb::Vec3I(1, 3, 2));
        mesh.indices.push_back(openvdb::Vec3I(4, 6, 5));
        mesh.indices.push_back(openvdb::Vec3I(5, 6, 7));
        mesh.indices.push_back(openvdb::Vec3I(0, 2, 4));
        mesh.indices.push_back(openvdb::Vec3I(2, 6, 4));
        mesh.indices.push_back(openvdb::Vec3I(1, 5, 3));
        mesh.indices.push_back(openvdb::Vec3I(3, 5, 7));
        mesh.indices.push_back(openvdb::Vec3I(0, 4, 1));
        mesh.indices.push_back(openvdb::Vec3I(1, 4, 5));
        mesh.indices.push_back(openvdb::Vec3I(2, 3, 6));
        mesh.indices.push_back(openvdb::Vec3I(3, 7, 6));

        return mesh;
    }
}
