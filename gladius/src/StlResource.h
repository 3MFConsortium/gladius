#pragma once

#include "ResourceManager.h"
#include "io/VdbImporter.h"

namespace gladius
{
    class StlResource : public ResourceBase
    {
      public:
        StlResource(ResourceKey filename)
            : ResourceBase(std::move(filename))
        {
            load();
        }

      private:
        void loadImpl() override;
    };
}