#include "ComputeContext.h"

#include "Profiling.h"
#include "exceptions.h"

#include <fmt/format.h>

#include <iostream>
#include <set>
#include <sstream>
#include <utility>

namespace gladius
{
    void checkError(cl_int err, const std::string & description)
    {
        if (err != CL_SUCCESS)
        {
            auto const threadId = std::this_thread::get_id();
            auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));

            std::cerr << fmt::format("OpenCL error: {} ({}): {} [Thread: {}]\n",
                                     description,
                                     err,
                                     getOpenCLErrorDescription(err),
                                     threadIdStr);
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
            std::cerr << fmt::format(
              "[GetQueue] {}: Thread={}, ContextValid={}, NumQueues={}, ContextPtr={}\n",
              stage,
              threadIdStr,
              m_isValid,
              m_queues.size(),
              static_cast<void *>(m_context.get()));
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
                                       "  Total Invalidations: {}\n",
                                       threadIdStr,
                                       m_isValid,
                                       static_cast<void *>(m_context.get()),
                                       m_queues.size(),
                                       static_cast<int>(m_outputMethod),
                                       m_invalidationCount.load());

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

        std::string logMsg =
          fmt::format("[ComputeContext::validateForOperation] Operation='{}', Thread={}, Valid={}",
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

        return allValid;
    }

    bool ComputeContext::validateBuffers(const std::string & operationName,
                                         const std::vector<cl::Memory> & buffers) const
    {
        if (!s_enableDebugOutput)
        {
            return true; // Skip validation when debug output is disabled
        }

        auto const threadId = std::this_thread::get_id();
        auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));

        bool allValid = true;
        std::string issues;

        std::cerr << fmt::format(
          "[ComputeContext::validateBuffers] Operation='{}', Thread={}, BufferCount={}\n",
          operationName,
          threadIdStr,
          buffers.size());

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

                if (s_enableDebugOutput)
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
                if (s_enableDebugOutput)
                {
                    std::cerr << fmt::format("  Buffer[{}]: CORRUPTED - {}\n", i, e.what());
                }
            }
        }

        if (s_enableDebugOutput)
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
        if (!s_enableDebugOutput)
        {
            return; // Skip memory layout checks when debug output is disabled
        }

        auto const threadId = std::this_thread::get_id();
        auto const threadIdStr = std::to_string(std::hash<std::thread::id>{}(threadId));

        std::cerr << fmt::format(
          "[ComputeContext::checkMemoryLayoutConflicts] Operation='{}', Thread={}\n",
          operationName,
          threadIdStr);

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

                std::cerr << fmt::format(
                  "  Device Memory: Global={}MB, MaxAlloc={}MB, Local={}KB\n",
                  globalMemSize / (1024 * 1024),
                  maxMemAlloc / (1024 * 1024),
                  localMemSize / 1024);
            }

            // Check for OpenCL context memory issues
            try
            {
                // Try to allocate a small test buffer to verify context health
                cl::Buffer testBuffer(*m_context, CL_MEM_READ_WRITE, 1024);
                auto size = testBuffer.getInfo<CL_MEM_SIZE>();
                std::cerr << fmt::format("  Test buffer allocation successful: {}B\n", size);
            }
            catch (const std::exception & e)
            {
                std::cerr << fmt::format("  WARNING: Test buffer allocation failed: {}\n",
                                         e.what());
            }
        }
        catch (const std::exception & e)
        {
            std::cerr << fmt::format(
              "[ComputeContext::checkMemoryLayoutConflicts] Error getting memory info: {}\n",
              e.what());
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

                        if (currentContext == nullptr || currentDC == nullptr)
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
                }
                else
                {
                    m_context = std::make_unique<cl::Context>(cl::Context({m_device}));
                    m_outputMethod = OutputMethod::readpixel;
                    // OpenGL sharing enabled using the readpixel method
                }
            }

            m_isValid = true;
        }
        catch (std::exception const & e)
        {
            // Handle any initialization errors
            m_isValid = false;
            throw;
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

}
