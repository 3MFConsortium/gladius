#include "Document.h"
#include "CliReader.h"
#include "CliWriter.h"
#include "FileChooser.h"
#include "FileSystemUtils.h"
#include "MeshExporter.h"
#include "ResourceManager.h"
#include "TimeMeasurement.h"
#include "compute/ComputeCore.h"
#include "imguinodeeditor.h"
#include "io/3mf/Writer3mf.h"
#include "io/ImporterVdb.h"
#include "nodes/GraphFlattener.h"
#include "nodes/Model.h"
#include "nodes/OptimizeOutputs.h"
#include "nodes/ToCommandStreamVisitor.h"
#include "nodes/ToOCLVisitor.h"
#include "nodes/Validator.h"

#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <utility>

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(gladius_resources);

namespace gladius
{
    using namespace std;

    void Document::resetGeneratorContext()
    {
        if (!m_assembly || !m_core)
        {
            throw std::runtime_error("No assembly or core");
        }

        m_generatorContext = std::make_unique<nodes::GeneratorContext>(
          &m_core->getResourceContext(), m_assembly->getFilename().remove_filename());
        m_primitiveDateNeedsUpdate = true;
    }

    Document::Document(std::shared_ptr<ComputeCore> core)
        : m_core(std::move(core))
    {
        m_channels.push_back(BitmapChannel{"DownSkin", [&](float z_mm, Vector2 pixelSize_mm) {
                                               return m_core->generateDownSkinMap(
                                                 z_mm, std::move(pixelSize_mm));
                                           }});

        m_channels.push_back(BitmapChannel{"UpSkin", [&](float z_mm, Vector2 pixelSize_mm) {
                                               return m_core->generateUpSkinMap(
                                                 z_mm, std::move(pixelSize_mm));
                                           }});

        newModel();
        resetGeneratorContext();
        refreshModelAsync();
    }

    void Document::refreshModelAsync()
    {
        if (!m_assembly || !m_core)
        {
            return;
        }

        m_futureMeshLoading = std::async(std::launch::async, [&]() { refreshWorker(); });
    }

    void Document::loadAllMeshResources()
    {
        io::Importer3mf importer{getSharedLogger()};

        if (!m_3mfmodel)
        {
            return;
        }

        importer.loadMeshes(m_3mfmodel, *this);
    }

    void Document::refreshWorker()
    {
        ProfileFunction;
        auto computeToken = m_core->requestComputeToken();
        if (!computeToken.has_value())
        {
            m_core->invalidatePreCompSdf();
            return;
        }

        m_core->getMeshResourceState().signalCompilationStarted();

        m_assembly->updateInputsAndOutputs();
        loadAllMeshResources();

        updateParameterRegistration();
        updateParameter();
        m_parameterDirty = true;
        m_contoursDirty = true;

        updateFlatAssembly();
        m_core->refreshProgram(m_flatAssembly);
        m_core->recompileBlockingNoLock();
        m_core->invalidatePreCompSdf();
        m_core->resetBoundingBox();
        m_core->precomputeSdfForWholeBuildPlatform();
        m_core->getMeshResourceState().signalCompilationFinished();
    }

    void Document::updateFlatAssembly()
    {
        using namespace gladius::events;
        nodes::Assembly assemblyToFlat = *m_assembly;
        nodes::OptimizeOutputs optimizer{&assemblyToFlat};
        optimizer.optimize();

        nodes::GraphFlattener flattener(assemblyToFlat);
        nodes::Validator validator;
        auto logger = getSharedLogger();
        if (!validator.validate(*m_assembly))
        {

            if (logger)
            {
                for (auto const & error : validator.getErrors())
                {
                    logger->addEvent({fmt::format("{}: Review parameter {} of node {} in model {}",
                                                  error.message,
                                                  error.parameter,
                                                  error.node,
                                                  error.model),
                                      Severity::Error});
                }
            }
            else
            {
                for (auto const & error : validator.getErrors())
                {
                    std::cerr << fmt::format("{}: Review parameter {} of node {} in model {}\n",
                                             error.message,
                                             error.parameter,
                                             error.node,
                                             error.model);
                }
            }
            return;
        }
        try
        {
            m_flatAssembly = std::make_shared<nodes::Assembly>(flattener.flatten());
        }
        catch (std::exception const & e)
        {
            if (logger)
            {
                logger->addEvent(
                  {"Error flattening assembly: " + std::string(e.what()), Severity::Error});
            }
            else
            {
                std::cerr << "Error flattening assembly: " << e.what() << "\n";
            }
        }
    }

    void Document::updateMemoryOffsets()
    {
        if (!m_generatorContext)
        {
            throw std::runtime_error("No generator context");
        }
        for (auto & model : m_assembly->getFunctions())
        {
            if (!model.second)
            {
                continue;
            }
            for (auto & node : *model.second)
            {
                node.second->updateMemoryOffsets(*m_generatorContext);
            }
        }
    }

    bool Document::refreshModelIfNoCompilationIsRunning()
    {
        ProfileFunction;
        if (m_core->getBestRenderProgram()->isCompilationInProgress() ||
            m_core->getSlicerProgram()->isCompilationInProgress() ||
            !m_core->getMeshResourceState().isModelUpToDate())
        {
            return false;
        }

        refreshModelAsync();
        return true;
    }

    void Document::newModel()
    {
        ProfileFunction;
        m_assembly = std::make_shared<nodes::Assembly>();
        m_assembly->assemblyModel()->createValidVoid();
        m_modelFileName.clear();
        m_3mfmodel.reset();

        io::Importer3mf importer{getSharedLogger()};
        m_3mfmodel = importer.get3mfWrapper()->CreateModel();

        m_core->getResourceContext().clearImageStacks();
        resetGeneratorContext();
    }

    void Document::newEmptyModel()
    {
        ProfileFunction;
        m_assembly = std::make_shared<nodes::Assembly>();
        m_assembly->assemblyModel()->createBeginEndWithDefaultInAndOuts();
        m_modelFileName.clear();
        m_3mfmodel.reset();

        io::Importer3mf importer{getSharedLogger()};
        m_3mfmodel = importer.get3mfWrapper()->CreateModel();

        m_core->getResourceContext().clearImageStacks();
        resetGeneratorContext();
    }

    void Document::newFromTemplate()
    {
        auto templateFiletName = getAppDir() / "examples/template.3mf";
        if (!std::filesystem::exists(templateFiletName))
        {
            newModel();
        }
        loadNonBlocking(templateFiletName);
    }

    void Document::updateParameter()
    {
        ProfileFunction;
        if (!m_assembly || !m_core)
        {
            return;
        }

        updatePayload();

        m_parameterDirty = m_core->tryToupdateParameter(*m_assembly);
        if (m_parameterDirty)
        {
            m_parameterDirty = !m_core->precomputeSdfForWholeBuildPlatform();
        }
        else
        {
            m_parameterDirty = true;
        }
    }

    void Document::updateParameterRegistration()
    {
        if (!m_assembly)
        {
            return;
        }

        for (auto & model : m_assembly->getFunctions())
        {
            if (!model.second)
            {
                continue;
            }
            for (auto & node : *model.second)
            {
                model.second->registerInputs(*node.second);
            }
        }
    }

    void Document::updatePayload()
    {
        ProfileFunction;
        if (!m_generatorContext)
        {
            resetGeneratorContext();
        }
        if (!m_generatorContext)
        {
            throw std::runtime_error("No generator context");
        }
        try
        {
            if (!m_core)
            {
                return;
            }

            auto computeToken = m_core->requestComputeToken();

            if (!m_generatorContext)
            {
                return;
            }

            m_generatorContext->primitives = &m_core->getPrimitives();
            if (!m_generatorContext->primitives)
            {
                return;
            }

            if (!m_assembly)
            {
                return;
            }

            m_generatorContext->basePath = m_assembly->getFilename().remove_filename();

            m_generatorContext->computeContext = &m_core->getComputeContext();
            if (!m_generatorContext->computeContext)
            {
                return;
            }

            CL_ERROR(m_core->getComputeContext().GetQueue().finish());

            updateMemoryOffsets(); // determines which resources are needed

            if (m_primitiveDateNeedsUpdate)
            {
                if (m_generatorContext->primitives)
                {
                    m_generatorContext->primitives->clear();
                }

                updateParameterRegistration();
                for (auto & model : m_assembly->getFunctions())
                {
                    if (!model.second)
                    {
                        continue;
                    }
                    for (auto & node : *model.second)
                    {
                        if (node.second)
                        {
                            node.second->generate(*m_generatorContext);
                        }
                    }
                }

                m_generatorContext->resourceManager.loadResources();
                m_generatorContext->resourceManager.writeResources(*m_generatorContext->primitives);
            }

            // update start and end indices
            updateMemoryOffsets();

            m_primitiveDateNeedsUpdate = false;
        }
        catch (std::exception const & e)
        {
            std::cerr << "unhandled exception: " << e.what() << "\n";
        }
    }

    void Document::refreshModelBlocking()
    {
        ProfileFunction;
        m_core->getSlicerProgram()->waitForCompilation();
        refreshWorker();
        try
        {
            if (m_futureMeshLoading.valid())
            {
                m_futureMeshLoading.get(); // wait for the future to finish
            }
        }
        catch (std::future_error const & e)
        {
            std::cerr << "future error: " << e.what() << "\n";
        }
        m_core->compileSlicerProgramBlocking();
        updateParameter();
    }

    void Document::exportAsStl(std::filesystem::path const & filename)
    {
        refreshModelBlocking();

        vdb::MeshExporter exporter;
        exporter.beginExport(filename, *m_core);
        while (exporter.advanceExport(*m_core))
        {
            std::cout << " Processing layer with z = " << m_core->getSliceHeight() << "\n";
        }
        exporter.finalizeExportSTL(*m_core);
    }

    void Document::markFileAsChanged()
    {
        m_fileChanged = true;
    }

    void Document::invalidatePrimitiveData()
    {
        m_primitiveDateNeedsUpdate = true;
    }

    void Document::load(std::filesystem::path filename)
    {
        loadImpl(filename);
        refreshModelBlocking();
        m_core->updateBBox();
    }

    void Document::loadNonBlocking(std::filesystem::path filename)
    {
        loadImpl(filename);
        refreshModelAsync();
    }

    void Document::merge(std::filesystem::path filename)
    {
        mergeImpl(filename);
        refreshModelAsync();
    }

    void Document::saveAs(std::filesystem::path filename)
    {
        if (filename.extension() == ".3mf")
        {
            io::saveTo3mfFile(filename, *this);
        }

        m_fileChanged = false;
        m_currentAssemblyFileName = filename;
        m_assembly->setFilename(filename);
    }

    nodes::SharedAssembly Document::getAssembly() const
    {
        return m_assembly;
    }

    float Document::getFloatParameter(ResourceId modelId,
                                      std::string const & nodeName,
                                      std::string const & parameterName)
    {
        auto const & parameter = findParameterOrThrow(modelId, nodeName, parameterName);
        auto val = parameter.getValue();
        if (auto * pval = std::get_if<float>(&val))
        {
            return *pval;
        }
        throw ParameterCouldNotBeConvertedToFloat();
    }

    void Document::setFloatParameter(ResourceId modelId,
                                     std::string const & nodeName,
                                     std::string const & parameterName,
                                     float value)
    {
        findParameterOrThrow(modelId, nodeName, parameterName).setValue(value);
    }

    std::string & Document::getStringParameter(ResourceId modelId,
                                               std::string const & nodeName,
                                               std::string const & parameterName)
    {
        auto const & parameter = findParameterOrThrow(modelId, nodeName, parameterName);
        auto val = parameter.getValue();
        if (auto * pval = std::get_if<std::string>(&val))
        {
            return *pval;
        }
        throw ParameterCouldNotBeConvertedToString();
    }

    void Document::setStringParameter(ResourceId modelId,
                                      std::string const & nodeName,
                                      std::string const & parameterName,
                                      std::string const & value)
    {
        findParameterOrThrow(modelId, nodeName, parameterName).setValue(value);
    }

    nodes::float3 & Document::getVector3fParameter(ResourceId modelId,
                                                   std::string const & nodeName,
                                                   std::string const & parameterName)
    {
        auto const & parameter = findParameterOrThrow(modelId, nodeName, parameterName);
        auto val = parameter.getValue();
        if (auto * pval = std::get_if<nodes::float3>(&val))
        {
            return *pval;
        }
        throw ParameterCouldNotBeConvertedToVector();
    }

    void Document::setVector3fParameter(ResourceId modelId,
                                        std::string const & nodeName,
                                        std::string const & parameterName,
                                        nodes::float3 const & value)
    {
        findParameterOrThrow(modelId, nodeName, parameterName).setValue(value);
    }

    PolyLines Document::generateContour(float z, nodes::SliceParameter const & sliceParameter) const
    {
        m_core->setSliceHeight(z);
        m_core->requestContourUpdate(sliceParameter);
        return m_core->getContour().getContour();
    }

    BoundingBox Document::computeBoundingBox() const
    {
        if (!m_core->updateBBox())
        {
            return {};
        }
        m_core->getResourceContext().releasePreComputedSdf(); // saving memory (api usage)
        return m_core->getBoundingBox().value_or(BoundingBox{});
    }

    gladius::Mesh Document::generateMesh() const
    {
        return gladius::vdb::generatePreviewMesh(*m_core, *m_assembly);
    }

    BitmapChannels & Document::getBitmapChannels()
    {
        return m_channels;
    }

    nodes::GeneratorContext & Document::getGeneratorContext()
    {
        return *m_generatorContext;
    }

    ComputeContext & Document::getComputeContext() const
    {
        if (!m_core)
        {
            throw std::runtime_error("No core");
        }
        return m_core->getComputeContext();
    }

    events::SharedLogger Document::getSharedLogger() const
    {
        if (!m_core)
        {
            throw std::runtime_error("No core");
        }
        return m_core->getSharedLogger();
    }

    std::shared_ptr<ComputeCore> Document::getCore()
    {
        return m_core;
    }

    void Document::set3mfModel(Lib3MF::PModel model)
    {
        m_3mfmodel = std::move(model);
    }

    Lib3MF::PModel Document::get3mfModel() const
    {
        return m_3mfmodel;
    }

    nodes::Model & Document::createNewFunction()
    {
        if (!m_3mfmodel)
        {
            throw std::runtime_error("No 3mf model loaded");
        }

        auto const new3mfFunc = m_3mfmodel->AddImplicitFunction();
        auto const modelId = new3mfFunc->GetModelResourceID();
        m_assembly->addModelIfNotExisting(modelId);
        auto & model = *m_assembly->getFunctions().at(modelId);
        model.createBeginEnd();
        return model;
    }

    nodes::VariantParameter & Document::findParameterOrThrow(ResourceId modelId,
                                                             std::string const & nodeName,
                                                             std::string const & parameterName)
    {
        auto const modelIter = m_assembly->getFunctions().find(modelId);
        if (modelIter == std::end(m_assembly->getFunctions()))
        {
            throw ParameterAndModelNotFound();
        }

        auto & model = modelIter->second;
        auto const node = model->findNode(nodeName);
        if (!node)
        {
            throw ParameterAndNodeNotFound();
        }

        auto const parameterIter = node->parameter().find(parameterName);
        if (parameterIter == std::end(node->parameter()))
        {
            throw ParameterNotFoundException();
        }
        return parameterIter->second;
    }

    void Document::loadImpl(const std::filesystem::path & filename)
    {
        auto computeToken = m_core->requestComputeToken();
        m_buildItems.clear();

        resetGeneratorContext();
        m_core->reset();
        m_core->getResourceContext().clearImageStacks();
        auto newFilename = filename;
        m_primitiveDateNeedsUpdate = true;
        m_assembly->setFilename(filename);

        if (filename.extension() == ".vdb")
        {
            newEmptyModel();
            io::loadFromOpenVdbFile(filename, *this);
            return;
        }

        newFilename.replace_extension(".3mf");
        m_currentAssemblyFileName = newFilename;

        if (filename.extension() == ".3mf")
        {
            m_assembly = {};

            try
            {
                io::loadFrom3mfFile(filename, *this);
            }
            catch (std::exception const & e)
            {
                std::cerr << "unhandled exception: " << e.what() << "\n";
                auto logger = getSharedLogger();
                if (logger)
                {
                    logger->addEvent(events::Event{e.what(), events::Severity::Error});
                }
                newModel();
                return;
            }
        }
    }

    void Document::mergeImpl(const std::filesystem::path & filename)
    {
        if (filename.extension() == ".3mf")
        {
            io::mergeFrom3mfFile(filename, *this);
        }
        m_primitiveDateNeedsUpdate = true;
    }

    void Document::injectSmoothingKernel(std::string const & kernel)
    {
        m_core->injectSmoothingKernel(kernel);
    }

    nodes::BuildItems::iterator Document::addBuildItem(nodes::BuildItem && item)
    {
        m_buildItems.emplace_back(std::move(item));
        return std::prev(m_buildItems.end());
    }

    nodes::BuildItems const & Document::getBuildItems() const
    {
        return m_buildItems;
    }

    void Document::replaceMeshResource(ResourceKey const & key, SharedMesh mesh)
    {
        // auto * res = getGeneratorContext().resourceManager.getResourcePtr(key);
        // TODO: Implement
    }
}
