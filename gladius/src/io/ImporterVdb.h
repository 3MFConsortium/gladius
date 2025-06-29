#pragma once
#include "Parameter.h"
#include "Primitives.h"
#include <filesystem>

namespace gladius
{
    class Document;
}

namespace gladius::nodes
{
    class Builder;
}

namespace gladius::io
{
    class ImporterVdb
    {
      public:
        void load(std::filesystem::path const & filename, Document & doc);
    };

    void loadFromOpenVdbFile(std::filesystem::path const filename, Document & doc);
}