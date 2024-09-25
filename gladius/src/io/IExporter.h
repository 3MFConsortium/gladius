#pragma once
#include <filesystem>

namespace gladius
{
    class ComputeCore;
}
namespace gladius::io
{
    class IExporter
    {
      public:
        virtual ~IExporter() = default;

        virtual void beginExport(const std::filesystem::path & fileName,
                                 ComputeCore & generator) = 0;

        virtual bool advanceExport(ComputeCore & generator) = 0;

        virtual void finalize() = 0;

        [[nodiscard]] virtual double getProgress() const = 0;
    };
}