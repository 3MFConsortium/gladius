#pragma once

#include "gpgpu.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#include "EventLogger.h"

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

    // Global logger access for low-level error paths (e.g., checkError)
    void setGlobalLogger(events::SharedLogger logger);
    events::SharedLogger getGlobalLogger();

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

    // Note: no global logger or global debug flags are used.

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

        // Logger injection
        void setLogger(events::SharedLogger logger)
        {
            m_logger = std::move(logger);
        }
        [[nodiscard]] events::SharedLogger getLogger() const
        {
            return m_logger;
        }

        // Device memory capabilities
        [[nodiscard]] size_t getDeviceGlobalMemBytes() const
        {
            return m_deviceGlobalMemBytes;
        }
        [[nodiscard]] size_t getDeviceMaxAllocBytes() const
        {
            return m_deviceMaxAllocBytes;
        }

        // Best-effort free memory estimate (may use vendor extensions or internal accounting)
        [[nodiscard]] size_t getApproxFreeMemBytes() const;

        // Memory tracking hooks for buffer lifetime
        void onBufferAllocated(size_t bytes);
        void onBufferReleased(size_t bytes);

        // Centralized factory for OpenCL buffers with safety checks
        // Throws GladiusException on unreasonable requests or allocation failures
        std::unique_ptr<cl::Buffer> createBufferChecked(cl_mem_flags flags,
                                                        size_t bytes,
                                                        void * hostPtr = nullptr,
                                                        const char * debugTag = nullptr);

        // Centralized factories for OpenCL images with safety checks
        std::unique_ptr<cl::Image2D> createImage2DChecked(const cl::ImageFormat & format,
                                                          size_t width,
                                                          size_t height,
                                                          cl_mem_flags flags = CL_MEM_READ_WRITE,
                                                          size_t rowPitch = 0,
                                                          void * hostPtr = nullptr,
                                                          const char * debugTag = nullptr);

        std::unique_ptr<cl::Image3D> createImage3DChecked(const cl::ImageFormat & format,
                                                          size_t width,
                                                          size_t height,
                                                          size_t depth,
                                                          cl_mem_flags flags = CL_MEM_READ_WRITE,
                                                          size_t rowPitch = 0,
                                                          size_t slicePitch = 0,
                                                          void * hostPtr = nullptr,
                                                          const char * debugTag = nullptr);

        // Interop image from existing GL texture; does not count towards OpenCL allocation
        std::unique_ptr<cl::ImageGL>
        createImageGLInteropChecked(GLenum target,
                                    GLint miplevel,
                                    GLuint texture,
                                    cl_mem_flags flags = CL_MEM_READ_WRITE,
                                    const char * debugTag = nullptr);

        // Utility: estimate image size in bytes (used for safety checks and accounting)
        static size_t estimateImageSizeBytes(const cl::ImageFormat & format,
                                             size_t width,
                                             size_t height,
                                             size_t depth = 1);

        [[nodiscard]] OutputMethod outputMethod() const;
        void setOutputMethod(const OutputMethod & outputMethod);

        void invalidate();

        /// @brief Invalidate context with reason for debugging
        /// @param reason The reason for invalidation
        void invalidate(const std::string & reason);

        /// @brief Enable or disable OpenCL debug output (instance-level)
        void setDebugOutputEnabled(bool enabled)
        {
            m_debugOutputEnabled = enabled;
        }
        [[nodiscard]] bool isDebugOutputEnabled() const
        {
            return m_debugOutputEnabled;
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
        void queryDeviceMemoryCaps();
        bool tryQueryVendorFreeMem(size_t & freeBytesOut) const; // best-effort

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

        // Debug output control (instance-level)
        bool m_debugOutputEnabled{false};

        // Device memory capabilities (queried at init)
        size_t m_deviceGlobalMemBytes{0};
        size_t m_deviceMaxAllocBytes{0};

        // Runtime accounting of allocated OpenCL buffer bytes via our factory
        std::atomic<size_t> m_trackedAllocatedBytes{0};

        // Safety budgets (fractions of device memory; conservative defaults)
        static constexpr double kTotalMemSafetyUtilization = 0.85;    // leave headroom
        static constexpr double kSingleAllocSafetyUtilization = 0.95; // below driver hard cap

        // Event logger (optional)
        events::SharedLogger m_logger{};
    };

    using SharedComputeContext = std::shared_ptr<ComputeContext>;
}
