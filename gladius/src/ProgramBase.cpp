#include "ProgramBase.h"

#include <fmt/format.h>
#include <string>
#include "Profiling.h"

#include "exceptions.h"

namespace gladius
{
    ProgramBase::ProgramBase(SharedComputeContext context, const SharedResources resources)
        : m_ComputeContext(context)
        , m_programFront(std::make_unique<CLProgram>(context))
        , m_resoures(resources)
    {
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
        ProfileFunction
        if (!m_ComputeContext->isValid())
        {
            return;
        }
        m_programFront->finishCompilation();
    }

    void ProgramBase::dumpSource(std::filesystem::path const & path) const
    {
        ProfileFunction
        m_programFront->dumpSource(path);
    }

    void ProgramBase::recompileNonBlocking()
    {
        ProfileFunction
        try
        {
            if (m_modelKernel.empty())
            {
                std::cerr << "aborting compilation: No model source set\n";
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
            m_ComputeContext->invalidate();
            throw e;
        }
    }

    void ProgramBase::recompileBlocking()
    {
        ProfileFunction
        if (m_modelKernel.empty())
        {
            std::cerr << "aborting compilation: No model source set\n";
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
        ProfileFunction
        m_programFront->clearSources();

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
}
