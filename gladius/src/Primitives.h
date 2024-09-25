#pragma once

#include "Buffer.h"
#include "kernel/types.h"
#include <cassert>

#include <fmt/format.h>

namespace gladius
{
    using PrimitiveMetaBuffer = Buffer<PrimitiveMeta>;
    using PrimitiveDataBuffer = Buffer<PrimitiveData>;

    using MetaContainer = std::vector<PrimitiveMeta>;
    using DataContainer = std::vector<PrimitiveData>;

    struct PrimitiveBuffer
    {
        MetaContainer meta;
        DataContainer data;
    };

    class Primitives
    {
      public:
        explicit Primitives(ComputeContext & context)
            : primitives(context)
            , data(context)
        {
        }

        void write()
        {
            primitives.write();
            data.write();
        }

        void read()
        {
            primitives.read();
            data.read();
        }

        void create()
        {
            primitives.create();
            data.create();
        }

        void clear()
        {
            primitives.getData().clear();
            data.getData().clear();
        }

        void add(PrimitiveBuffer & source)
        {

            auto currentOffset = data.getSize();
            for (const auto & meta : source.meta)
            {
                auto newMeta = meta;
                newMeta.start = meta.start + static_cast<int>(currentOffset);
                auto size = meta.end - meta.start;
                if (size < 0)
                {
                    std::cerr << "Primitives::add: size < 0: " << size << std::endl;
                    throw std::runtime_error(fmt::format("Primitives::add: size < 0: {}", size));
                }
                newMeta.end = newMeta.start + size;

                primitives.getData().push_back(newMeta);
            }

            data.getData().insert(data.getData().end(), source.data.begin(), source.data.end());
        }

        // private:
        PrimitiveMetaBuffer primitives;
        PrimitiveDataBuffer data;
    };
}
