#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include <fmt/core.h>
#include <lodepng.h>

#include "CliReader.h"
#include "Contour.h"
#include "Mesh.h"
#include "Profiling.h"
#include "ProgramManager.h"
#include "RenderProgram.h"
#include "ResourceContext.h"
#include "SlicerProgram.h"
#include "compute/ProgramManager.h"
#include "gpgpu.h"
#include "nodes/GraphFlattener.h"
#include "nodes/OptimizeOutputs.h"
#include <ToCommandStreamVisitor.h>
#include <ToOCLVisitor.h>

namespace gladius
{
    ProgramManager::ProgramManager(SharedComputeContext context,
                                   RequiredCapabilities requiredCapabilities,
                                   events::SharedLogger logger,
                                   SharedResources resources)
        : m_ComputeContext(context)
        , m_resources(resources)
        , m_capabilities(requiredCapabilities)
        , m_eventLogger(logger)

    {
        init();
    }

    void ProgramManager::init()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        m_slicerProgram = std::make_unique<SlicerProgram>(m_ComputeContext, m_resources);
        m_optimizedRenderProgram = std::make_unique<RenderProgram>(m_ComputeContext, m_resources);

        // Propagate logger to programs so that CL diagnostics go to the event logger
        if (m_eventLogger)
        {
            m_slicerProgram->setLogger(m_eventLogger);
            m_optimizedRenderProgram->setLogger(m_eventLogger);
        }

        m_optimizedRenderProgram->buildKernelLib();
        recompileIfRequired();
        LOG_LOCATION
    }

    void ProgramManager::reset()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        m_renderState.signalCompilationRequired();
        m_slicerState.signalCompilationRequired();
    }

    ComputeToken ProgramManager::waitForComputeToken()
    {
        return ComputeToken(m_computeMutex);
    }

    OptionalComputeToken ProgramManager::requestComputeToken()
    {
        if (!m_computeMutex.try_lock())
        {
            return {};
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);
        return OptionalComputeToken(m_computeMutex);
    }

    void ProgramManager::compileSlicerProgram()
    {
        ProfileFunction std::lock_guard<std::mutex> lock(m_modelSourceMutex);
        std::lock_guard<std::recursive_mutex> lockCompute(m_computeMutex);

        if (!m_slicerProgram->isCompilationInProgress())
        {
            m_slicerProgram->setEnableVdb(m_enableVdb);
            m_slicerProgram->setModelKernel(m_modelSource);
            m_slicerProgram->recompileNonBlocking();
            m_slicerState.signalCompilationStarted();
        }
    }

    void ProgramManager::compileRenderProgram()
    {
        ProfileFunction std::lock_guard<std::mutex> lock(m_modelSourceMutex);
        std::lock_guard<std::recursive_mutex> lockCompute(m_computeMutex);
        LOG_LOCATION
        if (!m_optimizedRenderProgram->isCompilationInProgress())
        {
            LOG_LOCATION
            m_optimizedRenderProgram->setEnableVdb(m_enableVdb);
            m_optimizedRenderProgram->setModelKernel(m_modelSource);
            m_optimizedRenderProgram->recompileNonBlocking();
            m_renderState.signalCompilationStarted();
        }
    }

    void ProgramManager::recompileIfRequired()
    {
        ProfileFunction;
        LOG_LOCATION
        if (!m_optimizedRenderProgram->isCompilationInProgress())
        {
            m_renderState.signalCompilationFinished();
        }

        if (!m_slicerProgram->isCompilationInProgress())
        {
            m_slicerState.signalCompilationFinished();
        }

        if (!m_renderState.isCompilationRequired())
        {
            return;
        }

        logMsg("starting compilation of optimized program");
        compileRenderProgram();

        compileSlicerProgram();
    }

    void ProgramManager::recompileBlockingNoLock()
    {
        ProfileFunction

          m_optimizedRenderProgram->setModelKernel(m_modelSource);
        m_slicerProgram->setModelKernel(m_modelSource);

        m_optimizedRenderProgram->recompileBlocking();
        m_slicerProgram->recompileBlocking();
        m_renderState.signalCompilationFinished();

        m_slicerState.signalCompilationFinished();
    }

    void ProgramManager::setComputeContext(std::shared_ptr<ComputeContext> context)
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lockCompute(m_computeMutex);

        m_ComputeContext = std::move(context);
        reset();
        init();
    }

    void ProgramManager::throwIfNoOpenGL() const
    {
        if (m_capabilities == RequiredCapabilities::ComputeOnly)
        {
            throw std::runtime_error("Operation requires OpenGL which is not available");
        }
    }

    bool ProgramManager::isVdbRequired() const
    {
        ProfileFunction return m_enableVdb;
    }

    [[nodiscard]] bool ProgramManager::isAnyCompilationInProgress() const
    {

        return m_optimizedRenderProgram->isCompilationInProgress() ||
               m_slicerProgram->isCompilationInProgress();
    }

    ComputeContext & ProgramManager::getComputeContext() const
    {
        return *m_ComputeContext;
    }

    void ProgramManager::compileSlicerProgramBlocking()
    {
        ProfileFunction std::lock_guard<std::mutex> lock(m_modelSourceMutex);
        std::lock_guard<std::recursive_mutex> lockCompute(m_computeMutex);
        m_slicerState.signalCompilationStarted();
        m_slicerProgram->setEnableVdb(isVdbRequired());
        m_slicerProgram->waitForCompilation();
        m_slicerProgram->recompileNonBlocking();
        m_slicerProgram->waitForCompilation();
        m_slicerState.signalCompilationFinished();
    }

    void ProgramManager::logMsg(std::string msg) const
    {
        if (m_eventLogger)
        {
            getLogger().addEvent({msg, events::Severity::Info});
        }
        else
        {
            std::cerr << msg << "\n";
        }
    }

    events::Logger & ProgramManager::getLogger() const
    {
        if (!m_eventLogger)
        {
            throw std::runtime_error("logger is missing");
        }
        return *m_eventLogger;
    }

    void ProgramManager::reinitIfNecssary()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        if (m_ComputeContext->isValid())
        {
            return;
        }
        m_eventLogger->addEvent({"Reinitializing compute context"});

        reset();
        init();
    }

    SlicerProgram * ProgramManager::getSlicerProgram() const
    {
        return m_slicerProgram.get();
    }

    RenderProgram * ProgramManager::getRenderProgram() const
    {
        return m_optimizedRenderProgram.get();
    }

    events::SharedLogger ProgramManager::getSharedLogger() const
    {
        return m_eventLogger;
    }

    CodeGenerator ProgramManager::getCodeGenerator() const
    {
        return m_codeGenerator;
    }

    void ProgramManager::setCodeGenerator(CodeGenerator generator)
    {
        m_codeGenerator = generator;
    }

    void ProgramManager::setModelSource(std::string source)
    {
        std::lock_guard<std::mutex> lock(m_modelSourceMutex);
        m_modelSource = std::move(source);
        m_slicerState.signalCompilationRequired();
        m_renderState.signalCompilationRequired();
    }

    ModelState const & ProgramManager::getSlicerState()
    {
        return m_slicerState;
    }

    ModelState const & ProgramManager::getRendererState()
    {
        return m_renderState;
    }
}
