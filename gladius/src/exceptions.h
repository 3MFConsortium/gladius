#pragma once

#include <stdexcept>
#include <string>

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

    using NoValidBinaryStlFile = GladiusException;

    class FileIOError : public GladiusException
    {
      public:
        explicit FileIOError(std::string const& message)
            : GladiusException("File I/O error: " + message)
        {
        }
    };

}
