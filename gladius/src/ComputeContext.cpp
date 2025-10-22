#include "ComputeContext.h"

#include "Profiling.h"
#include "exceptions.h"

#include <fmt/format.h>

#include <iostream>
#include <set>
#include <sstream>
#include <utility>

#if defined(__has_include)
#if __has_include(<CL/cl_ext.h>)
#include <CL/cl_ext.h>
#endif
#endif

namespace gladius
{
    namespace
    {
        // Global weak logger holder for low-level logging paths
        std::weak_ptr<events::Logger> g_loggerWeak;
    }

    void setGlobalLogger(events::SharedLogger logger)
    {
        g_loggerWeak = logger;
    }

    events::SharedLogger getGlobalLogger()
    {
        return g_loggerWeak.lock();
    }

    void checkError(cl_int err, const std::string & description)
    {
        if (err != CL_SUCCESS)
        {
            auto const threadId = std::this_thread::get_id();
            auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));

            auto const msg = fmt::format("OpenCL error: {} ({}): {} [Thread: {}]",
                                         description,
                                         err,
                                         getOpenCLErrorDescription(err),
                                         threadIdStr);
            if (auto logger = getGlobalLogger())
            {
                logger->logError(msg);
            }
            else
            {
                std::cerr << msg << "\n";
            }
            throw OpenCLError(err);
        }
    }

    Capabilities queryCapabilities(cl::Device const & device)
    {
        Capabilities caps;

        try
        {
            caps.openCLVersion = getOpenCLVersion(device);

            auto const fp32Config =
              static_cast<cl_uint>(device.getInfo<CL_DEVICE_SINGLE_FP_CONFIG>());
            caps.correctlyRoundedDivedSqrt = ((fp32Config & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT) ==
                                              CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);

            auto const extensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
            auto constexpr fp64ExtensionName = "cl_khr_fp64";
            // More robust extension checking to avoid false positives
            std::string const searchPattern = std::string(" ") + fp64ExtensionName + " ";
            std::string const prefixPattern = std::string(fp64ExtensionName) + " ";
            std::string const suffixPattern = std::string(" ") + fp64ExtensionName;

            caps.fp64 =
              (extensions.find(searchPattern) != std::string::npos) ||
              (extensions.find(prefixPattern) == 0) ||
              (extensions.length() >= suffixPattern.length() &&
               extensions.rfind(suffixPattern) == extensions.length() - suffixPattern.length()) ||
              (extensions == fp64ExtensionName);

            auto const deviceType = device.getInfo<CL_DEVICE_TYPE>();
            caps.cpu = (deviceType == CL_DEVICE_TYPE_CPU);
            caps.gpu = (deviceType == CL_DEVICE_TYPE_GPU);

            auto const maxClock = device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
            auto const computeUnits = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();

            auto const vendor = device.getInfo<CL_DEVICE_VENDOR>();

            // Safe vendor string comparison
            bool const isIntel = (!vendor.empty() && vendor.rfind("Intel", 0) == 0);

            auto const vendorRating = (isIntel) ? 0.01 : 1.;
            auto const deviceTypeRating = caps.cpu ? 0.1 : 1.;
            caps.performanceEstimation = maxClock * computeUnits * vendorRating * deviceTypeRating;
        }
        catch (std::exception const & e)
        {
            std::string deviceInfo = "unknown device";
            try
            {
                deviceInfo = device.getInfo<CL_DEVICE_NAME>();
            }
            catch (...)
            {
                // If we can't even get the device name, use generic info
            }

            // Check if this is an OpenCL-related error by examining the error message
            std::string errorMsg = e.what();
            if (errorMsg.find("OpenCL") != std::string::npos ||
                errorMsg.find("CL_") != std::string::npos)
            {
                throw OpenCLDeviceQueryError(deviceInfo, errorMsg);
            }

            std::cerr << "Warning: Failed to query device capabilities for " << deviceInfo << ": "
                      << e.what() << "\n";
            // Return default capabilities with safe defaults
            caps.openCLVersion = 1.0;
            caps.fp64 = false;
            caps.correctlyRoundedDivedSqrt = false;
            caps.cpu = false;
            caps.gpu = false;
            caps.performanceEstimation = 0.0;
        }

        return caps;
    }

    AcceleratorList queryAccelerators(std::ostream & logStream)
    {
        AcceleratorList candidates;

        try
        {
            std::vector<cl::Platform> allPlatforms;
            cl::Platform::get(&allPlatforms);

            if (allPlatforms.empty())
            {
                logStream << "No OpenCL platforms found.\n";
                throw OpenCLPlatformError("No OpenCL platforms available on this system. Please "
                                          "check OpenCL installation and drivers.");
            }

            for (unsigned int i = 0; i < allPlatforms.size(); i++)
            {
                try
                {
                    auto const platformName = allPlatforms[i].getInfo<CL_PLATFORM_NAME>();
                    logStream << "\nDevices of platform " << i + 1 << ") " << platformName << ":\n";

                    std::vector<cl::Device> allDevices;
                    allPlatforms[i].getDevices(CL_DEVICE_TYPE_ALL, &allDevices);

                    if (allDevices.empty())
                    {
                        logStream << "\tNo device found. \n";
                        continue;
                    }

                    for (auto & device : allDevices)
                    {
                        try
                        {
                            auto const deviceName = device.getInfo<CL_DEVICE_NAME>();
                            logStream << "\n\t" << deviceName << "\n";

                            auto caps = queryCapabilities(device);
                            logStream << "Performance rating:" << caps.performanceEstimation
                                      << "\n";

                            auto const vendor = device.getInfo<CL_DEVICE_VENDOR>();
                            logStream << "Vendor:" << vendor << "\n";

                            auto const extensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
                            logStream << "Extensions:\n" << extensions << "\n";

                            if (caps.fp64) // fp64 is required by nanovdb
                            {
                                auto accelerator =
                                  Accelerator{device, allPlatforms[i], std::move(caps)};
                                candidates.push_back(accelerator);
                            }
                            else
                            {
                                logStream
                                  << "\tSkipping device (no fp64 support required for nanovdb)\n";
                            }
                        }
                        catch (OpenCLDeviceQueryError const & e)
                        {
                            logStream << "\tError: " << e.what() << "\n";
                            // Continue with next device instead of breaking the loop
                        }
                        catch (std::exception const & e)
                        {
                            logStream << "\tWarning: Failed to query device info: " << e.what()
                                      << "\n";
                        }
                    }
                }
                catch (std::exception const & e)
                {
                    logStream << "\tWarning: Failed to query platform " << i + 1 << " ("
                              << allPlatforms[i].getInfo<CL_PLATFORM_NAME>() << "): " << e.what()
                              << "\n";
                }
            }
        }
        catch (OpenCLPlatformError const &)
        {
            // Re-throw platform-specific errors
            throw;
        }
        catch (std::exception const & e)
        {
            throw OpenCLPlatformError("Failed to enumerate OpenCL platforms: " +
                                      std::string(e.what()));
        }

        if (candidates.empty())
        {
            logStream << "\nNo suitable OpenCL devices found with required capabilities.\n";
        }

        return candidates;
    }

    OpenCLVersion getOpenCLVersion(cl::Device const & device)
    {
        std::string openCLVersionStr;

        try
        {
            openCLVersionStr = device.getInfo<CL_DEVICE_OPENCL_C_VERSION>();
        }
        catch (std::exception const & e)
        {
            throw OpenCLVersionParseError(
              "unknown", "Failed to retrieve OpenCL version from device: " + std::string(e.what()));
        }

        try
        {
            // Expected format: "OpenCL C 1.2" or "OpenCL C 2.0", etc.
            auto constexpr prefix = "OpenCL C ";
            auto constexpr prefixLength = 9;

            // Enhanced bounds checking to prevent buffer overruns
            if (openCLVersionStr.length() < prefixLength)
            {
                throw OpenCLVersionParseError(openCLVersionStr, "Version string too short");
            }

            if (openCLVersionStr.substr(0, prefixLength) != prefix)
            {
                throw OpenCLVersionParseError(openCLVersionStr,
                                              "Unexpected prefix, expected 'OpenCL C '");
            }

            if (openCLVersionStr.length() < prefixLength + 3) // Minimum: "OpenCL C 1.0"
            {
                throw OpenCLVersionParseError(openCLVersionStr, "Incomplete version number");
            }

            auto const numberStr = openCLVersionStr.substr(prefixLength);

            // Find the space or end to get just the version number
            auto const spacePos = numberStr.find(' ');
            auto const versionStr =
              (spacePos != std::string::npos) ? numberStr.substr(0, spacePos) : numberStr;

            // Additional validation: check if version string is not empty
            if (versionStr.empty())
            {
                throw OpenCLVersionParseError(openCLVersionStr, "Empty version number extracted");
            }

            OpenCLVersion const version{std::stod(versionStr)};

            // Validate that the version is reasonable
            if (version < 1.0 || version > 10.0) // Sanity check for version range
            {
                throw OpenCLVersionParseError(openCLVersionStr,
                                              "Version number " + std::to_string(version) +
                                                " is outside expected range [1.0, 10.0]");
            }

            return version;
        }
        catch (OpenCLVersionParseError const &)
        {
            // Re-throw our specific exceptions
            throw;
        }
        catch (std::invalid_argument const & e)
        {
            throw OpenCLVersionParseError(openCLVersionStr,
                                          "Invalid number format: " + std::string(e.what()));
        }
        catch (std::out_of_range const & e)
        {
            throw OpenCLVersionParseError(openCLVersionStr,
                                          "Version number out of range: " + std::string(e.what()));
        }
        catch (std::exception const & e)
        {
            throw OpenCLVersionParseError(openCLVersionStr,
                                          "Unexpected error: " + std::string(e.what()));
        }
    }

    ComputeContext::ComputeContext()
    {
        initContext();
    }

    ComputeContext::ComputeContext(EnableGLOutput enableOutput)
        : m_outputGL(enableOutput)
    {
        initContext();
    }

    const cl::Context & ComputeContext::GetContext() const
    {
        if (!m_context)
        {
            throw OpenCLContextCreationError(
              "Context is null - ComputeContext was not properly initialized");
        }

        if (!m_isValid)
        {
            throw OpenCLContextCreationError("ComputeContext is in invalid state");
        }

        return *m_context;
    }

    const cl::CommandQueue & ComputeContext::GetQueue()
    {
        std::lock_guard<std::mutex> lock(m_queuesMutex);

        auto const currentThreadId = std::this_thread::get_id();
        auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(currentThreadId));

        // Enhanced logging for queue retrieval
        auto logDiagnostics = [&](const std::string & stage)
        {
            if (m_debugOutputEnabled)
            {
                std::cerr << fmt::format(
                  "[GetQueue] {}: Thread={}, ContextValid={}, NumQueues={}, ContextPtr={}\n",
                  stage,
                  threadIdStr,
                  m_isValid,
                  m_queues.size(),
                  static_cast<void *>(m_context.get()));
            }
        };

        // Check if context is valid before attempting to create queue
        if (!m_isValid)
        {
            logDiagnostics("Context Invalid");
            throw OpenCLQueueCreationError("ComputeContext is not valid", currentThreadId);
        }

        if (!m_context)
        {
            logDiagnostics("Context Null");
            throw OpenCLQueueCreationError("OpenCL context is null", currentThreadId);
        }

        auto iter = m_queues.find(currentThreadId);
        if (iter == m_queues.end())
        {

            try
            {
                auto newQueue = createQueue();
                auto result = m_queues.emplace(currentThreadId, std::move(newQueue));
                if (!result.second)
                {
                    logDiagnostics("Queue insertion failed");
                    throw ThreadQueueManagementError("queue insertion", currentThreadId);
                }

                return result.first->second;
            }
            catch (OpenCLQueueCreationError const &)
            {
                logDiagnostics("Queue creation error - rethrowing");
                // Re-throw our specific queue creation errors
                throw;
            }
            catch (ThreadQueueManagementError const &)
            {
                logDiagnostics("Thread management error - rethrowing");
                // Re-throw thread management errors
                throw;
            }
            catch (OpenCLError const & e)
            {
                logDiagnostics("OpenCL error during creation");
                throw OpenCLQueueCreationError(
                  "OpenCL error during queue creation: " + std::string(e.what()), currentThreadId);
            }
            catch (std::exception const & e)
            {
                logDiagnostics("Unexpected error during creation");
                throw OpenCLQueueCreationError("Unexpected error during queue creation: " +
                                                 std::string(e.what()),
                                               currentThreadId);
            }
        }

        return iter->second;
    }

    bool ComputeContext::isValid() const
    {
        return m_isValid;
    }

    const cl::Device & ComputeContext::GetDevice() const
    {
        if (!m_isValid)
        {
            throw OpenCLContextCreationError(
              "ComputeContext is in invalid state - cannot return device");
        }

        return m_device;
    }

    void ComputeContext::invalidate()
    {
        invalidate("unspecified reason");
    }

    void ComputeContext::invalidate(const std::string & reason)
    {
        std::lock_guard<std::mutex> lock(m_queuesMutex);
        auto const threadIdStr =
          std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));

        m_invalidationCount++;
        if (m_debugOutputEnabled)
        {
            std::cerr << fmt::format("[ComputeContext::invalidate] Reason='{}', Thread={}, had {} "
                                     "queues, total invalidations={}\n",
                                     reason,
                                     threadIdStr,
                                     m_queues.size(),
                                     m_invalidationCount.load());

            if (m_invalidationCount > 5)
            {
                std::cerr
                  << "[ComputeContext::invalidate] WARNING: High number of invalidations detected! "
                     "This may indicate a serious OpenCL context issue.\n";
            }
        }

        m_isValid = false;

        // Clear all queues as they're now invalid
        m_queues.clear();
    }

    bool ComputeContext::validateQueue(const cl::CommandQueue & queue) const
    {
        if (!m_isValid)
        {
            return false;
        }

        try
        {
            // Try to get queue info to check if it's still valid
            auto context = queue.getInfo<CL_QUEUE_CONTEXT>();
            auto device = queue.getInfo<CL_QUEUE_DEVICE>();
            return (context() == m_context->operator()() && device() == m_device());
        }
        catch (...)
        {
            return false;
        }
    }

    std::string ComputeContext::getDiagnosticInfo() const
    {
        std::lock_guard<std::mutex> lock(m_queuesMutex);
        auto const threadIdStr =
          std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));

        std::string info = fmt::format("ComputeContext Diagnostics:\n"
                                       "  Current Thread: {}\n"
                                       "  Context Valid: {}\n"
                                       "  Context Ptr: {}\n"
                                       "  Number of Queues: {}\n"
                                       "  Output Method: {}\n"
                                       "  Total Invalidations: {}\n"
                                       "  Device Global Mem: {} MB\n"
                                       "  Device Max Alloc: {} MB\n"
                                       "  Tracked Allocated: {} MB\n",
                                       threadIdStr,
                                       m_isValid,
                                       static_cast<void *>(m_context.get()),
                                       m_queues.size(),
                                       static_cast<int>(m_outputMethod),
                                       m_invalidationCount.load(),
                                       m_deviceGlobalMemBytes / (1024 * 1024),
                                       m_deviceMaxAllocBytes / (1024 * 1024),
                                       m_trackedAllocatedBytes.load() / (1024 * 1024));

        if (m_isValid && m_context)
        {
            try
            {
                auto deviceName = m_device.getInfo<CL_DEVICE_NAME>();
                auto platformName = m_device.getInfo<CL_DEVICE_PLATFORM>();
                cl::Platform platform(platformName);
                auto platformNameStr = platform.getInfo<CL_PLATFORM_NAME>();

                info += fmt::format("  Device: {}\n"
                                    "  Platform: {}\n",
                                    deviceName,
                                    platformNameStr);
            }
            catch (...)
            {
                info += "  Device/Platform info: [Error retrieving]\n";
            }
        }

        return info;
    }

    bool ComputeContext::validateForOperation(const std::string & operationName) const
    {
        std::lock_guard<std::mutex> lock(m_queuesMutex);
        auto const threadId = std::this_thread::get_id();
        auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));

        bool allValid = true;
        std::string issues;

        if (!m_isValid)
        {
            allValid = false;
            issues += "Context marked invalid; ";
        }

        if (!m_context)
        {
            allValid = false;
            issues += "Context pointer is null; ";
        }

        auto queueIt = m_queues.find(threadId);
        if (queueIt != m_queues.end())
        {
            if (!validateQueue(queueIt->second))
            {
                allValid = false;
                issues += "Queue validation failed; ";
            }
        }
        else
        {
            // No queue exists for this thread yet - not necessarily an issue
            issues += "No queue exists for current thread (will be created); ";
        }

        if (m_debugOutputEnabled)
        {
            std::string logMsg = fmt::format(
              "[ComputeContext::validateForOperation] Operation='{}', Thread={}, Valid={}",
              operationName,
              threadIdStr,
              allValid);
            if (!issues.empty())
            {
                logMsg += ", Issues: " + issues;
            }

            std::cerr << logMsg << "\n";

            if (!allValid)
            {
                std::cerr << getDiagnosticInfo() << "\n";
            }
        }

        return allValid;
    }

    bool ComputeContext::validateBuffers(const std::string & operationName,
                                         const std::vector<cl::Memory> & buffers) const
    {
        if (!m_debugOutputEnabled)
        {
            return true; // Skip validation when debug output is disabled
        }

        auto const threadId = std::this_thread::get_id();
        auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));

        bool allValid = true;
        std::string issues;

        if (m_debugOutputEnabled)
        {
            std::cerr << fmt::format(
              "[ComputeContext::validateBuffers] Operation='{}', Thread={}, BufferCount={}\n",
              operationName,
              threadIdStr,
              buffers.size());
        }

        // Track buffer handles and check for corruption indicators
        std::set<cl_mem> uniqueHandles;
        size_t totalMemory = 0;

        for (size_t i = 0; i < buffers.size(); ++i)
        {
            try
            {
                // Basic buffer validity check
                auto size = buffers[i].getInfo<CL_MEM_SIZE>();
                auto flags = buffers[i].getInfo<CL_MEM_FLAGS>();
                auto context = buffers[i].getInfo<CL_MEM_CONTEXT>();
                auto handle = buffers[i]();

                totalMemory += size;

                // Track unique handles (reuse is normal and expected)
                bool isReused = uniqueHandles.count(handle) > 0;
                uniqueHandles.insert(handle);

                // Verify buffer belongs to our context
                bool contextMatch = (m_context && context() == m_context->operator()());
                if (!contextMatch)
                {
                    allValid = false;
                    issues += fmt::format("Buffer[{}] context mismatch; ", i);
                }

                // Check for signs of corruption
                bool suspiciousSize = false;
                if (size == 0)
                {
                    allValid = false;
                    issues += fmt::format("Buffer[{}] zero size; ", i);
                    suspiciousSize = true;
                }
                else if (size > (4ULL << 30))
                { // 4GB seems excessive for most operations
                    allValid = false;
                    issues += fmt::format(
                      "Buffer[{}] extremely large ({}GB); ", i, size / (1024 * 1024 * 1024));
                    suspiciousSize = true;
                }

                // Check if buffer handle is null (clear corruption indicator)
                if (handle == nullptr)
                {
                    allValid = false;
                    issues += fmt::format("Buffer[{}] null handle; ", i);
                }

                // Try to get host pointer if available
                std::string hostPtrInfo = "device-only";
                if (flags & CL_MEM_USE_HOST_PTR)
                {
                    try
                    {
                        auto hostPtr = buffers[i].getInfo<CL_MEM_HOST_PTR>();
                        hostPtrInfo =
                          fmt::format("host:0x{:x}", reinterpret_cast<uintptr_t>(hostPtr));
                    }
                    catch (...)
                    {
                        hostPtrInfo = "host:invalid";
                        allValid = false;
                        issues += fmt::format("Buffer[{}] invalid host ptr; ", i);
                    }
                }

                if (m_debugOutputEnabled)
                {
                    std::cerr << fmt::format("  Buffer[{}]: Size={}MB, Flags=0x{:x}, Context={}, "
                                             "Handle=0x{:x}, Memory={}, Reused={}\n",
                                             i,
                                             size / (1024 * 1024),
                                             flags,
                                             contextMatch ? "OK" : "MISMATCH",
                                             reinterpret_cast<uintptr_t>(handle),
                                             hostPtrInfo,
                                             isReused ? "YES" : "NO");
                }
            }
            catch (const std::exception & e)
            {
                allValid = false;
                issues += fmt::format("Buffer[{}] access failed: {}; ", i, e.what());
                if (m_debugOutputEnabled)
                {
                    std::cerr << fmt::format("  Buffer[{}]: CORRUPTED - {}\n", i, e.what());
                }
            }
        }

        if (m_debugOutputEnabled)
        {
            std::cerr << fmt::format("  Total GPU Memory: {}MB across {} unique buffers\n",
                                     totalMemory / (1024 * 1024),
                                     uniqueHandles.size());

            if (!allValid)
            {
                std::cerr << fmt::format(
                  "[ComputeContext::validateBuffers] CORRUPTION DETECTED: {}\n", issues);
            }
            else
            {
                std::cerr << "[ComputeContext::validateBuffers] All buffers appear healthy\n";
            }
        }

        return allValid;
    }

    void ComputeContext::checkMemoryLayoutConflicts(const std::string & operationName) const
    {
        if (!m_debugOutputEnabled)
        {
            return; // Skip memory layout checks when debug output is disabled
        }

        auto const threadId = std::this_thread::get_id();
        auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));

        if (m_debugOutputEnabled)
        {
            std::cerr << fmt::format(
              "[ComputeContext::checkMemoryLayoutConflicts] Operation='{}', Thread={}\n",
              operationName,
              threadIdStr);
        }

        try
        {
            // Get context memory statistics
            auto devices = m_context->getInfo<CL_CONTEXT_DEVICES>();
            if (!devices.empty())
            {
                auto device = devices[0];
                auto maxMemAlloc = device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
                auto globalMemSize = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
                auto localMemSize = device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();

                if (m_debugOutputEnabled)
                {
                    std::cerr << fmt::format(
                      "  Device Memory: Global={}MB, MaxAlloc={}MB, Local={}KB\n",
                      globalMemSize / (1024 * 1024),
                      maxMemAlloc / (1024 * 1024),
                      localMemSize / 1024);
                }
            }

            // Check for OpenCL context memory issues
            try
            {
                // Try to allocate a small test buffer to verify context health
                cl::Buffer testBuffer(*m_context, CL_MEM_READ_WRITE, 1024);
                auto size = testBuffer.getInfo<CL_MEM_SIZE>();
                if (m_debugOutputEnabled)
                {
                    std::cerr << fmt::format("  Test buffer allocation successful: {}B\n", size);
                }
            }
            catch (const std::exception & e)
            {
                if (m_debugOutputEnabled)
                {
                    std::cerr << fmt::format("  WARNING: Test buffer allocation failed: {}\n",
                                             e.what());
                }
            }
        }
        catch (const std::exception & e)
        {
            if (m_debugOutputEnabled)
            {
                std::cerr << fmt::format(
                  "[ComputeContext::checkMemoryLayoutConflicts] Error getting memory info: {}\n",
                  e.what());
            }
        }
    }

    cl::CommandQueue ComputeContext::createQueue() const
    {
        if (!m_context)
        {
            throw OpenCLQueueCreationError("Context is null", std::this_thread::get_id());
        }

        try
        {
            cl_int err;
            auto queue = cl::CommandQueue(*m_context, m_device, 0, &err);
            checkError(err, "Failed to create command queue");
            return queue;
        }
        catch (OpenCLError const & e)
        {
            throw OpenCLQueueCreationError("OpenCL error in createQueue: " + std::string(e.what()),
                                           std::this_thread::get_id());
        }
        catch (std::exception const & e)
        {
            throw OpenCLQueueCreationError("Unexpected error in createQueue: " +
                                             std::string(e.what()),
                                           std::this_thread::get_id());
        }
    }

    void ComputeContext::initContext()
    {
        cl::Platform defaultPlatform;

        try
        {
            // get all platforms (drivers)
            std::vector<cl::Platform> allPlatforms;
            cl::Platform::get(&allPlatforms);

            if (allPlatforms.empty())
            {
                throw OpenCLPlatformError("No OpenCL platforms found. Check OpenCL installation!");
            }

            // Use a null stream to suppress output during initialization
            std::ostringstream nullStream;
            auto accelerators = queryAccelerators(nullStream);

            if (accelerators.empty())
            {
                throw NoSuitableOpenCLDevicesFound();
            }

            std::stable_sort(std::begin(accelerators),
                             std::end(accelerators),
                             [](Accelerator const & lhs, Accelerator const & rhs)
                             {
                                 return lhs.capabilities.performanceEstimation >
                                        rhs.capabilities.performanceEstimation;
                             });

            m_device = accelerators.front().device;
            defaultPlatform = accelerators.front().platform;

            if (m_outputGL == EnableGLOutput::disabled)
            {
                try
                {
                    m_context = std::make_unique<cl::Context>(cl::Context({m_device}));
                    m_outputMethod = OutputMethod::disabled;
                    // Query memory capability limits now that device/context are set
                    queryDeviceMemoryCaps();
                }
                catch (std::exception const & e)
                {
                    throw OpenCLContextCreationError("Failed to create basic OpenCL context: " +
                                                     std::string(e.what()));
                }
            }
            else
            {
                cl_int err = CL_INVALID_CONTEXT;
                if (m_outputMethod == OutputMethod::interop)
                {
                    try
                    {
#ifdef WIN32
                        auto const currentContext = wglGetCurrentContext();
                        auto const currentDC = wglGetCurrentDC();

                        if (m_outputGL == EnableGLOutput::disabled)
                        {
                            throw OpenGLInteropError(
                              "No active OpenGL context found for Windows interop");
                        }
                        else
                        {
                            cl_context_properties configuration[] = {
                              CL_GL_CONTEXT_KHR,
                              reinterpret_cast<cl_context_properties>(currentContext),
                              CL_WGL_HDC_KHR,
                              reinterpret_cast<cl_context_properties>(currentDC),
                              CL_CONTEXT_PLATFORM,
                              reinterpret_cast<cl_context_properties>(defaultPlatform()),
                              0};

                            m_context = std::make_unique<cl::Context>(
                              cl::Context({m_device}, configuration, nullptr, nullptr, &err));
                        }
#endif
#ifdef __linux__
                        auto const currentContext = glXGetCurrentContext();
                        auto const currentDisplay = glXGetCurrentDisplay();

                        if (currentContext == nullptr || currentDisplay == nullptr)
                        {
                            throw OpenGLInteropError(
                              "No active OpenGL context found for Linux interop");
                        }
                        else
                        {
                            cl_context_properties configuration[] = {
                              CL_GL_CONTEXT_KHR,
                              (cl_context_properties) currentContext,
                              CL_GLX_DISPLAY_KHR,
                              (cl_context_properties) currentDisplay,
                              CL_CONTEXT_PLATFORM,
                              (cl_context_properties) defaultPlatform(),
                              0};

                            m_context = std::make_unique<cl::Context>(
                              cl::Context({m_device}, configuration, nullptr, nullptr, &err));
                        }
#endif
                    }
                    catch (OpenGLInteropError const &)
                    {
                        // Re-throw interop errors to be caught by outer handler
                        throw;
                    }
                    catch (std::exception const & e)
                    {
                        throw OpenGLInteropError("Failed to initialize interop mode: " +
                                                 std::string(e.what()));
                    }
                }
                if (err == CL_SUCCESS)
                {
                    // OpenGL sharing enabled using interop method
                    // Query memory capability limits now that device/context are set
                    queryDeviceMemoryCaps();
                }
                else
                {
                    m_context = std::make_unique<cl::Context>(cl::Context({m_device}));
                    m_outputMethod = OutputMethod::readpixel;
                    // Query memory capability limits now that device/context are set
                    queryDeviceMemoryCaps();
                    // OpenGL sharing enabled using the readpixel method
                }
            }

            m_isValid = true;
        }
        catch (std::exception const & /* e */)
        {
            // Handle any initialization errors
            m_isValid = false;
            throw;
        }
    }

    bool ComputeContext::isOpenCLAvailable()
    {
        try
        {
            // Try to get all platforms
            std::vector<cl::Platform> allPlatforms;
            cl::Platform::get(&allPlatforms);

            if (allPlatforms.empty())
            {
                return false;
            }

            // Query for accelerators with a null stream to avoid console output
            std::ostringstream nullStream;
            auto accelerators = queryAccelerators(nullStream);

            // Return true if we found at least one suitable device
            return !accelerators.empty();
        }
        catch (...)
        {
            // If any exception occurs (OpenCL not available, driver issues, etc.)
            return false;
        }
    }

    OutputMethod ComputeContext::outputMethod() const
    {
        return m_outputMethod;
    }

    void ComputeContext::setOutputMethod(const OutputMethod & outputMethod)
    {
        m_outputMethod = outputMethod;
    }

    void ComputeContext::queryDeviceMemoryCaps()
    {
        try
        {
            m_deviceGlobalMemBytes = m_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
            m_deviceMaxAllocBytes = m_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();

            // Clamp safety utilization against reported caps
            if (m_deviceMaxAllocBytes >
                static_cast<size_t>(m_deviceGlobalMemBytes * kSingleAllocSafetyUtilization))
            {
                // Some drivers report large max alloc; keep our own conservative cap
                m_deviceMaxAllocBytes =
                  static_cast<size_t>(m_deviceGlobalMemBytes * kSingleAllocSafetyUtilization);
            }

            if (m_debugOutputEnabled)
            {
                std::cerr << fmt::format(
                  "[ComputeContext] Device memory caps: Global={} MB, MaxAlloc={} MB\n",
                  m_deviceGlobalMemBytes / (1024 * 1024),
                  m_deviceMaxAllocBytes / (1024 * 1024));
            }
        }
        catch (const std::exception & e)
        {
            if (m_debugOutputEnabled)
            {
                std::cerr << fmt::format(
                  "[ComputeContext] WARNING: Failed to query device memory caps: {}\n", e.what());
            }
            m_deviceGlobalMemBytes = 0;
            m_deviceMaxAllocBytes = 0;
        }
    }

    bool ComputeContext::tryQueryVendorFreeMem(size_t & freeBytesOut) const
    {
        freeBytesOut = 0;
        // OpenCL core does not expose free VRAM. Some vendors have extensions:
        // - AMD: CL_DEVICE_GLOBAL_FREE_MEMORY_AMD (returns kB) via clGetDeviceInfo
        // - NVIDIA: No direct free mem query; can use clGetMemObjectInfo? not applicable
        // We'll attempt AMD query if the header symbol is available.
#ifdef CL_DEVICE_GLOBAL_FREE_MEMORY_AMD
        try
        {
            // The AMD extension is queried via raw C API; returns two size_t values: free/total in
            // KB
            size_t memInfoKB[2] = {0, 0};
            cl_device_id devId = m_device();
            cl_int status = clGetDeviceInfo(
              devId, CL_DEVICE_GLOBAL_FREE_MEMORY_AMD, sizeof(memInfoKB), memInfoKB, nullptr);
            if (status == CL_SUCCESS)
            {
                // memInfoKB[0] is free memory in KB according to docs
                freeBytesOut = memInfoKB[0] * 1024ULL;
                return true;
            }
        }
        catch (...)
        {
            // ignore and report failure
        }
#endif
        return false;
    }

    size_t ComputeContext::getApproxFreeMemBytes() const
    {
        size_t vendorFree = 0;
        if (tryQueryVendorFreeMem(vendorFree) && vendorFree > 0)
        {
            return vendorFree;
        }
        // Fallback to conservative accounting: total - trackedAllocated
        if (m_deviceGlobalMemBytes == 0)
        {
            return 0; // unknown
        }
        size_t accounted = m_trackedAllocatedBytes.load(std::memory_order_relaxed);
        if (accounted > m_deviceGlobalMemBytes)
        {
            return 0;
        }
        return m_deviceGlobalMemBytes - accounted;
    }

    void ComputeContext::onBufferAllocated(size_t bytes)
    {
        m_trackedAllocatedBytes.fetch_add(bytes, std::memory_order_relaxed);
    }

    void ComputeContext::onBufferReleased(size_t bytes)
    {
        m_trackedAllocatedBytes.fetch_sub(bytes, std::memory_order_relaxed);
    }

    std::unique_ptr<cl::Buffer> ComputeContext::createBufferChecked(cl_mem_flags flags,
                                                                    size_t bytes,
                                                                    void * hostPtr,
                                                                    const char * debugTag)
    {
        if (!m_isValid || !m_context)
        {
            throw OpenCLContextCreationError("Context invalid while creating buffer");
        }

        if (bytes == 0)
        {
            // Normalize to 1 byte minimal allocation to avoid zero-sized undefined behavior
            bytes = 1;
        }

        // Enforce single-allocation limit
        if (m_deviceMaxAllocBytes != 0 && bytes > m_deviceMaxAllocBytes)
        {
            auto msg = fmt::format("Requested allocation exceeds device max: {} MB > {} MB{}",
                                   bytes / (1024 * 1024),
                                   m_deviceMaxAllocBytes / (1024 * 1024),
                                   debugTag ? fmt::format(" [{}]", debugTag) : std::string());
            if (m_logger)
                m_logger->logError(msg);
            throw GladiusException(msg);
        }

        // Enforce total utilization safety budget
        if (m_deviceGlobalMemBytes != 0)
        {
            size_t approxFree = getApproxFreeMemBytes();
            // If unknown (0) we skip this check; otherwise ensure we keep
            // kTotalMemSafetyUtilization
            if (approxFree != 0)
            {
                // Don't exceed safety fraction of total memory
                size_t targetCap =
                  static_cast<size_t>(m_deviceGlobalMemBytes * kTotalMemSafetyUtilization);
                size_t currentlyAllocated = m_trackedAllocatedBytes.load(std::memory_order_relaxed);
                if (currentlyAllocated + bytes > targetCap)
                {
                    auto msg = fmt::format(
                      "Allocation would exceed safety cap: need {} MB, used {} MB, cap {} MB{}",
                      bytes / (1024 * 1024),
                      currentlyAllocated / (1024 * 1024),
                      targetCap / (1024 * 1024),
                      debugTag ? fmt::format(" [{}]", debugTag) : std::string());
                    if (m_logger)
                        m_logger->logError(msg);
                    throw GladiusException(msg);
                }
                // Optional extra guard: if vendor reported free < requested, warn/throw
                if (approxFree < bytes)
                {
                    auto msg = fmt::format(
                      "Vendor-reported free VRAM too low: free {} MB, requested {} MB{}",
                      approxFree / (1024 * 1024),
                      bytes / (1024 * 1024),
                      debugTag ? fmt::format(" [{}]", debugTag) : std::string());
                    if (m_logger)
                        m_logger->logWarning(msg);
                    throw GladiusException(msg);
                }
            }
        }

        cl_int err = CL_SUCCESS;
        std::unique_ptr<cl::Buffer> buf;
        try
        {
            buf = std::make_unique<cl::Buffer>(*m_context, flags, bytes, hostPtr, &err);
            checkError(err, "createBufferChecked: cl::Buffer ctor");
        }
        catch (const OpenCLError &)
        {
            throw; // already descriptive
        }
        catch (const std::exception & e)
        {
            auto msg = std::string("Failed to allocate OpenCL buffer: ") + e.what();
            if (m_logger)
                m_logger->logError(msg);
            throw GladiusException(msg);
        }

        // Track accounted bytes on success
        onBufferAllocated(bytes);
        if (m_logger && m_debugOutputEnabled)
        {
            m_logger->logInfo(
              fmt::format("Allocated {} MB (tag: {}). In-use: {} MB of {} MB (max single {} MB)",
                          bytes / (1024 * 1024),
                          debugTag ? debugTag : "-",
                          m_trackedAllocatedBytes.load() / (1024 * 1024),
                          m_deviceGlobalMemBytes / (1024 * 1024),
                          m_deviceMaxAllocBytes / (1024 * 1024)));
        }
        return buf;
    }

    size_t ComputeContext::estimateImageSizeBytes(const cl::ImageFormat & format,
                                                  size_t width,
                                                  size_t height,
                                                  size_t depth)
    {
        // Rough estimation based on channel order and type; sufficient for safety checks
        auto channels = [&]() -> size_t
        {
            switch (format.image_channel_order)
            {
            case CL_R:
            case CL_A:
                return 1;
            case CL_RG:
            case CL_RA:
                return 2;
            case CL_RGB:
                return 3;
            case CL_RGBA:
            case CL_BGRA:
            case CL_ARGB:
                return 4;
            default:
                return 4; // assume worst-case
            }
        }();

        auto bytesPerChannel = [&]() -> size_t
        {
            switch (format.image_channel_data_type)
            {
            case CL_SNORM_INT8:
            case CL_UNORM_INT8:
            case CL_SIGNED_INT8:
            case CL_UNSIGNED_INT8:
                return 1;
            case CL_SNORM_INT16:
            case CL_UNORM_INT16:
            case CL_SIGNED_INT16:
            case CL_UNSIGNED_INT16:
            case CL_HALF_FLOAT:
                return 2;
            case CL_SIGNED_INT32:
            case CL_UNSIGNED_INT32:
            case CL_FLOAT:
                return 4;
            default:
                return 4; // conservative
            }
        }();

        // Guard against overflow
        if (width == 0 || height == 0 || depth == 0)
            return 0;
        double pixels =
          static_cast<double>(width) * static_cast<double>(height) * static_cast<double>(depth);
        double bpc = static_cast<double>(channels * bytesPerChannel);
        double total = pixels * bpc;
        if (total < 0 || total > static_cast<double>(std::numeric_limits<size_t>::max()))
            return std::numeric_limits<size_t>::max();
        return static_cast<size_t>(total);
    }

    std::unique_ptr<cl::Image2D>
    ComputeContext::createImage2DChecked(const cl::ImageFormat & format,
                                         size_t width,
                                         size_t height,
                                         cl_mem_flags flags,
                                         size_t rowPitch,
                                         void * hostPtr,
                                         const char * debugTag)
    {
        const size_t estBytes = estimateImageSizeBytes(format, width, height, 1);
        // Reuse buffer-checked logic for limits using estimated bytes
        if (estBytes == 0)
        {
            throw GladiusException("Image2D size is zero");
        }
        if (m_deviceMaxAllocBytes != 0 && estBytes > m_deviceMaxAllocBytes)
        {
            auto msg = fmt::format("Image2D exceeds device max: {} MB > {} MB{}",
                                   estBytes / (1024 * 1024),
                                   m_deviceMaxAllocBytes / (1024 * 1024),
                                   debugTag ? fmt::format(" [{}]", debugTag) : std::string());
            if (m_logger)
                m_logger->logError(msg);
            throw GladiusException(msg);
        }
        // Enforce total budget based on accounting
        if (m_deviceGlobalMemBytes != 0)
        {
            size_t cap = static_cast<size_t>(m_deviceGlobalMemBytes * kTotalMemSafetyUtilization);
            size_t used = m_trackedAllocatedBytes.load(std::memory_order_relaxed);
            if (used + estBytes > cap)
            {
                auto msg = fmt::format(
                  "Image2D allocation exceeds safety cap: need {} MB, used {} MB, cap {} MB{}",
                  estBytes / (1024 * 1024),
                  used / (1024 * 1024),
                  cap / (1024 * 1024),
                  debugTag ? fmt::format(" [{}]", debugTag) : std::string());
                if (m_logger)
                    m_logger->logError(msg);
                throw GladiusException(msg);
            }
        }

        cl_int err = CL_SUCCESS;
        std::unique_ptr<cl::Image2D> img;
        try
        {
            img = std::make_unique<cl::Image2D>(
              *m_context, flags, format, width, height, rowPitch, hostPtr, &err);
            checkError(err, "createImage2DChecked: cl::Image2D ctor");
        }
        catch (const std::exception & e)
        {
            auto msg = std::string("Failed to allocate Image2D: ") + e.what();
            if (m_logger)
                m_logger->logError(msg);
            throw GladiusException(msg);
        }
        onBufferAllocated(estBytes);
        return img;
    }

    std::unique_ptr<cl::Image3D>
    ComputeContext::createImage3DChecked(const cl::ImageFormat & format,
                                         size_t width,
                                         size_t height,
                                         size_t depth,
                                         cl_mem_flags flags,
                                         size_t rowPitch,
                                         size_t slicePitch,
                                         void * hostPtr,
                                         const char * debugTag)
    {
        const size_t estBytes = estimateImageSizeBytes(format, width, height, depth);
        if (estBytes == 0)
        {
            throw GladiusException("Image3D size is zero");
        }
        if (m_deviceMaxAllocBytes != 0 && estBytes > m_deviceMaxAllocBytes)
        {
            auto msg = fmt::format("Image3D exceeds device max: {} MB > {} MB{}",
                                   estBytes / (1024 * 1024),
                                   m_deviceMaxAllocBytes / (1024 * 1024),
                                   debugTag ? fmt::format(" [{}]", debugTag) : std::string());
            if (m_logger)
                m_logger->logError(msg);
            throw GladiusException(msg);
        }
        if (m_deviceGlobalMemBytes != 0)
        {
            size_t cap = static_cast<size_t>(m_deviceGlobalMemBytes * kTotalMemSafetyUtilization);
            size_t used = m_trackedAllocatedBytes.load(std::memory_order_relaxed);
            if (used + estBytes > cap)
            {
                auto msg = fmt::format(
                  "Image3D allocation exceeds safety cap: need {} MB, used {} MB, cap {} MB{}",
                  estBytes / (1024 * 1024),
                  used / (1024 * 1024),
                  cap / (1024 * 1024),
                  debugTag ? fmt::format(" [{}]", debugTag) : std::string());
                if (m_logger)
                    m_logger->logError(msg);
                throw GladiusException(msg);
            }
        }

        cl_int err = CL_SUCCESS;
        std::unique_ptr<cl::Image3D> img;
        try
        {
            img = std::make_unique<cl::Image3D>(
              *m_context, flags, format, width, height, depth, rowPitch, slicePitch, hostPtr, &err);
            checkError(err, "createImage3DChecked: cl::Image3D ctor");
        }
        catch (const std::exception & e)
        {
            auto msg = std::string("Failed to allocate Image3D: ") + e.what();
            if (m_logger)
                m_logger->logError(msg);
            throw GladiusException(msg);
        }
        onBufferAllocated(estBytes);
        return img;
    }

    std::unique_ptr<cl::ImageGL> ComputeContext::createImageGLInteropChecked(GLenum target,
                                                                             GLint miplevel,
                                                                             GLuint texture,
                                                                             cl_mem_flags flags,
                                                                             const char * debugTag)
    {
        // Interop images alias GL memory; do not track as OpenCL allocation, but still create
        // safely
        cl_int err = CL_SUCCESS;
        std::unique_ptr<cl::ImageGL> img;
        try
        {
            img = std::make_unique<cl::ImageGL>(*m_context, flags, target, miplevel, texture, &err);
            checkError(err, "createImageGLInteropChecked: cl::ImageGL ctor");
        }
        catch (const std::exception & e)
        {
            auto msg = std::string("Failed to create ImageGL interop: ") + e.what();
            if (m_logger)
                m_logger->logError(msg);
            throw GladiusException(msg);
        }
        if (m_logger && m_debugOutputEnabled)
        {
            m_logger->logInfo(fmt::format("Created GL interop image (target={}, tag={})",
                                          static_cast<int>(target),
                                          debugTag ? debugTag : "-"));
        }
        return img;
    }

}
