#pragma once
#include "types.h"


namespace gladius::nodes
{
    class Component
    {
      private:
      public:
        ResourceId id {};
        Matrix4x4 transform;
    };
} // namespace gladius::nodes