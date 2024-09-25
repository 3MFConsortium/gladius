#pragma once

#include <BitmapLayer.h>
#include <ContourExtractor.h>
#include <EventLogger.h>
#include <GLImageBuffer.h>
#include <ImageRGBA.h>
#include <ModelState.h>
#include <RenderProgram.h>
#include <ResourceContext.h>
#include <SlicerProgram.h>
#include <nodes/BuildParameter.h>
#include <nodes/Model.h>
#include <nodes/nodesfwd.h>
#include <ui/OrbitalCamera.h>
#include <compute/types.h>

#include <mutex>

namespace gladius
{
    class ProgramManager
    {
      public:
        explicit ProgramManager(SharedComputeContext context,
                                RequiredCapabilities requiredCapabilities,
                                events::SharedLogger logger,
                                SharedResources resources);

        void init();
        void reset();

        ComputeToken waitForComputeToken();
        OptionalComputeToken requestComputeToken();

        [[nodiscard]] SlicerProgram * getSlicerProgram() const;

        [[nodiscard]] RenderProgram * getRenderProgram() const;

        [[nodiscard]] bool isAnyCompilationInProgress() const;

        [[nodiscard]] ComputeContext & getComputeContext() const;

        void compileSlicerProgramBlocking();

        void logMsg(std::string msg) const;

        void recompileIfRequired();
        void recompileBlockingNoLock();

        void setComputeContext(std::shared_ptr<ComputeContext> context);

        [[nodiscard]] events::SharedLogger getSharedLogger() const;

        [[nodiscard]] CodeGenerator getCodeGenerator() const;
        void setCodeGenerator(CodeGenerator generator);

        void setModelSource(std::string source);

        ModelState const & getSlicerState();
        ModelState const & getRendererState();

      private:
        void compileSlicerProgram();
        void compileRenderProgram();

        void throwIfNoOpenGL() const;
        [[nodiscard]] bool isVdbRequired() const;
        [[nodiscard]] events::Logger & getLogger() const;

        void reinitIfNecssary();

        mutable std::recursive_mutex m_computeMutex; // TODO: replace with std::mutex

        SharedComputeContext m_ComputeContext;
        SharedResources m_resources;

        std::unique_ptr<SlicerProgram> m_slicerProgram;

        std::unique_ptr<RenderProgram> m_optimizedRenderProgram;

        bool m_isComputationTimeLoggingEnabled = false;

        RequiredCapabilities m_capabilities = RequiredCapabilities::OpenGLInterop;
        events::SharedLogger m_eventLogger;

        ModelState m_renderState;

        ModelState m_slicerState;
        CodeGenerator m_codeGenerator = CodeGenerator::Code;

        bool m_enableVdb = true;

        std::mutex m_modelSourceMutex;
        std::string m_modelSource;
    };
}
