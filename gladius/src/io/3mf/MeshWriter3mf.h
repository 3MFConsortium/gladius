/**
 * @file MeshWriter3mf.h
 * @brief 3MF mesh exporter that uses only the core 3MF specification
 *
 * This writer exports meshes to 3MF format using only features from the core specification,
 * making the files compatible with any 3MF-compliant software. It does not use extensions
 * like volumetric or implicit functions.
 */

#pragma once

#include "EventLogger.h"
#include "Mesh.h"
#include "io/3mf/Writer3mfBase.h"

#include <lib3mf_implicit.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace gladius
{
    class Document;
    class ResourceKey;
} // namespace gladius

namespace gladius::io
{

    class MeshWriter3mf : public Writer3mfBase
    {
      public:
        explicit MeshWriter3mf(events::SharedLogger logger);

        void exportMesh(std::filesystem::path const & filePath,
                        Mesh const & mesh,
                        std::string const & meshName,
                        Document const * sourceDocument = nullptr,
                        bool writeThumbnail = false);

        void exportMeshes(std::filesystem::path const & filePath,
                          std::vector<std::pair<std::shared_ptr<Mesh>, std::string>> const & meshes,
                          Document const * sourceDocument = nullptr,
                          bool writeThumbnail = false);

        void exportMeshFromDocument(std::filesystem::path const & filePath,
                                    Document & document,
                                    ResourceKey const & resourceKey,
                                    bool writeThumbnail = true);

        bool validateMesh(Mesh const & mesh);

      private:
        Lib3MF::PMeshObject
        addMeshToModel(Lib3MF::PModel model3mf, Mesh const & mesh, std::string const & meshName);
        void createBuildItem(Lib3MF::PModel model3mf,
                             Lib3MF::PMeshObject meshObject,
                             std::string const & partNumber);
    };

    void exportMeshTo3mfCore(std::filesystem::path const & filePath,
                             Mesh const & mesh,
                             std::string const & meshName,
                             Document const * sourceDocument,
                             events::SharedLogger logger);

} // namespace gladius::io
