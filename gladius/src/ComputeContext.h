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

        [[nodiscard]] const cl::Context & GetContext() const;

        [[nodiscard]] const cl::CommandQueue & GetQueue();

        [[nodiscard]] bool isValid() const;

        [[nodiscard]] const cl::Device & GetDevice() const;

        [[nodiscard]] OutputMethod outputMethod() const;
        void setOutputMethod(const OutputMethod & outputMethod);

        void invalidate();

        /// @brief Invalidate context with reason for debugging
        /// @param reason The reason for invalidation
        void invalidate(const std::string & reason);

        /// @brief Enable or disable OpenCL debug output
        /// @param enabled true to enable debug output, false to disable
        static void setDebugOutputEnabled(bool enabled)
        {
            s_enableDebugOutput = enabled;
        }

        /// @brief Check if OpenCL debug output is enabled
        /// @return true if debug output is enabled, false otherwise
        [[nodiscard]] static bool isDebugOutputEnabled()
        {
            return s_enableDebugOutput;
        }

        /// @brief Validate that a command queue is still valid for use
        /// @param queue The queue to validate
        /// @return true if the queue is valid, false otherwise
        bool validateQueue(const cl::CommandQueue & queue) const;

        /// @brief Get diagnostic information about the compute context state
        /// @return String with detailed context information
        std::string getDiagnosticInfo() const;

        /// @brief Perform a comprehensive validation check before critical operations
        /// @param operationName Name of the operation being performed (for logging)
        /// @return true if everything appears valid, false otherwise
        bool validateForOperation(const std::string & operationName) const;

        /// @brief Validate OpenCL memory objects and buffers for corruption
        /// @param operationName Name of the operation being performed
        /// @param buffers List of OpenCL memory objects to validate
        /// @return true if all buffers appear valid, false otherwise
        bool validateBuffers(const std::string & operationName,
                             const std::vector<cl::Memory> & buffers = {}) const;

        /// @brief Check for potential memory layout conflicts between BVH and NanoVDB data
        /// @param operationName Name of the operation being performed
        void checkMemoryLayoutConflicts(const std::string & operationName) const;

      private:
        cl::CommandQueue createQueue() const;
        void initContext();

        std::unique_ptr<cl::Context> m_context;

        // queue per thread
        QueuePerThread m_queues;
        mutable std::mutex m_queuesMutex;

        cl::Device m_device;

        bool m_isValid = false;
        EnableGLOutput m_outputGL = EnableGLOutput::disabled;
        OutputMethod m_outputMethod = OutputMethod::readpixel;

        // Debug tracking
        mutable std::atomic<size_t> m_invalidationCount{0};

        // Debug output control
        static inline bool s_enableDebugOutput = false;
    };

    using SharedComputeContext = std::shared_ptr<ComputeContext>;
}
