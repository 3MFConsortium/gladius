#pragma once

#include <stdexcept>
#include <string>
#include <thread>

namespace gladius
{
    std::string getOpenCLErrorName(int error);
    std::string getOpenCLErrorDescription(int error);

    class GladiusException : public std::runtime_error
    {
      public:
        explicit GladiusException(std::string msg);
    };

    class OpenCLError : public GladiusException
    {
      public:
        explicit OpenCLError(int ocl_error);
    };

    class NoSuitableOpenCLDevicesFound : public GladiusException
    {
      public:
        NoSuitableOpenCLDevicesFound()
            : GladiusException(
                "Could not find any suitable OpenCL device. Please check if an OpenCL device (e.g. "
                "GPU) is "
                "installed as well as proper drivers for this device. Some devices may need an "
                "additional OpenCL runtime. If you are running Gladius inside a virtual machine "
                "lacking a GPU you may try to install a CPU based OpenCL runtime. Gladius requires "
                "OpenCL 1.2 or higher, with fp64 support.")
        {
        }
    };

    class NoValidBinaryStlFile : public GladiusException
    {
      public:
        explicit NoValidBinaryStlFile(std::string const & message)
            : GladiusException("Invalid STL file: " + message)
        {
        }
    };

    class FileIOError : public GladiusException
    {
      public:
        explicit FileIOError(std::string const & message)
            : GladiusException("File I/O error: " + message)
        {
        }
    };

    /// @brief Exception thrown when OpenCL context creation fails
    class OpenCLContextCreationError : public GladiusException
    {
      public:
        explicit OpenCLContextCreationError(std::string const & details)
            : GladiusException("Failed to create OpenCL context: " + details)
        {
        }
    };

    /// @brief Exception thrown when OpenCL command queue creation fails
    class OpenCLQueueCreationError : public GladiusException
    {
      public:
        explicit OpenCLQueueCreationError(std::string const & details, std::thread::id threadId)
            : GladiusException("Failed to create OpenCL command queue for thread " +
                               std::to_string(std::hash<std::thread::id>{}(threadId)) + ": " +
                               details)
        {
        }
    };

    /// @brief Exception thrown when OpenCL device capabilities cannot be queried
    class OpenCLDeviceQueryError : public GladiusException
    {
      public:
        explicit OpenCLDeviceQueryError(std::string const & deviceInfo, std::string const & details)
            : GladiusException("Failed to query OpenCL device '" + deviceInfo + "': " + details)
        {
        }
    };

    /// @brief Exception thrown when OpenCL platform enumeration fails
    class OpenCLPlatformError : public GladiusException
    {
      public:
        explicit OpenCLPlatformError(std::string const & details)
            : GladiusException("OpenCL platform error: " + details)
        {
        }
    };

    /// @brief Exception thrown when OpenGL-OpenCL interop fails
    class OpenGLInteropError : public GladiusException
    {
      public:
        explicit OpenGLInteropError(std::string const & details)
            : GladiusException("OpenGL-OpenCL interoperability error: " + details)
        {
        }
    };

    /// @brief Exception thrown when OpenCL version parsing fails
    class OpenCLVersionParseError : public GladiusException
    {
      public:
        explicit OpenCLVersionParseError(std::string const & versionString,
                                         std::string const & details)
            : GladiusException("Failed to parse OpenCL version '" + versionString + "': " + details)
        {
        }
    };

    /// @brief Exception thrown when thread-local queue management fails
    class ThreadQueueManagementError : public GladiusException
    {
      public:
        explicit ThreadQueueManagementError(std::string const & operation, std::thread::id threadId)
            : GladiusException("Thread queue management error during " + operation +
                               " for thread " +
                               std::to_string(std::hash<std::thread::id>{}(threadId)))
        {
        }
    };
}
