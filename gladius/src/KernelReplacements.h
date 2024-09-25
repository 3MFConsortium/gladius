#pragma once
#include <memory>
#include <unordered_map>

namespace gladius
{
    using KernelReplacements = std::unordered_map<std::string, std::string>;
    using SharedKernelReplacements = std::shared_ptr<KernelReplacements>;
}