#pragma once

#include "BitmapChannel.h"
#include "Mesh.h"
#include "compute/ComputeCore.h"
#include "io/3mf/Importer3mf.h"
#include "nodes/Assembly.h"
#include "nodes/BuildItem.h"
#include "nodes/Model.h"
#include "ui/GLView.h"

#include <atomic>
#include <filesystem>

namespace gladius
{
    namespace vdb
    {
        struct TriangleMesh;
    }
    class ParameterNotFoundException : public std::exception
    {
      public:
        const char * what() const noexcept override
        {
            return "Parameter could not be found";
        }
    };

    class ParameterAndModelNotFound : public ParameterNotFoundException
    {
      public:
        char const * what() const noexcept override
        {
            return "Parameter could not be found. The model name does not match a model in "
                   "this assembly";
        }
    };

    class ParameterAndNodeNotFound : public ParameterNotFoundException
    {
      public:
        char const * what() const noexcept override
        {
            return "Parameter could not be found. The node name does not match to a node in "
                   "this model.";
        }
    };

    class ParameterCouldNotBeConvertedToFloat : public ParameterNotFoundException
    {
      public:
        char const * what() const noexcept override
        {
            return "The parameter could not be converted to a float";
        }
    };

    class ParameterCouldNotBeConvertedToVector : public ParameterNotFoundException
    {
      public:
        char const * what() const noexcept override
        {
            return "The parameter could not be converted to a Vector";
        }
    };

    class ParameterCouldNotBeConvertedToString : public ParameterNotFoundException
    {
      public:
        char const * what() const noexcept override
        {
            return "The parameter could not be converted to a double";
        }
    };

    class Document
    {
      public:
        void resetGeneratorContext();
        explicit Document(std::shared_ptr<ComputeCore> core);
        [[nodiscard]] bool refreshModelIfNoCompilationIsRunning();

        void load(std::filesystem::path filename);
        void loadNonBlocking(std::filesystem::path filename);
        void merge(std::filesystem::path filename);
        void saveAs(std::filesystem::path filename);

        void newModel();
        void newEmptyModel();
        void newFromTemplate();

        void updateParameter();
        void updateParameterRegistration();
        void updatePayload();
        void refreshModelBlocking();

        void exportAsStl(std::filesystem::path const & filename);

        void markFileAsChanged();
        void invalidatePrimitiveData();
        nodes::SharedAssembly getAssembly() const;

        float getFloatParameter(ResourceId modelId,
                                std::string const & nodeName,
                                std::string const & parameterName);

        void setFloatParameter(ResourceId modelId,
                               std::string const & nodeName,
                               std::string const & parameterName,
                               float value);

        std::string & getStringParameter(ResourceId modelId,
                                         std::string const & nodeName,
                                         std::string const & parameterName);

        void setStringParameter(ResourceId modelId,
                                std::string const & nodeName,
                                std::string const & parameterName,
                                std::string const & value);

        nodes::float3 & getVector3fParameter(ResourceId modelId,
                                             std::string const & nodeName,
                                             std::string const & parameterName);

        void setVector3fParameter(ResourceId modelId,
                                  std::string const & nodeName,
                                  std::string const & parameterName,
                                  nodes::float3 const & value);

        [[nodiscard]] PolyLines generateContour(float z,
                                                nodes::SliceParameter const & sliceParameter) const;

        [[nodiscard]] BoundingBox computeBoundingBox() const;

        [[nodiscard]] Mesh generateMesh() const;

        [[nodiscard]] BitmapChannels & getBitmapChannels();

        [[nodiscard]] nodes::GeneratorContext & getGeneratorContext();

        [[nodiscard]] ComputeContext & getComputeContext() const;

        [[nodiscard]] events::SharedLogger getSharedLogger() const;

        [[nodiscard]] std::shared_ptr<ComputeCore> getCore();

        void set3mfModel(Lib3MF::PModel model);

        [[nodiscard]] Lib3MF::PModel get3mfModel() const;

        nodes::Model & createNewFunction();

        void injectSmoothingKernel(std::string const & kernel);

        nodes::BuildItems::iterator addBuildItem(nodes::BuildItem && item);

        [[nodiscard]] nodes::BuildItems const & getBuildItems() const;

        void replaceMeshResource(ResourceKey const & key, SharedMesh mesh);

        std::optional<ResourceKey> addMeshResource(std::filesystem::path const & filename);
        ResourceKey addMeshResource(vdb::TriangleMesh && mesh, std::string const & name);

        void deleteResource(ResourceId id);

        void deleteFunction(ResourceId id);

        ResourceManager & getResourceManager();

        void addBoundingBoxAsMesh();

      private:
        [[nodiscard]] nodes::VariantParameter &
        findParameterOrThrow(ResourceId modelId,
                             std::string const & nodeName,
                             std::string const & parameterName);

        void loadImpl(const std::filesystem::path & filename);
        void mergeImpl(const std::filesystem::path & filename);
        void refreshModelAsync();
        void loadAllMeshResources();
        void refreshWorker();
        void updateFlatAssembly();

        void updateMemoryOffsets();

        std::unique_ptr<nodes::GeneratorContext> m_generatorContext;
        nodes::SharedAssembly m_assembly;
        nodes::SharedAssembly m_flatAssembly;

        std::filesystem::path m_modelFileName;
        std::optional<std::filesystem::path> m_currentAssemblyFileName;
        std::shared_ptr<ComputeCore> m_core;
        bool m_fileChanged{false};
        std::atomic<bool> m_parameterDirty{false};
        std::atomic<bool> m_contoursDirty{false};

        bool m_primitiveDateNeedsUpdate{true};

        BitmapChannels m_channels;

        Lib3MF::PModel m_3mfmodel;

        std::future<void> m_futureMeshLoading;

        nodes::BuildItems m_buildItems;
    };

    using SharedDocument = std::shared_ptr<Document>;
}
