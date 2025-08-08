#include "ComputeContext.h"

#include "Profiling.h"
#include "exceptions.h"

#include <fmt/format.h>

#include <iostream>
#include <sstream>
#include <utility>

namespace gladius
{
    void checkError(cl_int err, const std::string & description)
    {
        if (err != CL_SUCCESS)
        {
            std::cerr << fmt::format(
              "OpenCL error: {} ({}): {}\n", description, err, getOpenCLErrorDescription(err));
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

        // Check if context is valid before attempting to create queue
        if (!m_isValid)
        {
            throw OpenCLQueueCreationError("ComputeContext is not valid", currentThreadId);
        }

        if (!m_context)
        {
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
                    throw ThreadQueueManagementError("queue insertion", currentThreadId);
                }
                return result.first->second;
            }
            catch (OpenCLQueueCreationError const &)
            {
                // Re-throw our specific queue creation errors
                throw;
            }
            catch (ThreadQueueManagementError const &)
            {
                // Re-throw thread management errors
                throw;
            }
            catch (OpenCLError const & e)
            {
                throw OpenCLQueueCreationError(
                  "OpenCL error during queue creation: " + std::string(e.what()), currentThreadId);
            }
            catch (std::exception const & e)
            {
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
        m_isValid = false;
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

            auto accelerators = queryAccelerators(std::cout);

            if (accelerators.empty())
            {
                throw NoSuitableOpenCLDevicesFound();
            }

            std::stable_sort(std::begin(accelerators),
                             std::end(accelerators),
                             [](Accelerator const & lhs, Accelerator const & rhs) {
                                 return lhs.capabilities.performanceEstimation >
                                        rhs.capabilities.performanceEstimation;
                             });

            m_device = accelerators.front().device;
            defaultPlatform = accelerators.front().platform;

            std::cout << "\n\n";
            std::cout << "\nUsing device: " << m_device.getInfo<CL_DEVICE_NAME>() << "\n";
        }
        catch (NoSuitableOpenCLDevicesFound const &)
        {
            // Re-throw this specific exception as it's expected by callers
            throw;
        }
        catch (OpenCLPlatformError const &)
        {
            // Re-throw platform errors
            throw;
        }
        catch (OpenCLDeviceQueryError const & e)
        {
            std::cerr << "Device query error: " << e.what() << "\n";
            throw NoSuitableOpenCLDevicesFound();
        }
        catch (std::exception const & e)
        {
            throw OpenCLPlatformError("Failed to initialize OpenCL platform/device: " +
                                      std::string(e.what()));
        }

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

            try
            {
                if (err == CL_SUCCESS)
                {
                    std::cout << "Enabling opengl sharing using interop method\n";
                }
                else
                {
                    throw OpenCLContextCreationError("OpenCL context creation failed with error " +
                                                     std::to_string(err));
                }
            }
            catch (OpenGLInteropError const & e)
            {
                std::cerr << "OpenGL interop failed: " << e.what() << "\n";
                std::cerr << "Falling back to readpixel method\n";
                m_outputMethod = OutputMethod::readpixel;
                err = CL_INVALID_CONTEXT; // Trigger fallback
            }
            catch (OpenCLContextCreationError const & e)
            {
                std::cerr << "OpenCL context creation failed: " << e.what() << "\n";
                std::cerr << "Falling back to readpixel method\n";
                m_outputMethod = OutputMethod::readpixel;
                err = CL_INVALID_CONTEXT; // Trigger fallback
            }

            if (err != CL_SUCCESS)
            {
                try
                {
                    m_context = std::make_unique<cl::Context>(cl::Context({m_device}));
                    m_outputMethod = OutputMethod::readpixel;
                    std::cout << "Enabling opengl sharing using the readpixel method\n";
                }
                catch (std::exception const & e)
                {
                    throw OpenCLContextCreationError("Failed to create fallback OpenCL context: " +
                                                     std::string(e.what()));
                }
            }
        }

        m_isValid = true;
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
