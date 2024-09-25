#include "exceptions.h"
#include <fmt/format.h>

namespace gladius
{
    GladiusException::GladiusException(std::string msg)
        : std::runtime_error(msg)
    {
    }

    OpenCLError::OpenCLError(int ocl_error)
        : GladiusException(fmt::format("{} (OpenCL error code {} = {})",
                                       getOpenCLErrorDescription(ocl_error),
                                       ocl_error,
                                       getOpenCLErrorName(ocl_error)))
    {
    }

    std::string getOpenCLErrorName(int error)
    {
        switch (error)
        {
        // run-time and JIT compiler errors
        case 0:
            return "CL_SUCCESS";

        case -1:
            return "CL_DEVICE_NOT_FOUND";

        case -2:
            return "CL_DEVICE_NOT_AVAILABLE";

        case -3:
            return "CL_COMPILER_NOT_AVAILABLE";

        case -4:
            return "CL_MEM_OBJECT_ALLOCATION_FAILURE";

        case -5:
            return "CL_OUT_OF_RESOURCES";

        case -6:
            return "CL_OUT_OF_HOST_MEMORY";

        case -7:
            return "CL_PROFILING_INFO_NOT_AVAILABLE";

        case -8:
            return "CL_MEM_COPY_OVERLAP";

        case -9:
            return "CL_IMAGE_FORMAT_MISMATCH";

        case -10:
            return "CL_IMAGE_FORMAT_NOT_SUPPORTED";

        case -11:
            return "CL_BUILD_PROGRAM_FAILURE";

        case -12:
            return "CL_MAP_FAILURE";

        case -13:
            return "CL_MISALIGNED_SUB_BUFFER_OFFSET";

        case -14:
            return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";

        case -15:
            return "CL_COMPILE_PROGRAM_FAILURE";

        case -16:
            return "CL_LINKER_NOT_AVAILABLE";

        case -17:
            return "CL_LINK_PROGRAM_FAILURE";

        case -18:
            return "CL_DEVICE_PARTITION_FAILED";

        case -19:
            return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

        // compile-time errors
        case -30:
            return "CL_INVALID_VALUE";

        case -31:
            return "CL_INVALID_DEVICE_TYPE";

        case -32:
            return "CL_INVALID_PLATFORM";

        case -33:
            return "CL_INVALID_DEVICE";

        case -34:
            return "CL_INVALID_CONTEXT";

        case -35:
            return "CL_INVALID_QUEUE_PROPERTIES";

        case -36:
            return "CL_INVALID_COMMAND_QUEUE";

        case -37:
            return "CL_INVALID_HOST_PTR";

        case -38:
            return "CL_INVALID_MEM_OBJECT";

        case -39:
            return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";

        case -40:
            return "CL_INVALID_IMAGE_SIZE";

        case -41:
            return "CL_INVALID_SAMPLER";

        case -42:
            return "CL_INVALID_BINARY";

        case -43:
            return "CL_INVALID_BUILD_OPTIONS";

        case -44:
            return "CL_INVALID_PROGRAM";

        case -45:
            return "CL_INVALID_PROGRAM_EXECUTABLE";

        case -46:
            return "CL_INVALID_KERNEL_NAME";

        case -47:
            return "CL_INVALID_KERNEL_DEFINITION";

        case -48:
            return "CL_INVALID_KERNEL";

        case -49:
            return "CL_INVALID_ARG_INDEX";

        case -50:
            return "CL_INVALID_ARG_VALUE";

        case -51:
            return "CL_INVALID_ARG_SIZE";

        case -52:
            return "CL_INVALID_KERNEL_ARGS";

        case -53:
            return "CL_INVALID_WORK_DIMENSION";

        case -54:
            return "CL_INVALID_WORK_GROUP_SIZE";

        case -55:
            return "CL_INVALID_WORK_ITEM_SIZE";

        case -56:
            return "CL_INVALID_GLOBAL_OFFSET";

        case -57:
            return "CL_INVALID_EVENT_WAIT_LIST";

        case -58:
            return "CL_INVALID_EVENT";

        case -59:
            return "CL_INVALID_OPERATION";

        case -60:
            return "CL_INVALID_GL_OBJECT";

        case -61:
            return "CL_INVALID_BUFFER_SIZE";

        case -62:
            return "CL_INVALID_MIP_LEVEL";

        case -63:
            return "CL_INVALID_GLOBAL_WORK_SIZE";

        case -64:
            return "CL_INVALID_PROPERTY";

        case -65:
            return "CL_INVALID_IMAGE_DESCRIPTOR";

        case -66:
            return "CL_INVALID_COMPILER_OPTIONS";

        case -67:
            return "CL_INVALID_LINKER_OPTIONS";

        case -68:
            return "CL_INVALID_DEVICE_PARTITION_COUNT";

        // extension errors
        case -1000:
            return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";

        case -1001:
            return "CL_PLATFORM_NOT_FOUND_KHR";

        case -1002:
            return "CL_INVALID_D3D10_DEVICE_KHR";

        case -1003:
            return "CL_INVALID_D3D10_RESOURCE_KHR";

        case -1004:
            return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";

        case -1005:
            return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";

        default:
            return "Unknown OpenCL error";
        }
    }

    std::string getOpenCLErrorDescription(int error)
    {
        switch (error)
        {
        // run-time and JIT compiler errors
        case 0:
            return "CL_SUCCESS";

        case -1:
            return "The requested OpenCL device could not be found. Probably a device previously "
                   "selected is not available anymore.";

        case -2:
            return "The OpenCL device is not available. This could probably be an issue with the "
                   "driver. You might try rebooting your computer.";

        case -3:
            return "The OpenCL runtime does not provide an OpenCL compiler. You may select another "
                   "OpenCL device or try the latest driver for your OpenCL device. You may also "
                   "look for a specific OpenCL runtime provided by the device vendor.";

        case -4:
            return "Memory allocation on the OpenCL device failed. This might be either a bug or "
                   "you are requesting an operation that needs to much memory. Using an OpenCL "
                   "device with more memory might help as well as trying to make the model less "
                   "demanding.";

        case -5:
            return "The OpenCL driver reports that the device is out of resources. Depending on "
                   "the vendor this can mean almost anything.";

        case -6:
            return "The operation requires to much system memory.";

        case -7:
            return "OpenCL profiling is not avilable";

        case -8:
            return "An image copy operation trys to copy something to an area that is also part of "
                   "the source. If this happens, it is most likely a bug";

        case -9:
            return "There is mismatch in the image formats (e.g. of source and destination).";

        case -10:
            return "The required image format is not supported by the selected OpenCL device";

        case -11:
            return "The compilation of an OpenCL program failed. This might happen if the model is "
                   "invalid";

        case -12:
            return "CL_MAP_FAILURE";

        case -13:
            return "CL_MISALIGNED_SUB_BUFFER_OFFSET";

        case -14:
            return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";

        case -15:
            return "The compilation of an OpenCL program failed. This might happen if the model is "
                   "invalid";

        case -16:
            return "The OpenCL linker is not available."
                   "Updating to the latest OpenCL runtime or driver for your device might help. ";
        case -17:
            return "The OpenCL program failed to link. Please check if the model is valid. "
                   "Updating to the latest OpenCL runtime or driver for your device might also "
                   "help.";

        case -18:
            return "CL_DEVICE_PARTITION_FAILED";

        case -19:
            return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

        // compile-time errors
        case -30:
            return "CL_INVALID_VALUE";

        case -31:
            return "CL_INVALID_DEVICE_TYPE";

        case -32:
            return "CL_INVALID_PLATFORM";

        case -33:
            return "CL_INVALID_DEVICE";

        case -34:
            return "CL_INVALID_CONTEXT";

        case -35:
            return "CL_INVALID_QUEUE_PROPERTIES";

        case -36:
            return "CL_INVALID_COMMAND_QUEUE";

        case -37:
            return "CL_INVALID_HOST_PTR";

        case -38:
            return "CL_INVALID_MEM_OBJECT";

        case -39:
            return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";

        case -40:
            return "CL_INVALID_IMAGE_SIZE";

        case -41:
            return "CL_INVALID_SAMPLER";

        case -42:
            return "CL_INVALID_BINARY";

        case -43:
            return "CL_INVALID_BUILD_OPTIONS";

        case -44:
            return "CL_INVALID_PROGRAM";

        case -45:
            return "The OpenCL kernel program is invalid. This may happen if the model is somehow invalid. It might be necessary to restart Gladius to recover from this error.";

        case -46:
            return "CL_INVALID_KERNEL_NAME";

        case -47:
            return "CL_INVALID_KERNEL_DEFINITION";

        case -48:
            return "CL_INVALID_KERNEL";

        case -49:
            return "CL_INVALID_ARG_INDEX";

        case -50:
            return "CL_INVALID_ARG_VALUE";

        case -51:
            return "CL_INVALID_ARG_SIZE";

        case -52:
            return "CL_INVALID_KERNEL_ARGS";

        case -53:
            return "CL_INVALID_WORK_DIMENSION";

        case -54:
            return "CL_INVALID_WORK_GROUP_SIZE";

        case -55:
            return "CL_INVALID_WORK_ITEM_SIZE";

        case -56:
            return "CL_INVALID_GLOBAL_OFFSET";

        case -57:
            return "CL_INVALID_EVENT_WAIT_LIST";

        case -58:
            return "CL_INVALID_EVENT";

        case -59:
            return "CL_INVALID_OPERATION";

        case -60:
            return "CL_INVALID_GL_OBJECT";

        case -61:
            return "CL_INVALID_BUFFER_SIZE";

        case -62:
            return "CL_INVALID_MIP_LEVEL";

        case -63:
            return "CL_INVALID_GLOBAL_WORK_SIZE";

        case -64:
            return "CL_INVALID_PROPERTY";

        case -65:
            return "CL_INVALID_IMAGE_DESCRIPTOR";

        case -66:
            return "Invalid compiler options. Your OpenCL runtime might not be supported";

        case -67:
            return "CL_INVALID_LINKER_OPTIONS";

        case -68:
            return "CL_INVALID_DEVICE_PARTITION_COUNT";

        // extension errors
        case -1000:
            return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";

        case -1001:
            return "CL_PLATFORM_NOT_FOUND_KHR";

        case -1002:
            return "CL_INVALID_D3D10_DEVICE_KHR";

        case -1003:
            return "CL_INVALID_D3D10_RESOURCE_KHR";

        case -1004:
            return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";

        case -1005:
            return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";

        default:
            return "Unknown OpenCL error";
        }
    }
}