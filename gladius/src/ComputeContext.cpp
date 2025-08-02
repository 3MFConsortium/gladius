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
        caps.openCLVersion = getOpenCLVersion(device);

        auto const fp32Config = static_cast<cl_uint>(device.getInfo<CL_DEVICE_SINGLE_FP_CONFIG>());
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

        return caps;
    }

    AcceleratorList queryAccelerators(std::ostream & logStream)
    {
        AcceleratorList candidates;
        std::vector<cl::Platform> allPlatforms;
        cl::Platform::get(&allPlatforms);

        if (allPlatforms.empty())
        {
            return {};
        }

        for (unsigned int i = 0; i < allPlatforms.size(); i++)
        {
            logStream << "\nDevices of platform " << i + 1 << ") "
                      << allPlatforms[i].getInfo<CL_PLATFORM_NAME>() << ":\n";
            std::vector<cl::Device> allDevices;
            allPlatforms[i].getDevices(CL_DEVICE_TYPE_ALL, &allDevices);

            if (allDevices.empty())
            {
                logStream << "\tNo device found. \n";
            }

            for (auto & device : allDevices)
            {
                logStream << "\n\t" << device.getInfo<CL_DEVICE_NAME>() << "\n";
                auto caps = queryCapabilities(device);
                logStream << "Performance rating:" << caps.performanceEstimation << "\n";
                logStream << "Vendor:" << device.getInfo<CL_DEVICE_VENDOR>() << "\n";
                logStream << "Extensions:\n" << device.getInfo<CL_DEVICE_EXTENSIONS>() << "\n";

                if (caps.fp64) // fp64 is required by nanovdb
                {
                    auto accelerator = Accelerator{device, allPlatforms[i], std::move(caps)};
                    candidates.push_back(accelerator);
                }
            }
        }

        return candidates;
    }

    OpenCLVersion getOpenCLVersion(cl::Device const & device)
    {
        std::string const openCLVersionStr{device.getInfo<CL_DEVICE_OPENCL_C_VERSION>()};
        auto const numberStr = openCLVersionStr.substr(8, 4);
        OpenCLVersion const version{std::stod(numberStr)};
        return version;
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
            m_queues.emplace(std::this_thread::get_id(), createQueue());
            return m_queues[std::this_thread::get_id()];
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
        // get all platforms (drivers)
        std::vector<cl::Platform> allPlatforms;
        cl::Platform::get(&allPlatforms);

        if (allPlatforms.empty())
        {
            std::cerr << " No platforms found. Check OpenCL installation!\n";
            exit(1);
        }

        auto accelerators = queryAccelerators(std::cerr);

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
        cl::Platform defaultPlatform = accelerators.front().platform;

        std::cerr << "\n\n";
        std::cerr << "\nUsing device: " << m_device.getInfo<CL_DEVICE_NAME>() << "\n";

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
                    cl_context_properties configuration[] = {
                      CL_GL_CONTEXT_KHR,
                      reinterpret_cast<cl_context_properties>(wglGetCurrentContext()),
                      CL_WGL_HDC_KHR,
                      reinterpret_cast<cl_context_properties>(wglGetCurrentDC()),
                      CL_CONTEXT_PLATFORM,
                      reinterpret_cast<cl_context_properties>(defaultPlatform())};
#endif
#ifdef __linux__
                    cl_context_properties configuration[] = {
                      CL_GL_CONTEXT_KHR,
                      (cl_context_properties) glXGetCurrentContext(),
                      CL_GLX_DISPLAY_KHR,
                      (cl_context_properties) glXGetCurrentDisplay(),
                      CL_CONTEXT_PLATFORM,
                      (cl_context_properties) defaultPlatform(),
                      0};

#endif
                    m_context = std::make_unique<cl::Context>(
                      cl::Context({m_device, configuration, nullptr, nullptr, &err}));
                }
                catch (...)
                {
                    std::cerr << "Failed to initialize interop mode\n";
                    m_outputMethod = OutputMethod::readpixel;
                }
            }
            if (err == CL_SUCCESS)
            {
                std::cerr << "Enabling opengl sharing using interop method\n";
            }
            else
            {
                m_context = std::make_unique<cl::Context>(cl::Context({m_device}));
                m_outputMethod = OutputMethod::readpixel;
                std::cerr << "Enabling opengl sharing using the readpixel method\n";
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
