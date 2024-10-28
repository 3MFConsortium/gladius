#pragma once
#include "Parameter.h"
#include "src/Primitives.h"
#include "src/Profiling.h"
#include "vdb.h"
#include <filesystem>
#include <nanovdb/util/HostBuffer.h>

namespace gladius::vdb
{
    enum class Representation
    {
        NearDistanceField,
        Binary,
        FaceIndex
    };

    struct ImportSettings
    {
        float m_halfBandwidth_mm{20.f};
        float m_voxelSize_mm{0.5f};
        Representation representation = Representation::NearDistanceField;
    };



    struct TriangleMesh
    {
        std::vector<openvdb::Vec3s> vertices;
        std::vector<openvdb::Vec3I> indices;

        size_t polygonCount() const;         // Total number of polygons
        size_t pointCount() const;           // Total number of points
        static size_t vertexCount(size_t n); // Vertex count for polygon n

        // Return position pos in local grid index space for polygon n and vertex v
        void getIndexSpacePoint(size_t n, size_t v, openvdb::Vec3d & pos) const;

        void applyScaling(float scaling)
        {
            for (auto & vertex : vertices)
            {
                vertex *= scaling;
            }
        }

        void
        addTriangle(openvdb::Vec3s const & a, openvdb::Vec3s const & b, openvdb::Vec3s const & c)
        {

            updateMinMax(a);
            updateMinMax(b);
            updateMinMax(c);

            auto const currentIndex = vertices.size();
            vertices.push_back(a);
            vertices.push_back(b);
            vertices.push_back(c);

            indices.emplace_back(openvdb::Vec3i(currentIndex, currentIndex + 1, currentIndex + 2));
        }

        gladius::nodes::float3 getMin() const
        {
            return gladius::nodes::float3(m_min.x(), m_min.y(), m_min.z());
        }

        gladius::nodes::float3 getMax() const
        {
            return gladius::nodes::float3(m_max.x(), m_max.y(), m_max.z());
        }

      private:
        openvdb::Vec3s m_min{std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()};
        openvdb::Vec3s m_max{-std::numeric_limits<float>::max(),
                             -std::numeric_limits<float>::max(),
                             -std::numeric_limits<float>::max()};

        void updateMinMax(openvdb::Vec3s const & vertex)
        {
            m_min = openvdb::Vec3s(std::min(m_min.x(), vertex.x()),
                                   std::min(m_min.y(), vertex.y()),
                                   std::min(m_min.z(), vertex.z()));

            m_max = openvdb::Vec3s(std::max(m_max.x(), vertex.x()),
                                   std::max(m_max.y(), vertex.y()),
                                   std::max(m_max.z(), vertex.z()));
        }
    };

    TriangleMesh fromBoundingBox(openvdb::Vec3s const & min, openvdb::Vec3s const & max);

    // template <typename T>
    // void
    // importFromGridUint8(openvdb::GridBase::Ptr sdfGrid, PrimitiveBuffer & primitives, float scaling)
    // {
    //     ProfileFunction PrimitiveMeta metaData{};

    //     metaData.primitiveType = SDF_VDB_GRAYSCALE_8BIT;

    //     auto handle = nanovdb::createNanoGrid<io::OneChannelCharGrid, uint8_t, nanovdb::HostBuffer>(
    //       *sdfGrid, nanovdb::StatsMode::Default, nanovdb::ChecksumMode::Default, 0);
    //     auto * grid = handle.grid<T>();

    //     if (!grid)
    //     {
    //         throw std::runtime_error("File did not contain a grid with value type float");
    //     }

    //     metaData.scaling = scaling;
    //     metaData.start = static_cast<int>(primitives.data.size());

    //     auto worldBBox = grid->worldBBox();

    //     metaData.boundingBox.max.x = static_cast<float>(worldBBox.max()[0]);
    //     metaData.boundingBox.max.y = static_cast<float>(worldBBox.max()[1]);
    //     metaData.boundingBox.max.z = static_cast<float>(worldBBox.max()[2]);

    //     metaData.boundingBox.min.x = static_cast<float>(worldBBox.min()[0]);
    //     metaData.boundingBox.min.y = static_cast<float>(worldBBox.min()[1]);
    //     metaData.boundingBox.min.z = static_cast<float>(worldBBox.min()[2]);

    //     auto & targetBuffer = primitives.data;

    //     auto const required32BitBlocks = static_cast<size_t>(ceil(handle.size() / 4.));

    //     targetBuffer.resize(metaData.start + required32BitBlocks);
    //     void * dstPtr = &*(targetBuffer.begin() + metaData.start);
    //     memcpy(dstPtr, handle.data(), handle.size());

    //     metaData.end = static_cast<int>(primitives.data.size());
    //     primitives.meta.push_back(metaData);
    // }

    template <typename T>
    void importFromGrid(openvdb::GridBase::Ptr sdfGrid, PrimitiveBuffer & primitives, float scaling)
    {
        ProfileFunction

          PrimitiveMeta metaData{};

        if constexpr (std::is_same_v<T, float>)
        {
            metaData.primitiveType = SDF_VDB;
        }

        if constexpr (std::is_same_v<T, bool>)
        {
            metaData.primitiveType = SDF_VDB_BINARY;
        }

        if constexpr (std::is_same_v<T, openvdb::Int32>)
        {
            metaData.primitiveType = SDF_VDB_FACE_INDICES;
        }

        // if constexpr (std::is_same_v<T, uint8_t>)
        // {
        //     metaData.primitiveType = SDF_VDB_GRAYSCALE_8BIT;
        //     return importFromGridUint8<T>(sdfGrid, primitives, scaling);
        // }

        auto handle = nanovdb::openToNanoVDB(sdfGrid);
        auto * grid = handle.grid<T>();

        if (!grid)
        {
            throw std::runtime_error("File did not contain a grid with value type float");
        }

        metaData.scaling = scaling;
        metaData.start = static_cast<int>(primitives.data.size());

        auto worldBBox = grid->worldBBox();

        metaData.boundingBox.max.x = static_cast<float>(worldBBox.max()[0]);
        metaData.boundingBox.max.y = static_cast<float>(worldBBox.max()[1]);
        metaData.boundingBox.max.z = static_cast<float>(worldBBox.max()[2]);

        metaData.boundingBox.min.x = static_cast<float>(worldBBox.min()[0]);
        metaData.boundingBox.min.y = static_cast<float>(worldBBox.min()[1]);
        metaData.boundingBox.min.z = static_cast<float>(worldBBox.min()[2]);

        auto & targetBuffer = primitives.data;

        auto const required32BitBlocks = static_cast<size_t>(ceil(handle.size() / 4.));

        targetBuffer.resize(metaData.start + required32BitBlocks);
        void * dstPtr = &*(targetBuffer.begin() + metaData.start);
        memcpy(dstPtr, handle.data(), handle.size());

        metaData.end = static_cast<int>(primitives.data.size());
        primitives.meta.push_back(metaData);
    }

    class VdbImporter
    {
      public:
        void load(std::filesystem::path const & vdbFilename, PrimitiveBuffer & primitives) const;
        openvdb::GridBase::Ptr load(std::filesystem::path const & vdbFilename) const;
        void loadStl(std::filesystem::path const & stlFilename);

        void writeMesh(gladius::PrimitiveBuffer & primitives) const;

        static void writeMesh(TriangleMesh const & mesh, gladius::PrimitiveBuffer & primitives);

        [[nodiscard]] TriangleMesh const & getMesh() const
        {
            return m_mesh;
        }

      private:
        static void importFromMesh(TriangleMesh const & mesh,
                                   gladius::PrimitiveBuffer & primitives,
                                   ImportSettings & config);

        TriangleMesh loadStlAsMesh(std::filesystem::path const & stlFilename) const;
        void createFaceIndexGrid(TriangleMesh const & mesh,
                                 gladius::PrimitiveBuffer & primitives,
                                 ImportSettings & config) const;

        /**
         * \brief Writes the vertices (no kd-Tree)
         * \param mesh
         * \param primitives
         */
        static void writeFlatMesh(TriangleMesh const & mesh, gladius::PrimitiveBuffer & primitives);

        TriangleMesh m_mesh;
    };

}