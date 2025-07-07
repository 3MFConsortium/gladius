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
            caps.fp64 = extensions.find(fp64ExtensionName) != std::string::npos;

            auto const deviceType = device.getInfo<CL_DEVICE_TYPE>();
            caps.cpu = (deviceType == CL_DEVICE_TYPE_CPU);
            caps.gpu = (deviceType == CL_DEVICE_TYPE_GPU);

            auto const maxClock = device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
            auto const computeUnits = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();

            auto const vendor = device.getInfo<CL_DEVICE_VENDOR>();

            bool const isIntel = (vendor.rfind("Intel", 0) == 0);

            auto const vendorRating = (isIntel) ? 0.01 : 1.;
            auto const deviceTypeRating = caps.cpu ? 0.1 : 1.;
            caps.performanceEstimation = maxClock * computeUnits * vendorRating * deviceTypeRating;
        }
        catch (std::exception const & e)
        {
            std::cerr << "Warning: Failed to query device capabilities: " << e.what() << "\n";
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
                return {};
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
                    logStream << "\tWarning: Failed to query platform " << i + 1 << ": " << e.what()
                              << "\n";
                }
            }
        }
        catch (std::exception const & e)
        {
            logStream << "Error querying OpenCL platforms: " << e.what() << "\n";
        }

        return candidates;
    }

    OpenCLVersion getOpenCLVersion(cl::Device const & device)
    {
        try
        {
            std::string const openCLVersionStr{device.getInfo<CL_DEVICE_OPENCL_C_VERSION>()};

            // Expected format: "OpenCL C 1.2" or "OpenCL C 2.0", etc.
            auto constexpr prefix = "OpenCL C ";
            auto constexpr prefixLength = 9;

            if (openCLVersionStr.length() < prefixLength + 3) // Minimum: "OpenCL C 1.0"
            {
                std::cerr << "Unexpected OpenCL version string format: " << openCLVersionStr
                          << "\n";
                return 1.0; // Default fallback
            }

            auto const numberStr = openCLVersionStr.substr(prefixLength);

            // Find the space or end to get just the version number
            auto const spacePos = numberStr.find(' ');
            auto const versionStr =
              (spacePos != std::string::npos) ? numberStr.substr(0, spacePos) : numberStr;

            OpenCLVersion const version{std::stod(versionStr)};
            return version;
        }
        catch (std::exception const & e)
        {
            std::cerr << "Failed to parse OpenCL version: " << e.what() << "\n";
            return 1.0; // Default fallback
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
        return *m_context;
    }

    const cl::CommandQueue & ComputeContext::GetQueue()
    {
        std::lock_guard<std::mutex> lock(m_queuesMutex);
        auto iter = m_queues.find(std::this_thread::get_id());
        if (iter == m_queues.end())
        {
            try
            {
                m_queues.emplace(std::this_thread::get_id(), createQueue());
                return m_queues[std::this_thread::get_id()];
            }
            catch (std::exception const & e)
            {
                std::cerr << "Failed to create command queue for thread: " << e.what() << "\n";
                throw;
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
        return m_device;
    }

    void ComputeContext::invalidate()
    {
        m_isValid = false;
    }

    cl::CommandQueue ComputeContext::createQueue() const
    {
        cl_int err;
        auto queue = cl::CommandQueue(GetContext(), m_device, 0, &err);
        checkError(err, "Failed to create command queue");
        return queue;
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
                std::cout << " No platforms found. Check OpenCL installation!\n";
                exit(1);
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
        catch (std::exception const & e)
        {
            std::cerr << "Failed to initialize OpenCL platform/device: " << e.what() << "\n";
            throw NoSuitableOpenCLDevicesFound();
        }

        if (m_outputGL == EnableGLOutput::disabled)
        {
            m_context = std::make_unique<cl::Context>(cl::Context({m_device}));
            m_outputMethod = OutputMethod::disabled;
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
                        std::cerr << "No active OpenGL context found for Windows interop\n";
                        m_outputMethod = OutputMethod::readpixel;
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
                        std::cerr << "No active OpenGL context found for Linux interop\n";
                        m_outputMethod = OutputMethod::readpixel;
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
                catch (std::exception const & e)
                {
                    std::cerr << "Failed to initialize interop mode: " << e.what() << "\n";
                    m_outputMethod = OutputMethod::readpixel;
                    err = CL_INVALID_CONTEXT; // Ensure fallback is triggered
                }
            }
            if (err == CL_SUCCESS)
            {
                std::cout << "Enabling opengl sharing using interop method\n";
            }
            else
            {
                m_context = std::make_unique<cl::Context>(cl::Context({m_device}));
                m_outputMethod = OutputMethod::readpixel;
                std::cout << "Enabling opengl sharing using the readpixel method\n";
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
