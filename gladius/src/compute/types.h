#pragma once

#include <mutex>
#include <optional>
#include <vector>

namespace gladius
{

    using ComputeToken = std::lock_guard<std::recursive_mutex>;
    using OptionalComputeToken = std::optional<ComputeToken>;
    enum class RequiredCapabilities
    {
        ComputeOnly,
        OpenGLInterop
    };

    struct PlainImage
    {
        std::vector<unsigned char> data;
        size_t width;
        size_t height;
    };

    enum class CodeGenerator
    {
        Code,
        CommandStream,
        Automatic
    };

}