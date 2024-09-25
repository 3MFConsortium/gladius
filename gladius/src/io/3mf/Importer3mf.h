#pragma once

#include "Mesh.h"
#include "nodes/Assembly.h"
#include "nodes/Model.h"

#include <filesystem>
#include <unordered_map>

#include "EventLogger.h"

#include <lib3mf/Cpp/lib3mf_implicit.hpp>

namespace gladius
{
    class Document;
}

namespace gladius::nodes
{
    class Builder;
}

namespace gladius::io
{
    using MeshId = int;

    using MeshResources = std::unordered_map<MeshId, SharedMesh>;

    using Id3mfToNodeMap = std::unordered_map<std::string, gladius::nodes::NodeBase *>;
    using NodeMaps = std::unordered_map<ResourceId, Id3mfToNodeMap>;

    class Importer3mf
    {
      public:
        explicit Importer3mf(events::SharedLogger logger);

        void load(std::filesystem::path const & filename, Document & doc);

        void merge(std::filesystem::path const & filename, Document & doc);
        [[nodiscard]] Lib3MF::PWrapper get3mfWrapper() const;

        /**
         * @brief Loads all mesh objects from a 3MF model and adds them to the document.
         *
         * @param model The 3MF model to load the mesh objects from.
         * @param doc The document to add the loaded mesh objects to.
         */
        void loadMeshes(Lib3MF::PModel model, Document & doc);

      private:
        void logWarnings(std::filesystem::path const & filename, Lib3MF::PReader reader);

        void loadComponentObjects(Lib3MF::PModel model, Document & doc);

        /**
         * @brief Loads all implicit functions from a 3MF model and adds them to the document.
         *
         * @param model The 3MF model to load the implicit functions from.
         * @param doc The document to add the loaded implicit functions to.
         */
        void loadImplicitFunctions(Lib3MF::PModel model, Document & doc);

        nodes::Port * resolveInput(nodes::Model & model, Lib3MF::PImplicitPort & input);

        /**
         * @brief Loads a mesh object from a 3MF model and adds it to the document.
         *
         * @param model The 3MF model to load the mesh obj  ect from.
         * @param meshObject The mesh object to load.
         * @param doc The document to add the loaded mesh object to.
         */
        void
        loadMeshIfNecessary(Lib3MF::PModel model, Lib3MF::PMeshObject meshObject, Document & doc);

        void addMeshObject(Lib3MF::PModel model,
                           ResourceKey const & key,
                           Lib3MF::PMeshObject meshObject,
                           nodes::Matrix4x4 const & trafo,
                           Document & doc);

        void addVolumeData(Lib3MF::PVolumeData & volume,
                           Lib3MF::PModel & model,
                           gladius::Document & doc,
                           gladius::nodes::Builder & builder,
                           gladius::nodes::Port & coordinateSystemPort);

        void addLevelSetObject(Lib3MF::PModel model,
                               ResourceKey const & key,
                               Lib3MF::PLevelSet levelSetObject,
                               nodes::Matrix4x4 const & trafo,
                               Document & doc);

        void loadImageStacks(std::filesystem::path const & filename,
                             Lib3MF::PModel model,
                             Document & doc);

        void loadBuildItems(Lib3MF::PModel model, Document & doc);

        void createObject(Lib3MF::CObject & objectRes,
                          Lib3MF::PModel & model,
                          gladius::ResourceKey & key,
                          const gladius::nodes::Matrix4x4 & trafo,
                          gladius::Document & doc);

        void processImplicitFunction(Document & doc, Lib3MF::CImplicitFunction * func);

        void processFunctionFromImage3d(Document & doc, Lib3MF::CFunctionFromImage3D * func);

        void connectOutputs(gladius::nodes::Model & model,
                            gladius::nodes::NodeBase & endNode,
                            Lib3MF::CImplicitFunction & func);

        void connectNode(Lib3MF::CImplicitNode & node3mf,
                         Lib3MF::CImplicitFunction & func,
                         nodes::Model & model);
        
        BoundingBox computeBoundingBox(Lib3MF::PMeshObject mesh);

        Lib3MF::PWrapper m_wrapper{};

        MeshResources m_meshObjects;

        events::SharedLogger m_eventLogger;

        NodeMaps m_nodeMaps;
    };

    void loadFrom3mfFile(std::filesystem::path filename, Document & doc);
    void mergeFrom3mfFile(std::filesystem::path filename, Document & doc);
}
