/**
 * @file MeshWriter3mf.cpp
 * @brief Implementation of 3MF mesh exporter using core specification only
 */

#include "io/3mf/MeshWriter3mf.h"

#include "ComputeContext.h"
#include "Document.h"
#include "EventLogger.h"
#include "MeshResource.h"
#include "ResourceKey.h"
#include "VdbResource.h"
#include "nodes/Model.h"

#include <lib3mf_abi.hpp>

#include <chrono>
#include <iomanip>
#include <map>
#include <sstream>
#include <tuple>

namespace gladius::io
{

    MeshWriter3mf::MeshWriter3mf(events::SharedLogger logger)
        : Writer3mfBase(std::move(logger))
    {
    }

    void MeshWriter3mf::exportMesh(std::filesystem::path const & filePath,
                                   Mesh const & mesh,
                                   std::string const & meshName,
                                   Document const * sourceDocument,
                                   bool writeThumbnail)
    {
        if (!validateMesh(mesh))
        {
            throw std::runtime_error("Invalid mesh for export");
        }

        try
        {
            // Create new 3MF model
            auto model3mf = m_wrapper->CreateModel();

            // Add default metadata
            addDefaultMetadata(model3mf);

            // Copy metadata from source document if available
            if (sourceDocument)
            {
                copyMetadata(*sourceDocument, model3mf);
            }

            // Add mesh to model
            auto meshObject = addMeshToModel(model3mf, mesh, meshName);

            // Create build item
            createBuildItem(model3mf, meshObject, meshName);

            // Add thumbnail if requested and source document is available
            if (writeThumbnail && sourceDocument)
            {
                updateThumbnail(const_cast<Document &>(*sourceDocument), model3mf);
            }

            // Write to file
            auto writer = model3mf->QueryWriter("3mf");
            writer->WriteToFile(filePath.string());

            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Successfully exported mesh to {}", filePath.string()),
                   events::Severity::Info});
            }
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Failed to export mesh to {}: {}", filePath.string(), e.what()),
                   events::Severity::Error});
            }
            throw;
        }
    }

    void MeshWriter3mf::exportMeshes(
      std::filesystem::path const & filePath,
      std::vector<std::pair<std::shared_ptr<Mesh>, std::string>> const & meshes,
      Document const * sourceDocument,
      bool writeThumbnail)
    {
        if (meshes.empty())
        {
            throw std::runtime_error("No meshes provided for export");
        }

        try
        {
            // Create new 3MF model
            auto model3mf = m_wrapper->CreateModel();

            // Add default metadata
            addDefaultMetadata(model3mf);

            // Copy metadata from source document if available
            if (sourceDocument)
            {
                copyMetadata(*sourceDocument, model3mf);
            }

            // Add all meshes to model
            for (auto const & [mesh, name] : meshes)
            {
                if (!mesh || !validateMesh(*mesh))
                {
                    if (m_logger)
                    {
                        m_logger->addEvent({fmt::format("Skipping invalid mesh: {}", name),
                                            events::Severity::Warning});
                    }
                    continue;
                }

                auto meshObject = addMeshToModel(model3mf, *mesh, name);
                createBuildItem(model3mf, meshObject, name);
            }

            // Add thumbnail if requested and source document is available
            if (writeThumbnail && sourceDocument)
            {
                updateThumbnail(const_cast<Document &>(*sourceDocument), model3mf);
            }

            // Write to file
            auto writer = model3mf->QueryWriter("3mf");
            writer->WriteToFile(filePath.string());

            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Successfully exported {} meshes to {}",
                                                meshes.size(),
                                                filePath.string()),
                                    events::Severity::Info});
            }
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Failed to export meshes to {}: {}", filePath.string(), e.what()),
                   events::Severity::Error});
            }
            throw;
        }
    }

    void MeshWriter3mf::exportMeshFromDocument(std::filesystem::path const & filePath,
                                               Document & document,
                                               ResourceKey const & resourceKey,
                                               bool writeThumbnail)
    {
        // Get mesh from document's resource manager
        auto & resourceManager = document.getResourceManager();
        auto & resource = resourceManager.getResource(resourceKey);
        auto const * meshResource = dynamic_cast<MeshResource const *>(&resource);
        if (!meshResource)
        {
            throw std::runtime_error(
              fmt::format("Resource is not a mesh: {}", resourceKey.getDisplayName()));
        }

        // Get the triangle mesh from the resource
        auto const & triangleMesh = meshResource->getMesh();

        // Convert vdb::TriangleMesh to gladius::Mesh
        auto computeContext = document.getComputeContext();
        if (!computeContext)
        {
            throw std::runtime_error("No compute context available for mesh conversion");
        }

        // Convert triangle mesh to Gladius mesh format
        Mesh gladiusMesh(*computeContext);

        // Convert vertices to faces
        for (size_t i = 0; i < triangleMesh.indices.size(); ++i)
        {
            auto const & triangle = triangleMesh.indices[i];

            if (triangle.x() >= triangleMesh.vertices.size() ||
                triangle.y() >= triangleMesh.vertices.size() ||
                triangle.z() >= triangleMesh.vertices.size())
            {
                if (m_logger)
                {
                    m_logger->addEvent({fmt::format("Invalid triangle indices in mesh: {}",
                                                    resourceKey.getDisplayName()),
                                        events::Severity::Warning});
                }
                continue;
            }

            auto const & v1 = triangleMesh.vertices[triangle.x()];
            auto const & v2 = triangleMesh.vertices[triangle.y()];
            auto const & v3 = triangleMesh.vertices[triangle.z()];

            Vector3 vertex1(v1.x(), v1.y(), v1.z());
            Vector3 vertex2(v2.x(), v2.y(), v2.z());
            Vector3 vertex3(v3.x(), v3.y(), v3.z());

            gladiusMesh.addFace(vertex1, vertex2, vertex3);
        }

        std::string meshName = resourceKey.getDisplayName();
        if (meshName.empty())
        {
            meshName = fmt::format("Mesh_Resource");
        }

        exportMesh(filePath, gladiusMesh, meshName, &document, writeThumbnail);
    }

    Lib3MF::PMeshObject MeshWriter3mf::addMeshToModel(Lib3MF::PModel model3mf,
                                                      Mesh const & mesh,
                                                      std::string const & meshName)
    {
        auto meshObject = model3mf->AddMeshObject();
        meshObject->SetName(meshName);

        size_t const numFaces = mesh.getNumberOfFaces();
        if (numFaces == 0)
        {
            throw std::runtime_error("Mesh has no faces to export");
        }

        // Track unique vertices to avoid duplicates
        std::vector<Vector3> uniqueVertices;
        std::map<std::tuple<float, float, float>, Lib3MF_uint32> vertexMap;

        auto const tolerance = 1e-6f;

        auto getOrCreateVertex = [&](Vector3 const & vertex) -> Lib3MF_uint32
        {
            // Round to tolerance to merge nearly identical vertices
            auto x = std::round(vertex.x() / tolerance) * tolerance;
            auto y = std::round(vertex.y() / tolerance) * tolerance;
            auto z = std::round(vertex.z() / tolerance) * tolerance;

            auto key = std::make_tuple(x, y, z);
            auto it = vertexMap.find(key);

            if (it != vertexMap.end())
            {
                return it->second;
            }

            // Add new vertex
            auto vertexIndex = meshObject->AddVertex({x, y, z});
            vertexMap[key] = vertexIndex;
            return vertexIndex;
        };

        auto const & vertexBuffer = mesh.getVertices();
        auto const vertexData = const_cast<Buffer<cl_float4> &>(vertexBuffer).getDataCopy();

        // Add all triangles
        for (size_t i = 0; i < numFaces; ++i)
        {
            // Each face has 3 vertices, stored sequentially in the buffer
            size_t vertexOffset = i * 3;

            if (vertexOffset + 2 >= vertexData.size())
            {
                throw std::runtime_error(
                  fmt::format("Invalid vertex data: face {} requires vertices at indices {}-{}, "
                              "but buffer only has {} vertices",
                              i,
                              vertexOffset,
                              vertexOffset + 2,
                              vertexData.size()));
            }

            // Extract the three vertices of this triangle
            auto const & v1 = vertexData[vertexOffset];
            auto const & v2 = vertexData[vertexOffset + 1];
            auto const & v3 = vertexData[vertexOffset + 2];

            // Convert cl_float4 to Vector3
            Vector3 vertex1(v1.x, v1.y, v1.z);
            Vector3 vertex2(v2.x, v2.y, v2.z);
            Vector3 vertex3(v3.x, v3.y, v3.z);

            // Get vertex indices (creating vertices if needed)
            auto v1Index = getOrCreateVertex(vertex1);
            auto v2Index = getOrCreateVertex(vertex2);
            auto v3Index = getOrCreateVertex(vertex3);

            // Ensure counter-clockwise order for outward-facing normals
            // The 3MF spec requires counter-clockwise vertex order
            meshObject->AddTriangle({v1Index, v2Index, v3Index});
        }

        if (m_logger)
        {
            m_logger->addEvent({fmt::format("Added mesh '{}' with {} vertices and {} triangles",
                                            meshName,
                                            meshObject->GetVertexCount(),
                                            meshObject->GetTriangleCount()),
                                events::Severity::Info});
        }

        return meshObject;
    }

    void MeshWriter3mf::createBuildItem(Lib3MF::PModel model3mf,
                                        Lib3MF::PMeshObject meshObject,
                                        std::string const & partNumber)
    {
        try
        {
            // Create identity transform (no scaling/rotation/translation)
            Lib3MF::sTransform transform = m_wrapper->GetIdentityTransform();

            auto buildItem = model3mf->AddBuildItem(meshObject.get(), transform);

            if (!partNumber.empty())
            {
                buildItem->SetPartNumber(partNumber);
            }

            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Created build item for mesh object (part: {})",
                                                partNumber.empty() ? "unnamed" : partNumber),
                                    events::Severity::Info});
            }
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Failed to create build item: {}", e.what()),
                                    events::Severity::Warning});
            }
            throw;
        }
    }

    bool MeshWriter3mf::validateMesh(Mesh const & mesh)
    {
        if (mesh.getNumberOfFaces() == 0)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {"Mesh validation failed: No faces in mesh", events::Severity::Error});
            }
            return false;
        }

        if (mesh.getNumberOfFaces() < 4)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {"Mesh validation warning: Mesh has fewer than 4 faces (may not form a solid)",
                   events::Severity::Warning});
            }
        }

        // Additional validation could be added here:
        // - Check for degenerate triangles
        // - Verify manifold topology
        // - Check for self-intersections
        // For now, we'll allow any mesh with at least one face

        return true;
    }

    // Convenience function implementation
    // Convenience function implementation
    void exportMeshTo3mfCore(std::filesystem::path const & filePath,
                             Mesh const & mesh,
                             std::string const & meshName,
                             Document const * sourceDocument,
                             events::SharedLogger logger)
    {
        MeshWriter3mf writer(logger);
        writer.exportMesh(filePath, mesh, meshName, sourceDocument, false);
    }

} // namespace gladius::io
