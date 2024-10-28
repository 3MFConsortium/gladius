#pragma once

#include "Document.h"

namespace gladius::ui
{
    class ResourceView
    {
      public:
        void render(SharedDocument document) const;

      private:
       void addMesh(SharedDocument document) const;
       void addBoundingBox(SharedDocument document);
    };
}