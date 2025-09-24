#pragma once

#include "ComputeContext.h"
#include "EventLogger.h"
#include "exceptions.h"
#include "gpgpu.h"

#include <fmt/format.h>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
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
        void addDynamicSource(const std::string & source);

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

            auto logError = [&](const std::string & stage, const std::string & details = "")
            {
                auto const threadId = std::this_thread::get_id();
                auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));
                std::string msg =
                  fmt::format("[CLProgram::runNonBlocking] {}: Method='{}', Thread={}, Valid={}",
                              stage,
                              methodName,
                              threadIdStr,
                              isValid());
                if (!details.empty())
                {
                    msg += ", Details: " + details;
                }
                if (m_ComputeContext)
                {
                    // Validate the queue before including diagnostics
                    if (m_ComputeContext->validateQueue(queue))
                    {
                        msg += ", QueueValid=true";
                    }
                    else
                    {
                        msg += ", QueueValid=false";
                    }
                    msg += "\n" + m_ComputeContext->getDiagnosticInfo();
                }
                if (m_logger)
                {
                    m_logger->logError(msg);
                }
                // Avoid noisy stderr fallback; rely on logger when available
            };

            if (!isValid())
            {
                logError("Program not valid - returning");
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
                    logError("Kernel creation failed", fmt::format("OpenCL error: {}", err));
                    m_ComputeContext->invalidate(
                      "Kernel creation failed in CLProgram::runNonBlocking");
                    if (m_logger)
                    {
                        m_logger->logError("OpenCL: Creating kernel '" + methodName +
                                           "' failed (error: " + std::to_string(err) + ")");
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
                                }
                            }
                            catch (...)
                            {
                                // ignore
                            }

                            // Additional build diagnostics
                            try
                            {
                                cl_program_binary_type binType{};
                                m_program->getBuildInfo(device, CL_PROGRAM_BINARY_TYPE, &binType);
                                const char * typeStr =
                                  (binType == CL_PROGRAM_BINARY_TYPE_NONE) ? "NONE"
                                  : (binType == CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT)
                                    ? "COMPILED_OBJECT"
                                  : (binType == CL_PROGRAM_BINARY_TYPE_LIBRARY)    ? "LIBRARY"
                                  : (binType == CL_PROGRAM_BINARY_TYPE_EXECUTABLE) ? "EXECUTABLE"
                                                                                   : "UNKNOWN";
                                if (m_logger)
                                {
                                    m_logger->logError(std::string("  Binary type : ") + typeStr);
                                }
                            }
                            catch (...)
                            {
                            }

                            try
                            {
                                auto const status =
                                  m_program->getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device);
                                std::string statusStr = (status == CL_BUILD_NONE)      ? "NONE"
                                                        : (status == CL_BUILD_ERROR)   ? "ERROR"
                                                        : (status == CL_BUILD_SUCCESS) ? "SUCCESS"
                                                        : (status == CL_BUILD_IN_PROGRESS)
                                                          ? "IN_PROGRESS"
                                                          : "UNKNOWN";
                                if (m_logger)
                                {
                                    m_logger->logError(std::string("  Build status: ") + statusStr);
                                }
                            }
                            catch (...)
                            {
                            }

                            try
                            {
                                auto const log =
                                  m_program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
                                if (!log.empty())
                                {
                                    if (m_logger)
                                    {
                                        m_logger->logError(std::string("  Build log  :\n") + log);
                                    }
                                }
                            }
                            catch (...)
                            {
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

            try
            {
                setArgument(0, m_kernels[methodName], args...);
            }
            catch (const std::exception & e)
            {
                logError("Setting kernel arguments failed", e.what());
                throw;
            }

            try
            {
                CL_ERROR(
                  queue.enqueueNDRangeKernel(m_kernels[methodName], origin, range, cl::NullRange));
            }
            catch (const OpenCLError & e)
            {
                logError("Kernel enqueue failed", e.what());
                throw;
            }
        }

        template <typename... ArgumentTypes>
        void run(cl::CommandQueue const & queue,
                 const std::string & methodName,
                 cl::NDRange origin,
                 cl::NDRange range,
                 const ArgumentTypes &... args)
        {

            ProfileFunction;

            // Enhanced error logging with context information
            auto logError = [&](const std::string & stage, const std::string & details = "")
            {
                auto const threadId = std::this_thread::get_id();
                auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));
                std::string msg =
                  fmt::format("[CLProgram::run] {}: Method='{}', Thread={}, Valid={}",
                              stage,
                              methodName,
                              threadIdStr,
                              isValid());
                if (!details.empty())
                {
                    msg += ", Details: " + details;
                }
                if (m_ComputeContext)
                {
                    msg += "\n" + m_ComputeContext->getDiagnosticInfo();
                }
                if (m_logger)
                {
                    m_logger->logError(msg);
                }
                // Avoid noisy stderr fallback; rely on logger when available
            };

            try
            {
                CL_ERROR(queue.finish());
            }
            catch (const OpenCLError & e)
            {
                logError("Pre-finish failed", e.what());
                throw;
            }

            if (!isValid())
            {
                logError("Program invalid");
                return;
            }

            try
            {
                runNonBlocking(queue, methodName, origin, range, args...);
            }
            catch (const std::exception & e)
            {
                logError("RunNonBlocking failed", e.what());
                throw;
            }

            try
            {
                CL_ERROR(queue.finish());
            }
            catch (const OpenCLError & e)
            {
                logError("Queue finish failed", e.what());

                throw;
            }
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

        // Binary caching support
        void setCacheDirectory(const std::filesystem::path & path);
        void clearCache();
        void setCacheEnabled(bool enabled);
        [[nodiscard]] bool isCacheEnabled() const;

      private:
        template <typename T>
        void collectMemoryObjects(std::vector<cl::Memory> & buffers, const T & arg) const
        {
            if constexpr (std::is_base_of_v<cl::Memory, T>)
            {
                buffers.push_back(arg);
            }
        }

        template <typename T, typename... Args>
        void collectMemoryObjects(std::vector<cl::Memory> & buffers,
                                  const T & arg,
                                  const Args &... args) const
        {
            collectMemoryObjects(buffers, arg);
            if constexpr (sizeof...(Args) > 0)
            {
                collectMemoryObjects(buffers, args...);
            }
        }
        void compileAsLib();
        void compile(BuildCallBack & callBack);

        std::string generateDefineSymbol() const;

        void applyAllKernelReplacements();

        size_t computeHash() const;

        // Shared implementation for build methods to eliminate code duplication
        void buildFromSourceAndLinkWithLibImpl(const FileNames & filenames,
                                               const std::string & dynamicSource,
                                               BuildCallBack & callBack,
                                               bool nonBlocking);

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

        // Binary caching support
        std::filesystem::path m_cacheDirectory;
        bool m_cacheEnabled = false; // Cache disabled by default

        // Static vs Dynamic source tracking
        cl::Program::Sources m_staticSources;  // Static kernel files (.cl files from resources)
        cl::Program::Sources m_dynamicSources; // Dynamic model-specific source code
        FileNames
          m_sourceFilenames; // Track original filenames for header/implementation separation

      private:
        // Cache management helpers
        bool loadProgramFromCache(size_t hash);
        void saveProgramToCache(size_t hash);
        std::string getDeviceSignature() const;

        // Two-level caching helpers
        size_t computeStaticHash() const;
        size_t computeDynamicHash() const;
        bool loadStaticLibraryFromCache(size_t staticHash, cl::Program & staticLibrary);
        void saveStaticLibraryToCache(size_t staticHash, const cl::Program & staticLibrary);
        bool loadLinkedProgramFromCache(size_t staticHash, size_t dynamicHash);
        void saveLinkedProgramToCache(size_t staticHash, size_t dynamicHash);

        // Single-level compilation fallback extracted from compile()
        void compileSingleLevel(BuildCallBack & callBack, size_t currentHash);
    };
}
