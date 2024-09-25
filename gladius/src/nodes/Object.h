#pragma once

#include "Components.h"

#include <string>

namespace gladius::nodes
{

    class Object
    {
      public:
      private:
        Components m_components;
        std::string m_name;
    };

} // namespace gladius::nodes