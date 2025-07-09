#pragma once

#include "gpgpu.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#define STRINGIFY(x) #x
#define EXPAND_AND_STRINGIFY(x) STRINGIFY(x)
#define LOCATION __FILE__ " : " EXPAND_AND_STRINGIFY(__LINE__)
#define CL_ERROR(X) gladius::checkError(X, LOCATION)

namespace gladius
{
    enum class EnableGLOutput
    {
        enabled,
        disabled
    };

    enum class OutputMethod
    {
        disabled,
        interop,
        readpixel
    };

    void checkError(cl_int err, const std::string & description);

    using OpenCLVersion = double;

    using QueuePerThread = std::unordered_map<std::thread::id, cl::CommandQueue>;

    struct Capabilities
    {
        bool fp64{false};
        bool correctlyRoundedDivedSqrt{false};
        bool cpu{false};
        bool gpu{false};
        double performanceEstimation{}; // very rough estimation: number of compute units times max
                                        // clock frequency
        OpenCLVersion openCLVersion{1.0};
    };

    struct Accelerator
    {
        cl::Device device;
        cl::Platform platform;
        Capabilities capabilities{};
    };
    using AcceleratorList = std::vector<Accelerator>;

    Capabilities queryCapabilities(cl::Device const & device);
    AcceleratorList queryAccelerators(std::ostream & logStream);
    OpenCLVersion getOpenCLVersion(cl::Device const & device);

    class ComputeContext
    {
      public:
        ComputeContext();

        explicit ComputeContext(EnableGLOutput enableOutput);

        /// @brief Check if OpenCL acceleration is available on the system
        /// @return true if OpenCL is available and at least one suitable device is found, false
        /// otherwise
        [[nodiscard]] static bool isOpenCLAvailable();

        [[nodiscard]] const cl::Context & GetContext() const;

        [[nodiscard]] const cl::CommandQueue & GetQueue();

        [[nodiscard]] bool isValid() const;

        [[nodiscard]] const cl::Device & GetDevice() const;

        [[nodiscard]] OutputMethod outputMethod() const;
        void setOutputMethod(const OutputMethod & outputMethod);

        void invalidate();

      private:
        cl::CommandQueue createQueue() const;
        void initContext();

        std::unique_ptr<cl::Context> m_context;

        // queue per thread
        QueuePerThread m_queues;
        std::mutex m_queuesMutex;

        cl::Device m_device;

        bool m_isValid = false;
        EnableGLOutput m_outputGL = EnableGLOutput::disabled;
        OutputMethod m_outputMethod = OutputMethod::readpixel;
    };

    using SharedComputeContext = std::shared_ptr<ComputeContext>;
}
