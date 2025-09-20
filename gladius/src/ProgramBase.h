#pragma once

#include "Buffer.h"
#include "CLProgram.h"
#include "ResourceContext.h"

#include <atomic>

namespace gladius
{
    class ProgramBase
    {
      public:
        explicit ProgramBase(SharedComputeContext context, const SharedResources resources);
        void recompileNonBlocking();
        void recompileBlocking();
        void buildKernelLib() const;

        void setOnProgramSwapCallBack(const std::function<void()> & callBack);
        bool isCompilationInProgress() const;
        bool isValid() const;

        void setModelKernel(const std::string & newModelKernelSource);
        void setEnableVdb(bool enableVdb);

        /// Set a shared logger that will be propagated to the underlying CLProgram
        void setLogger(events::SharedLogger logger);

        void waitForCompilation() const;

        void dumpSource(std::filesystem::path const & path) const;

        // Binary caching support
        void setCacheDirectory(const std::filesystem::path & path);
        void clearCache();
        void setCacheEnabled(bool enabled);
        [[nodiscard]] bool isCacheEnabled() const;

      protected:
        void swapProgramsIfNeeded();
        static void noOp()
        {
        }
        SharedComputeContext m_ComputeContext;
        std::unique_ptr<CLProgram> m_programFront;
        std::atomic_bool m_programSwapRequired{false};

        SharedResources m_resoures;
        BuildCallBack m_buildFinishedCallBack;
        std::function<void()> m_onProgramSwapCallBack{noOp};

        std::string m_modelKernel;
        bool m_isFirstBuild = true;

        bool m_enableVdb = false;

        FileNames m_sourceFilesProgram;
        FileNames m_sourceFilesLib;

        events::SharedLogger m_logger{};
    };
}
