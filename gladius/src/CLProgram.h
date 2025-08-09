#pragma once

#include "ComputeContext.h"
#include "EventLogger.h"
#include "gpgpu.h"

#include <future>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ImageStackOCL.h"
#include "Profiling.h"
#include "ResourceContext.h"
#include "ResourceManager.h"

#include "KernelReplacements.h"

namespace gladius
{
    using FileNames = std::vector<std::string>;
    using BuildCallBack = std::optional<std::function<void()>>;

    class CLProgram;

    struct CallBackUserData
    {
        BuildCallBack * callBack{};
        const ComputeContext * computeContext{};
        cl::Program * program{};
        CLProgram * sender{};
    };

    void applyKernelReplacements(std::string & source,
                                 const KernelReplacements & kernelReplacements);

    template <typename ArgT, typename... ArgumentTypes>
    void setArgument(int argumentNumber,
                     cl::Kernel & kernel,
                     const ArgT & arg,
                     const ArgumentTypes &... args)
    {
        {
            CL_ERROR(kernel.setArg(argumentNumber, arg));
            ++argumentNumber;
        }
        if constexpr (sizeof...(ArgumentTypes) > 0)
        {
            setArgument(argumentNumber, kernel, args...);
        }
    }

    class CLProgram
    {
      public:
        CLProgram(SharedComputeContext context);

        /// Set the shared event logger used for diagnostics
        void setLogger(events::SharedLogger logger)
        {
            m_logger = std::move(logger);
        }

        /// Get the shared event logger
        [[nodiscard]] events::SharedLogger getSharedLogger() const
        {
            return m_logger;
        }

        void loadSourceFromFile(const FileNames & filenames);

        void addSource(const std::string & source);

        void compileNonBlocking(BuildCallBack & callBack);

        void buildWithLib(BuildCallBack & callBack);

        void buildWithLibNonBlocking(BuildCallBack & callBack);

        /// blocks until an ongoing compilation has finished
        void finishCompilation();

        template <typename... ArgumentTypes>
        void runNonBlocking(cl::CommandQueue const & queue,
                            const std::string & methodName,
                            cl::NDRange origin,
                            cl::NDRange range,
                            const ArgumentTypes &... args)
        {
            ProfileFunction;
            if (!isValid())
            {
                return;
            }
            std::scoped_lock lock(m_compilationMutex);

            const auto kernelIt = m_kernels.find(methodName);
            if (kernelIt == std::end(m_kernels))
            {
                cl_int err{CL_SUCCESS};
                m_kernels[methodName] = cl::Kernel(*m_program, methodName.c_str(), &err);
                if (err != CL_SUCCESS)
                {
                    m_ComputeContext->invalidate();
                    if (m_logger)
                    {
                        m_logger->logError("OpenCL: Creating kernel '" + methodName +
                                           "' failed (error: " + std::to_string(err) + ")");
                    }
                    else
                    {
                        std::cerr << "OpenCL: Creating kernel '" << methodName
                                  << "' failed (error: " << err << ")\n";
                    }
                    try
                    {
                        // Provide diagnostics about the program and attempted kernel
                        auto const & device = m_ComputeContext->GetDevice();
                        auto const devName = device.getInfo<CL_DEVICE_NAME>();
                        if (m_logger)
                        {
                            m_logger->logError(std::string("  Device      : ") + devName);
                        }
                        else
                        {
                            std::cerr << "  Device      : " << devName << "\n";
                        }
                        if (m_program)
                        {
                            // Best effort: show available kernel names
                            try
                            {
                                auto const kernelNames =
                                  m_program->getInfo<CL_PROGRAM_KERNEL_NAMES>();
                                if (!kernelNames.empty())
                                {
                                    if (m_logger)
                                    {
                                        m_logger->logError(std::string("  Program kernels: ") +
                                                           kernelNames);
                                    }
                                    else
                                    {
                                        std::cerr << "  Program kernels: " << kernelNames << "\n";
                                    }
                                }
                            }
                            catch (...)
                            {
                                // ignore
                            }
                        }
                    }
                    catch (...)
                    {
                        // swallow diagnostics exceptions
                    }
                }
                CL_ERROR(err);
            }
            setArgument(0, m_kernels[methodName], args...);

            CL_ERROR(
              queue.enqueueNDRangeKernel(m_kernels[methodName], origin, range, cl::NullRange));
        }

        template <typename... ArgumentTypes>
        void run(cl::CommandQueue const & queue,
                 const std::string & methodName,
                 cl::NDRange origin,
                 cl::NDRange range,
                 const ArgumentTypes &... args)
        {

            ProfileFunction;
            CL_ERROR(queue.finish());
            if (!isValid())
            {
                return;
            }
            runNonBlocking(queue, methodName, origin, range, args...);
            CL_ERROR(queue.finish());
        }

        template <typename... ArgumentTypes>
        void run(const std::string & methodName,
                 cl::NDRange origin,
                 cl::NDRange range,
                 const ArgumentTypes &... args)
        {
            ProfileFunction;
            if (!isValid())
            {
                throw std::runtime_error("Program has not been compiled successfully yet");
            }
            run(m_ComputeContext->GetQueue(), methodName, origin, range, args...);
        }

        void loadAndCompileSource(const FileNames & filenames,
                                  const std::string & dynamicSource,
                                  BuildCallBack & callBack);

        void buildFromSourceAndLinkWithLibNonBlocking(const FileNames & filenames,
                                                      const std::string & dynamicSource,
                                                      BuildCallBack & callBack);

        void buildFromSourceAndLinkWithLib(const FileNames & filenames,
                                           const std::string & dynamicSource,
                                           BuildCallBack & callBack);

        void loadAndCompileLib(const FileNames & filenames);

        [[nodiscard]] bool isValid() const;
        void compilationFinishedHandler();

        void addSymbol(const std::string & symbol);
        void removeSymbol(const std::string & symbol);

        void setAdditionalDefine(std::string define);
        void clearSources();

        void setUseFastRelaxedMath(bool useFastRelaxedMath);

        void dumpSource(std::filesystem::path filename);

        void setKernelReplacements(SharedKernelReplacements kernelReplacements);

        [[nodiscard]] bool isCompilationInProgress() const;

      private:
        void compileAsLib();
        void compile(BuildCallBack & callBack);

        std::string generateDefineSymbol() const;

        void applyAllKernelReplacements();

        size_t computeHash() const;

        std::unique_ptr<cl::Program> m_program;
        std::unique_ptr<cl::Program> m_lib;
        cl::Program::Sources m_sources;
        SharedComputeContext m_ComputeContext;
        events::SharedLogger m_logger{};

        std::map<std::string, cl::Kernel> m_kernels;
        std::atomic<bool> m_valid{false};
        std::atomic<bool> m_isCompilationInProgress{false};

        CallBackUserData m_callBackUserData;

        std::future<void> m_kernelCompilation;
        std::set<std::string> m_symbols;
        std::string m_additionalDefine;

        std::mutex m_compilationMutex;

        bool m_useFastRelaxedMath = true;
        std::atomic_size_t m_hashLastSuccessfulCompilation = 0u;

        SharedKernelReplacements m_kernelReplacements;
    };
}
