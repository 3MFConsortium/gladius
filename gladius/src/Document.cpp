#include "Document.h"

#include "BackupManager.h"
#include "CliReader.h"
#include "CliWriter.h"
#include "FileChooser.h"
#include "FileSystemUtils.h"
#include "MeshExporter.h"
#include "ResourceManager.h"
#include "TimeMeasurement.h"
#include "compute/ComputeCore.h"
#include "exceptions.h"
#include "imguinodeeditor.h"
#include "io/3mf/ImageExtractor.h"
#include "io/3mf/ImageStackCreator.h"
#include "io/3mf/ResourceDependencyGraph.h"
#include "io/3mf/ResourceIdUtil.h" // for resourceIdToUniqueResourceId
#include "io/3mf/Writer3mf.h"
#include "io/ImporterVdb.h"
#include "io/VdbImporter.h"
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

    AssemblyToken Document::waitForAssemblyToken() const
    {
        return AssemblyToken(m_assemblyMutex);
    }

    OptionalAssemblyToken Document::requestAssemblyToken() const
    {
        if (!m_assemblyMutex.try_lock())
        {
            return {};
        }
        std::lock_guard<std::mutex> lock(m_assemblyMutex, std::adopt_lock);
        return OptionalAssemblyToken(m_assemblyMutex);
    }

    void Document::resetGeneratorContext()
    {

        if (!m_assembly || !m_core)
        {
            throw std::runtime_error("No assembly or core");
        }
        // Get the resource context directly as a shared pointer
        auto resourceContextPtr = m_core->getResourceContext();
        m_generatorContext = std::make_unique<nodes::GeneratorContext>(
          resourceContextPtr, m_assembly->getFilename().remove_filename());
        m_primitiveDateNeedsUpdate = true;
    }

    Document::Document(std::shared_ptr<ComputeCore> core)
        : m_core(std::move(core))
    {
        m_channels.push_back(
          BitmapChannel{"DownSkin",
                        [&](float z_mm, Vector2 pixelSize_mm)
                        { return m_core->generateDownSkinMap(z_mm, std::move(pixelSize_mm)); }});

        m_channels.push_back(
          BitmapChannel{"UpSkin",
                        [&](float z_mm, Vector2 pixelSize_mm)
                        { return m_core->generateUpSkinMap(z_mm, std::move(pixelSize_mm)); }});

        newModel();
        resetGeneratorContext();

        // Initialize backup manager
        m_backupManager.initialize();
    }

    void Document::refreshModelAsync()
    {
        if (!m_assembly || !m_core)
        {
            return;
        }
        saveBackup();
        {
            m_futureModelRefresh = std::async(std::launch::async, [&]() { refreshWorker(); });
        }
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
        auto computeToken = m_core->waitForComputeToken();

        auto meshResourceState = m_core->getMeshResourceState();
        meshResourceState->signalCompilationStarted();

        m_assembly->updateInputsAndOutputs();

        loadAllMeshResources();

        updateParameterRegistration();
        updateParameter();
        m_parameterDirty = true;
        m_contoursDirty = true;

        // Rebuild resource dependency graph
        rebuildResourceDependencyGraph();
        updateFlatAssembly();

        m_core->refreshProgram(m_flatAssembly);
        m_core->recompileBlockingNoLock();
        m_core->invalidatePreCompSdf();
        m_core->resetBoundingBox();
        if (m_core->precomputeSdfForWholeBuildPlatform())
        {
            meshResourceState->signalCompilationFinished();
        }
    }

    void Document::updateFlatAssembly()
    {
        ProfileFunction;
        using namespace gladius::events;

        nodes::Assembly assemblyToFlat;
        {

            if (!m_assembly)
            {
                return;
            }

            if (!validateAssembly())
            {
                return;
            }

            assemblyToFlat = *m_assembly;
        }

        nodes::OptimizeOutputs optimizer{&assemblyToFlat};
        optimizer.optimize();

        // Pass the dependency graph to the flattener if available
        nodes::GraphFlattener flattener =
          m_resourceDependencyGraph
            ? nodes::GraphFlattener(assemblyToFlat, m_resourceDependencyGraph.get())
            : nodes::GraphFlattener(assemblyToFlat);

        try
        {
            m_flatAssembly = std::make_shared<nodes::Assembly>(flattener.flatten());
        }
        catch (std::exception const & e)
        {
            auto logger = getSharedLogger();
            if (logger)
                logger->addEvent(
                  {"Error flattening assembly: " + std::string(e.what()), Severity::Error});
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

    void Document::saveBackup()
    {
        // Only create backups when UI mode is active
        if (!m_uiMode)
        {
            return;
        }

        // Use the new BackupManager for improved backup handling
        try
        {
            // Create a temporary file for the backup
            auto tempDir = std::filesystem::temp_directory_path();
            auto tempBackupFile = tempDir / "gladius_temp_backup.3mf";

            // Preserve the original filename before saving backup
            auto originalFilename = m_currentAssemblyFileName;

            // Save current model to temporary file
            saveAs(tempBackupFile, false);

            // Restore the original filename (backup shouldn't change current file)
            m_currentAssemblyFileName = originalFilename;

            // Get original filename for backup naming
            std::string originalName = "untitled";
            if (originalFilename.has_value() && !originalFilename->empty())
            {
                originalName = originalFilename->stem().string();
            }

            // Create backup using BackupManager
            m_backupManager.createBackup(tempBackupFile, originalName);

            // Clean up temporary file
            if (std::filesystem::exists(tempBackupFile))
            {
                std::filesystem::remove(tempBackupFile);
            }

            // Update backup time
            m_lastBackupTime = std::chrono::system_clock::now();
        }
        catch (const std::exception &)
        {
            // Backup failure shouldn't crash the application
            // Could log error here if logging is available
        }
    }

    bool Document::refreshModelIfNoCompilationIsRunning()
    {
        ProfileFunction;
        if (m_core->getBestRenderProgram()->isCompilationInProgress() ||
            m_core->getSlicerProgram()->isCompilationInProgress() ||
            !m_core->getMeshResourceState()->isModelUpToDate())
        {
            return false;
        }

        refreshModelAsync();

        return true;
    }

    void Document::newModel()
    {
        ProfileFunction;
        {

            m_assembly = std::make_shared<nodes::Assembly>();
            m_assembly->assemblyModel()->createValidVoid();
        }
        m_modelFileName.clear();
        m_3mfmodel.reset();

        io::Importer3mf importer{getSharedLogger()};
        m_3mfmodel = importer.get3mfWrapper()->CreateModel();

        m_core->getResourceContext()->clearImageStacks();
        resetGeneratorContext();
    }

    void Document::newEmptyModel()
    {
        ProfileFunction;
        {

            m_assembly = std::make_shared<nodes::Assembly>();
            m_assembly->assemblyModel()->createBeginEndWithDefaultInAndOuts();
        }
        m_modelFileName.clear();
        m_3mfmodel.reset();

        io::Importer3mf importer{getSharedLogger()};
        m_3mfmodel = importer.get3mfWrapper()->CreateModel();

        m_core->getResourceContext()->clearImageStacks();
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

        {
            m_parameterDirty = m_core->tryToupdateParameter(*m_assembly);
        }

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
            // Get the primitives directly as a shared_ptr
            m_generatorContext->primitives = m_core->getPrimitives();
            if (!m_generatorContext->primitives)
            {
                return;
            }

            {

                if (!m_assembly)
                {
                    return;
                }

                m_generatorContext->basePath = m_assembly->getFilename().remove_filename();
            }
            // Get the compute context directly as a shared_ptr
            m_generatorContext->computeContext = m_core->getComputeContext();
            if (!m_generatorContext->computeContext)
            {
                return;
            }

            CL_ERROR(m_core->getComputeContext()->GetQueue().finish());

            updateMemoryOffsets(); // determines which resources are needed            if
                                   // (m_primitiveDateNeedsUpdate)
            {
                if (m_generatorContext->primitives)
                {
                    m_generatorContext->primitives->clear();
                }

                updateParameterRegistration();

                {

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
            auto logger = getSharedLogger();
            if (logger)
                logger->addEvent(
                  {std::string("unhandled exception: ") + e.what(), events::Severity::Error});
        }
    }

    void Document::refreshModelBlocking()
    {
        ProfileFunction;
        m_core->getSlicerProgram()->waitForCompilation();
        {

            refreshWorker();
        }
        try
        {
            if (m_futureModelRefresh.valid())
            {
                m_futureModelRefresh.get(); // wait for the future to finish
            }
        }
        catch (std::future_error const & e)
        {
            auto logger = getSharedLogger();
            if (logger)
                logger->addEvent(
                  {std::string("future error: ") + e.what(), events::Severity::Error});
        }
        m_core->compileSlicerProgramBlocking();
        updateParameter();

        saveBackup();
    }

    void Document::exportAsStl(std::filesystem::path const & filename)
    {
        refreshModelBlocking();

        vdb::MeshExporter exporter;
        exporter.beginExport(filename, *m_core);
        auto logger = getSharedLogger();
        while (exporter.advanceExport(*m_core))
        {
            if (logger)
                logger->addEvent(
                  {fmt::format("Processing layer with z = {}", m_core->getSliceHeight()),
                   events::Severity::Info});
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
        // reset back up time
        m_lastBackupTime = std::chrono::system_clock::now();
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

    void Document::saveAs(std::filesystem::path filename, bool writeThumbnail)
    {
        if (filename.extension() == ".3mf")
        {
            // Only acquire a compute token if we need to render a thumbnail.
            if (writeThumbnail)
            {
                auto computeToken = m_core->waitForComputeToken();
                (void) computeToken; // suppress unused warning in Release
            }
            io::saveTo3mfFile(filename, *this, writeThumbnail);
        }

        m_fileChanged = false;
        m_currentAssemblyFileName = filename;

        {

            m_assembly->setFilename(filename);
        }
    }

    nodes::SharedAssembly Document::getAssembly() const
    {

        return m_assembly;
    }

    std::optional<std::filesystem::path> Document::getCurrentAssemblyFilename() const
    {
        return m_currentAssemblyFileName;
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
        if (z != m_core->getSliceHeight())
        {
            m_core->setSliceHeight(z);
            m_core->requestContourUpdate(sliceParameter);
        }

        auto contourExtractor = m_core->getContour();
        PolyLines contours = contourExtractor->getContour();
        if (sliceParameter.offset != 0.f)
        {
            return contourExtractor->generateOffsetContours(sliceParameter.offset, contours);
        }
        return contours;
    }

    BoundingBox Document::computeBoundingBox() const
    {

        if (!m_core->updateBBox())
        {
            return {};
        }
        m_core->getResourceContext()->releasePreComputedSdf(); // saving memory (api usage)
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

    SharedComputeContext Document::getComputeContext() const
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

        std::lock_guard<std::mutex> lock(m_assemblyMutex);
        m_assembly->addModelIfNotExisting(modelId);
        auto & model = *m_assembly->getFunctions().at(modelId);
        model.createBeginEnd();
        return model;
    }

    nodes::Model & Document::createLevelsetFunction(std::string const & name)
    {
        if (!m_3mfmodel)
        {
            throw std::runtime_error("No 3mf model loaded");
        }

        auto const new3mfFunc = m_3mfmodel->AddImplicitFunction();
        auto const modelId = new3mfFunc->GetModelResourceID();

        std::lock_guard<std::mutex> lock(m_assemblyMutex);
        m_assembly->addModelIfNotExisting(modelId);
        auto & model = *m_assembly->getFunctions().at(modelId);

        // Create begin and end nodes
        model.createBeginEnd();
        model.setDisplayName(name);

        // Add pos vector input to begin node
        model.getBeginNode()->addOutputPort(nodes::FieldNames::Pos,
                                            nodes::ParameterTypeIndex::Float3);
        model.registerOutputs(*model.getBeginNode());

        // Add color vector output and shape scalar output to end node
        model.getEndNode()->parameter()[nodes::FieldNames::Color] =
          nodes::VariantParameter(nodes::float3{0.5f, 0.5f, 0.5f});
        model.getEndNode()->parameter()[nodes::FieldNames::Shape] =
          nodes::VariantParameter(float{-1.f});

        model.registerInputs(*model.getEndNode());
        model.getBeginNode()->updateNodeIds();
        model.getEndNode()->updateNodeIds();

        return model;
    }

    nodes::Model & Document::copyFunction(nodes::Model const & sourceModel,
                                          std::string const & name)
    {
        if (!m_3mfmodel)
        {
            throw std::runtime_error("No 3mf model loaded");
        }

        auto const new3mfFunc = m_3mfmodel->AddImplicitFunction();
        auto const modelId = new3mfFunc->GetModelResourceID();

        std::lock_guard<std::mutex> lock(m_assemblyMutex);
        m_assembly->addModelIfNotExisting(modelId);
        auto & model = *m_assembly->getFunctions().at(modelId);

        // Copy the source model
        model = sourceModel;
        model.setDisplayName(name);
        model.setResourceId(modelId);

        return model;
    }

    nodes::Model & Document::wrapExistingFunction(nodes::Model & sourceModel,
                                                  std::string const & name)
    {
        if (!m_3mfmodel)
        {
            throw std::runtime_error("No 3mf model loaded");
        }

        auto const new3mfFunc = m_3mfmodel->AddImplicitFunction();
        auto const modelId = new3mfFunc->GetModelResourceID();

        std::lock_guard<std::mutex> lock(m_assemblyMutex);
        m_assembly->addModelIfNotExisting(modelId);
        auto & model = *m_assembly->getFunctions().at(modelId);

        // Create begin and end nodes with same inputs and outputs as source
        model.createBeginEnd();
        model.setDisplayName(name);

        // Copy inputs from source model
        auto const & sourceInputs = sourceModel.getInputs();
        for (auto const & [inputName, inputPort] : sourceInputs)
        {
            model.getBeginNode()->addOutputPort(inputName, inputPort.getTypeIndex());
        }
        model.registerOutputs(*model.getBeginNode());

        // Copy outputs from source model
        auto const & sourceOutputs = sourceModel.getOutputs();
        for (auto const & [outputName, outputPort] : sourceOutputs)
        {
            model.getEndNode()->parameter()[outputName] =
              nodes::createVariantTypeFromTypeIndex(outputPort.getTypeIndex());
        }
        model.registerInputs(*model.getEndNode());

        // Create Resource node for the source function
        auto resourceNode = model.create<nodes::Resource>();
        resourceNode->parameter().at(nodes::FieldNames::ResourceId) =
          nodes::VariantParameter(sourceModel.getResourceId());

        // Create FunctionCall node
        auto functionCallNode = model.create<nodes::FunctionCall>();
        functionCallNode->parameter()
          .at(nodes::FieldNames::FunctionId)
          .setInputFromPort(resourceNode->getOutputs().at(nodes::FieldNames::Value));

        // Set display name to source function name
        auto sourceFunctionName = sourceModel.getDisplayName();
        if (sourceFunctionName.has_value())
        {
            functionCallNode->setDisplayName(sourceFunctionName.value());
        }

        // Update the function call node's inputs and outputs to match the source model
        functionCallNode->updateInputsAndOutputs(sourceModel);
        model.registerInputs(*functionCallNode);
        model.registerOutputs(*functionCallNode);

        // Connect begin node outputs to function call inputs
        for (auto const & [inputName, inputPort] : sourceInputs)
        {
            auto beginOutputIter = model.getBeginNode()->getOutputs().find(inputName);
            auto functionInputIter = functionCallNode->parameter().find(inputName);

            if (beginOutputIter != model.getBeginNode()->getOutputs().end() &&
                functionInputIter != functionCallNode->parameter().end())
            {
                functionInputIter->second.setInputFromPort(beginOutputIter->second);
            }
        }

        // Connect function call outputs to end node inputs
        for (auto const & [outputName, outputPort] : sourceOutputs)
        {
            auto functionOutputIter = functionCallNode->getOutputs().find(outputName);
            auto endInputIter = model.getEndNode()->parameter().find(outputName);

            if (functionOutputIter != functionCallNode->getOutputs().end() &&
                endInputIter != model.getEndNode()->parameter().end())
            {
                endInputIter->second.setInputFromPort(functionOutputIter->second);
            }
        }

        model.getBeginNode()->updateNodeIds();
        model.getEndNode()->updateNodeIds();

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
        auto computeToken = m_core->waitForComputeToken();
        m_buildItems.clear();
        // clear event logger
        auto logger = getSharedLogger();
        if (logger)
        {
            logger->clear();
        }
        resetGeneratorContext();
        m_core->reset();
        m_core->getResourceContext()->clearImageStacks();
        auto newFilename = filename;
        m_primitiveDateNeedsUpdate = true;

        {

            m_assembly->setFilename(filename);
        }

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
            {

                m_assembly = {};
            }

            try
            {
                io::loadFrom3mfFile(filename, *this);
            }
            catch (std::exception const & e)
            {
                auto logger = getSharedLogger();
                if (logger)
                    logger->addEvent(
                      {std::string("unhandled exception: ") + e.what(), events::Severity::Error});
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

    void Document::clearBuildItems()
    {
        m_buildItems.clear();

        m_assembly->assemblyModel()->clear();
        m_assembly->assemblyModel()->createBeginEndWithDefaultInAndOuts();
        m_assembly->assemblyModel()->setManaged(true);
    }

    void Document::replaceMeshResource(ResourceKey const & key, SharedMesh mesh)
    {
        // auto * res = getGeneratorContext().resourceManager.getResourcePtr(key);
        // TODO: Implement
    }

    std::optional<ResourceKey> Document::addMeshResource(std::filesystem::path const & filename)
    {
        vdb::VdbImporter reader;
        auto logger = getSharedLogger();
        try
        {
            reader.loadStl(filename);
            /* code */
        }
        catch (const std::exception & e)
        {
            auto logger = getSharedLogger();
            if (logger)
                logger->addEvent(
                  {std::string("STL load error: ") + e.what(), events::Severity::Error});
            return {};
        }

        auto mesh = reader.getMesh();
        return addMeshResource(std::move(mesh), filename.filename().string());
    }

    ResourceKey Document::addMeshResource(vdb::TriangleMesh && mesh, std::string const & name)
    {
        if (!m_3mfmodel)
        {
            throw std::runtime_error("No 3mf model loaded");
        }

        auto const new3mfMesh = m_3mfmodel->AddMeshObject();
        new3mfMesh->SetName(name);

        for (auto & vertex : mesh.vertices)
        {
            new3mfMesh->AddVertex({vertex.x(), vertex.y(), vertex.z()});
        }

        for (auto & triangle : mesh.indices)
        {
            new3mfMesh->AddTriangle({triangle[0], triangle[1], triangle[2]});
        }

        auto & resourceManager = getGeneratorContext().resourceManager;

        ResourceKey key = ResourceKey(new3mfMesh->GetModelResourceID());
        key.setDisplayName(name);
        resourceManager.addResource(key, std::move(mesh));

        resourceManager.loadResources();

        return key;
    }

    void Document::deleteResource(ResourceId id)
    {
        {

            m_assembly->deleteModel(id); // will just return if not a function
        }

        if (m_3mfmodel)
        {
            // NOTE: Keep in mind that id is a ModelResourceID, no a UniqueResourceID
            auto resIter = m_3mfmodel->GetResources();
            while (resIter->MoveNext())
            {
                auto resource = resIter->GetCurrent();
                if (resource->GetModelResourceID() == id)
                {
                    m_3mfmodel->RemoveResource(resource);
                    break;
                }
            }
        }
    }

    void Document::deleteResource(ResourceKey key)
    {
        auto id = key.getResourceId();
        if (!id)
        {
            return;
        }
        deleteResource(id.value());

        auto & resourceManager = getGeneratorContext().resourceManager;
        resourceManager.deleteResource(key);
    }

    void Document::deleteFunction(ResourceId id)
    {
        {

            m_assembly->deleteModel(id);
        }

        if (m_3mfmodel)
        {
            // NOTE: Keep in mind that id is a ModelResourceID, no a UniqueResourceID
            auto resIter = m_3mfmodel->GetResources();
            while (resIter->MoveNext())
            {
                auto resource = resIter->GetCurrent();
                if (resource->GetModelResourceID() == id)
                {
                    m_3mfmodel->RemoveResource(resource);
                    break;
                }
            }
        }
    }
    ResourceManager & Document::getResourceManager()
    {
        return getGeneratorContext().resourceManager;
    }

    void Document::addBoundingBoxAsMesh()
    {
        auto const bbox = computeBoundingBox();

        // create mesh from bounding box
        vdb::TriangleMesh mesh;

        // Top
        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.max.z},
                         {bbox.max.x, bbox.min.y, bbox.max.z},
                         {bbox.max.x, bbox.max.y, bbox.max.z});

        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.max.z},
                         {bbox.max.x, bbox.max.y, bbox.max.z},
                         {bbox.min.x, bbox.max.y, bbox.max.z});

        // Bottom
        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.max.y, bbox.min.z});

        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.max.y, bbox.min.z},
                         {bbox.min.x, bbox.max.y, bbox.min.z});

        // Front
        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.min.y, bbox.max.z});

        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.min.y, bbox.max.z},
                         {bbox.min.x, bbox.min.y, bbox.max.z});

        // Back
        mesh.addTriangle({bbox.min.x, bbox.max.y, bbox.min.z},
                         {bbox.max.x, bbox.max.y, bbox.min.z},
                         {bbox.max.x, bbox.max.y, bbox.max.z});

        mesh.addTriangle({bbox.min.x, bbox.max.y, bbox.min.z},
                         {bbox.max.x, bbox.max.y, bbox.max.z},
                         {bbox.min.x, bbox.max.y, bbox.max.z});

        // Left
        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.min.z},
                         {bbox.min.x, bbox.min.y, bbox.max.z},
                         {bbox.min.x, bbox.max.y, bbox.max.z});

        mesh.addTriangle({bbox.min.x, bbox.min.y, bbox.min.z},
                         {bbox.min.x, bbox.max.y, bbox.max.z},
                         {bbox.min.x, bbox.max.y, bbox.min.z});

        // Right
        mesh.addTriangle({bbox.max.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.min.y, bbox.max.z},
                         {bbox.max.x, bbox.max.y, bbox.max.z});

        mesh.addTriangle({bbox.max.x, bbox.min.y, bbox.min.z},
                         {bbox.max.x, bbox.max.y, bbox.max.z},
                         {bbox.max.x, bbox.max.y, bbox.min.z});

        addMeshResource(std::move(mesh), "bounding box");
    }

    ResourceKey Document::addImageStackResource(std::filesystem::path const & path)
    {
        io::ImageStackCreator creator;
        auto stack = creator.addImageStackFromDirectory(get3mfModel(), path);

        if (!stack)
        {
            auto logger = getSharedLogger();
            if (logger)
            {
                logger->addEvent(
                  {fmt::format("Failed to import image stack from directory: {}", path.string()),
                   events::Severity::Error});
            }

            return ResourceKey{0};
        }
        auto & resourceManager = getGeneratorContext().resourceManager;
        auto const key = ResourceKey{stack->GetModelResourceID()};

        io::ImageExtractor extractor;

        auto grid = extractor.loadAsVdbGrid(creator.getFiles(path), io::FileLoaderType::Filesystem);
        resourceManager.addResource(key, std::move(grid));
        resourceManager.loadResources();
        return key;
    }

    void Document::update3mfModel()
    {
        io::Writer3mf writer(getSharedLogger());
        writer.updateModel(*this);
    }

    void Document::updateDocumenFrom3mfModel(bool skipImplicitFunctions)
    {
        if (!m_3mfmodel)
        {
            throw std::runtime_error("No 3MF model available to update the document.");
        }

        io::Importer3mf importer{getSharedLogger()};

        // Load build items from the 3MF model
        clearBuildItems();
        importer.loadBuildItems(m_3mfmodel, *this);

        // Load implicit functions from the 3MF model
        if (!skipImplicitFunctions)
        {
            importer.loadImplicitFunctions(m_3mfmodel, *this);

            m_assembly->updateInputsAndOutputs();
        }

        // Update the assembly inputs and outputs
    }

    void Document::rebuildResourceDependencyGraph()
    {

        if (!m_3mfmodel)
        {
            return;
        }

        m_resourceDependencyGraph =
          std::make_unique<io::ResourceDependencyGraph>(m_3mfmodel, getSharedLogger());
        m_resourceDependencyGraph->buildGraph();
    }

    io::CanResourceBeRemovedResult Document::isItSafeToDeleteResource(ResourceKey key)
    {
        io::CanResourceBeRemovedResult result;
        result.canBeRemoved = true;

        if (!m_3mfmodel)
        {
            return result;
        }

        auto modelResIdOpt = key.getResourceId();
        if (!modelResIdOpt)
        {
            return result;
        }

        // map model ResourceId to UniqueResourceID for graph lookup
        Lib3MF_uint32 uniqueResId =
          io::resourceIdToUniqueResourceId(m_3mfmodel, modelResIdOpt.value());
        try
        {
            auto resource = m_3mfmodel->GetResourceByID(uniqueResId);
            result = m_resourceDependencyGraph->checkResourceRemoval(resource);
            return result;
        }
        catch (const Lib3MF::ELib3MFException & e)
        {
            // Resource not found, return empty result
            auto logger = getSharedLogger();
            if (logger)
            {
                logger->addEvent(
                  {fmt::format("Resource not found: {}", e.what()), events::Severity::Error});
            }
        }
        catch (const std::exception & e)
        {

            // Resource not found, return empty result
            auto logger = getSharedLogger();
            if (logger)
            {
                logger->addEvent(
                  {fmt::format("Exception occurred: {}", e.what()), events::Severity::Error});
            }
            return result;
        }

        return result;
    }

    std::size_t Document::removeUnusedResources()
    {
        if (!m_3mfmodel || !m_resourceDependencyGraph)
        {
            auto logger = getSharedLogger();
            if (logger)
            {
                logger->addEvent({"Cannot remove unused resources: Model or resource dependency "
                                  "graph not available",
                                  events::Severity::Warning});
            }
            return 0;
        }

        // Ensure the resource dependency graph is up-to-date
        rebuildResourceDependencyGraph();

        // Find all unused resources
        std::vector<Lib3MF::PResource> unusedResources =
          m_resourceDependencyGraph->findUnusedResources();

        if (unusedResources.empty())
        {
            auto logger = getSharedLogger();
            if (logger)
            {
                logger->addEvent(
                  {"No unused resources found in the model", events::Severity::Info});
            }
            return 0;
        }

        std::size_t removedCount = 0;
        auto & resourceManager = getGeneratorContext().resourceManager;

        // Remove each unused resource
        for (auto const & resource : unusedResources)
        {
            try
            {
                // Get the model resource ID for this resource
                Lib3MF_uint32 modelResourceId = resource->GetModelResourceID();
                ResourceKey key{modelResourceId};

                // Check if this is actually a function (need to handle differently)
                bool isFunction = false;
                try
                {
                    auto function = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                    if (function)
                    {
                        isFunction = true;
                        deleteFunction(modelResourceId);
                    }
                }
                catch (const std::exception &)
                {
                    // Not a function, continue with normal resource deletion
                }

                if (!isFunction)
                {
                    // Delete from resource manager if it exists there
                    if (resourceManager.hasResource(key))
                    {
                        resourceManager.deleteResource(key);
                    }

                    // Delete the resource from the 3MF model
                    m_3mfmodel->RemoveResource(resource);
                }

                removedCount++;
            }
            catch (const std::exception & e)
            {
                auto logger = getSharedLogger();
                if (logger)
                {
                    logger->addEvent({fmt::format("Failed to remove unused resource: {}", e.what()),
                                      events::Severity::Error});
                }
            }
        }

        if (removedCount > 0)
        {
            auto logger = getSharedLogger();
            if (logger)
            {
                logger->addEvent(
                  {fmt::format("Successfully removed {} unused resources", removedCount),
                   events::Severity::Info});
            }
            markFileAsChanged();

            // Rebuild the dependency graph now that resources have been removed
            rebuildResourceDependencyGraph();
        }

        return removedCount;
    }

    std::vector<Lib3MF::PResource> Document::findUnusedResources()
    {
        if (!m_3mfmodel || !m_resourceDependencyGraph)
        {
            auto logger = getSharedLogger();
            if (logger)
            {
                logger->addEvent(
                  {"Cannot find unused resources: Model or resource dependency graph not available",
                   events::Severity::Warning});
            }
            return {};
        }

        // Ensure the resource dependency graph is up-to-date
        rebuildResourceDependencyGraph();

        // Find all unused resources
        return m_resourceDependencyGraph->findUnusedResources();
    }

    const gladius::io::ResourceDependencyGraph * Document::getResourceDependencyGraph() const
    {
        return m_resourceDependencyGraph.get();
    }

    bool Document::validateAssembly() const
    {
        nodes::Validator validator;
        auto logger = getSharedLogger();

        if (!validator.validate(*m_assembly))
        {
            for (auto const & error : validator.getErrors())
            {
                if (logger)
                    logger->addEvent({fmt::format("{}: Review parameter {} of node {} in model {}",
                                                  error.message,
                                                  error.parameter,
                                                  error.node,
                                                  error.model),
                                      events::Severity::Error});
            }
            return false;
        }

        return true;
    }

    BackupManager & Document::getBackupManager()
    {
        return m_backupManager;
    }

    const BackupManager & Document::getBackupManager() const
    {
        return m_backupManager;
    }

    void Document::setUiMode(bool uiMode)
    {
        m_uiMode = uiMode;
    }

    bool Document::isUiMode() const
    {
        return m_uiMode;
    }
}
