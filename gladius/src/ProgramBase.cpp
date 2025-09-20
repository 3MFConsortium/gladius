#include "ProgramBase.h"

#include "Profiling.h"
#include <fmt/format.h>
#include <string>

#include "exceptions.h"

namespace gladius
{
    ProgramBase::ProgramBase(SharedComputeContext context, const SharedResources resources)
        : m_ComputeContext(context)
        , m_programFront(std::make_unique<CLProgram>(context))
        , m_resoures(resources)
    {
        if (m_logger)
        {
            m_programFront->setLogger(m_logger);
        }
        m_sourceFilesProgram = {"arguments.h",
                                "types.h",
                                "sdf.h",
                                "sampler.h",
                                "rendering.h",
                                "sdf_generator.h",
                                "CNanoVDB.h",
                                "sdf.cl",
                                "rendering.cl",
                                "sdf_generator.cl"};

        m_sourceFilesLib = {
          "arguments.h",
          "types.h",
          "CNanoVDB.h",
          "sdf.h",
          "sdf_generator.h",
          "sampler.h",
        };
    }

    void ProgramBase::swapProgramsIfNeeded()
    {

        if (m_programSwapRequired)
        {
            m_programSwapRequired = false;
            m_onProgramSwapCallBack();
        }
    }

    void ProgramBase::waitForCompilation() const
    {
        ProfileFunction if (!m_ComputeContext->isValid())
        {
            return;
        }
        m_programFront->finishCompilation();
    }

    void ProgramBase::dumpSource(std::filesystem::path const & path) const
    {
        ProfileFunction m_programFront->dumpSource(path);
    }

    void ProgramBase::recompileNonBlocking()
    {
        ProfileFunction
        try
        {
            if (m_modelKernel.empty())
            {
                if (m_logger)
                {
                    m_logger->logWarning("Aborting compilation: No model source set");
                }
                else
                {
                    std::cerr << "aborting compilation: No model source set\n";
                }
                return;
            }

            if (m_enableVdb)
            {
                m_programFront->addSymbol("ENABLE_VDB");
            }
            else
            {
                m_programFront->removeSymbol("ENABLE_VDB");
            }

            m_buildFinishedCallBack = [&]() { m_programSwapRequired = true; };
            m_programFront->clearSources();

            if (m_isFirstBuild)
            {
                m_isFirstBuild = false;
                waitForCompilation();

                swapProgramsIfNeeded();

                m_programFront->buildFromSourceAndLinkWithLib(
                  m_sourceFilesProgram, m_modelKernel, m_buildFinishedCallBack);
                m_programSwapRequired = true;
            }
            else
            {
                m_programFront->buildFromSourceAndLinkWithLibNonBlocking(
                  m_sourceFilesProgram, m_modelKernel, m_buildFinishedCallBack);
            }
        }
        catch (OpenCLError & e)
        {
            m_ComputeContext->invalidate("OpenCL error during compilation in ProgramBase");
            throw e;
        }
    }

    void ProgramBase::recompileBlocking()
    {
        ProfileFunction if (m_modelKernel.empty())
        {
            if (m_logger)
            {
                m_logger->logWarning("Aborting compilation: No model source set");
            }
            else
            {
                std::cerr << "aborting compilation: No model source set\n";
            }
            return;
        }

        if (m_enableVdb)
        {
            m_programFront->addSymbol("ENABLE_VDB");
        }
        else
        {
            m_programFront->removeSymbol("ENABLE_VDB");
        }

        m_programFront->clearSources();
        m_programFront->buildFromSourceAndLinkWithLib(
          m_sourceFilesProgram, m_modelKernel, m_buildFinishedCallBack);
        m_programSwapRequired = true;
        swapProgramsIfNeeded();
        m_isFirstBuild = false;
    }

    void ProgramBase::buildKernelLib() const
    {
        ProfileFunction m_programFront->clearSources();

        m_programFront->loadAndCompileLib(m_sourceFilesLib);
    }

    void ProgramBase::setOnProgramSwapCallBack(const std::function<void()> & callBack)
    {
        m_onProgramSwapCallBack = callBack;
    }

    bool ProgramBase::isCompilationInProgress() const
    {
        return m_programFront->isCompilationInProgress();
    }

    bool ProgramBase::isValid() const
    {
        if (!m_programFront)
        {
            return false;
        }

        return m_programFront->isValid();
    }

    void ProgramBase::setModelKernel(const std::string & newModelKernelSource)
    {
        m_modelKernel = newModelKernelSource;
    }

    void ProgramBase::setEnableVdb(bool enableVdb)
    {
        m_enableVdb = enableVdb;
    }

    void ProgramBase::setLogger(events::SharedLogger logger)
    {
        m_logger = std::move(logger);
        if (m_programFront)
        {
            m_programFront->setLogger(m_logger);
        }
    }

    void ProgramBase::setCacheDirectory(const std::filesystem::path & path)
    {
        if (m_logger)
        {
            m_logger->logInfo("ProgramBase::setCacheDirectory called with path: " + path.string());
        }
        if (m_programFront)
        {
            if (m_logger)
            {
                m_logger->logInfo("ProgramBase: Calling CLProgram setCacheDirectory");
            }
            m_programFront->setCacheDirectory(path);
        }
        else if (m_logger)
        {
            m_logger->logWarning("ProgramBase: m_programFront is null!");
        }
    }

    void ProgramBase::clearCache()
    {
        if (m_programFront)
        {
            m_programFront->clearCache();
        }
    }

    void ProgramBase::setCacheEnabled(bool enabled)
    {
        if (m_programFront)
        {
            m_programFront->setCacheEnabled(enabled);
        }
        else if (m_logger)
        {
            m_logger->logWarning(
              "ProgramBase: m_programFront is null, cannot set cache enabled state!");
        }
    }

    bool ProgramBase::isCacheEnabled() const
    {
        if (m_programFront)
        {
            return m_programFront->isCacheEnabled();
        }
        return true; // Default value when program is not available
    }
}
