#pragma once

#include "Mesh.h"
#include "nodes/Assembly.h"
#include "nodes/Model.h"

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "EventLogger.h"

#include <lib3mf_implicit.hpp>

namespace gladius::io
{
    /**
     * @brief Structure to represent duplicated implicit functions.
     *
     * This structure holds two implicit functions that are considered duplicates:
     * one from the original function list and one from the extended model.
     */
    struct Duplicates
    {
        Lib3MF::PImplicitFunction originalFunction; ///< Function from the original list
        Lib3MF::PImplicitFunction
          duplicateFunction; ///< Duplicated function from the extended model
    };
}

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

        /**
         * @brief Loads all build items from a 3MF model and adds them to the document.
         *
         * @param model The 3MF model to load the build items from.
         * @param doc The document to add the loaded build items to.
         */
        void loadBuildItems(Lib3MF::PModel model, Document & doc);

        /**
         * @brief Loads all implicit functions from a 3MF model and adds them to the document.
         *
         * @param model The 3MF model to load the implicit functions from.
         * @param doc The document to add the loaded implicit functions to.
         */
        void loadImplicitFunctions(Lib3MF::PModel model, Document & doc);

        /**
         * @brief Loads all implicit functions from a 3MF model and adds them to the document,
         * except for those identified as duplicates.
         *
         * This function is similar to loadImplicitFunctions, but it skips loading any functions
         * that are included in the duplicates list. This is useful for avoiding the creation
         * of duplicate functions when merging 3MF models.
         *
         * @param model The 3MF model to load the implicit functions from.
         * @param doc The document to add the loaded implicit functions to.
         * @param duplicates Vector of Duplicates structs containing functions to be skipped.
         */
        void loadImplicitFunctionsFiltered(Lib3MF::PModel model,
                                           Document & doc,
                                           std::vector<Duplicates> const & duplicates);

        /**
         * @brief Loads all component objects from a 3MF model and adds them to the document.
         *
         * @param model The 3MF model to load the component objects from.
         * @param doc The document to add the loaded component objects to.
         */

        void loadComponentObjects(Lib3MF::PModel model, Document & doc);

        /**
         * @brief Replaces references to duplicated functions in an implicit function graph.
         *
         * This method iterates through all implicit functions in the model. For each function,
         * it identifies nodes of type ConstResourceID that reference a duplicated function,
         * and replaces those references with the original function.
         *
         * @param duplicates Vector of Duplicates structs containing pairs of original and duplicate
         * functions
         * @param model The 3MF model containing functions that may reference duplicate functions
         */
        void replaceDuplicatedFunctionReferences(std::vector<Duplicates> const & duplicates,
                                                 Lib3MF::PModel const & model) const;

      private:
        void logWarnings(std::filesystem::path const & filename, Lib3MF::PReader reader);

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

        /**
         * @brief Loads a beam lattice from a mesh object if present and adds it to the document.
         *
         * @param model The 3MF model containing the mesh object.
         * @param meshObject The mesh object that may contain a beam lattice.
         * @param doc The document to add the loaded beam lattice to.
         */
        void loadBeamLatticeIfNecessary(Lib3MF::PModel model,
                                        Lib3MF::PMeshObject meshObject,
                                        Document & doc);

        void addMeshObject(Lib3MF::PModel model,
                           ResourceKey const & key,
                           Lib3MF::PMeshObject meshObject,
                           nodes::Matrix4x4 const & trafo,
                           Document & doc);

        void addBeamLatticeObject(Lib3MF::PModel model,
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

        void createObject(Lib3MF::CObject & objectRes,
                          Lib3MF::PModel & model,
                          gladius::ResourceKey & key,
                          const gladius::nodes::Matrix4x4 & trafo,
                          gladius::Document & doc);

        void processImplicitFunction(Document & doc, Lib3MF::CImplicitFunction * func);

        void processFunctionFromImage3d(Document & doc, Lib3MF::CFunctionFromImage3D * func);

        /**
         * @brief Collects all function resource IDs from a 3MF model.
         *
         * This function iterates through all resources in the model and identifies
         * those that are either implicit functions or functions from 3D images.
         *
         * @param model The 3MF model to collect function resource IDs from.
         * @return A set containing the resource IDs of all functions in the model.
         */
        std::set<Lib3MF_uint32> collectFunctionResourceIds(Lib3MF::PModel const & model) const;

        /**
         * @brief Collects all implicit functions from a 3MF model.
         *
         * This function iterates through all resources in the model and collects
         * all implicit functions (excluding functions from 3D images).
         *
         * @param model The 3MF model to collect implicit functions from.
         * @return A vector containing shared pointers to the implicit functions in the model.
         */
        std::vector<Lib3MF::PImplicitFunction>
        collectImplicitFunctions(Lib3MF::PModel const & model) const;

        /**
         * @brief Finds duplicated implicit functions between original functions and an extended
         * model.
         *
         * This function compares each original function with functions in the extended model
         * to identify duplicates using the FunctionComparator.
         *
         * @param originalFunctions Vector of implicit functions to check for duplicates
         * @param extendedModel The extended model to search in for duplicates
         * @return Vector of Duplicates structs containing the IDs of duplicated functions
         */
        std::vector<Duplicates>
        findDuplicatedFunctions(std::vector<Lib3MF::PImplicitFunction> const & originalFunctions,
                                Lib3MF::PModel const & extendedModel) const;

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
