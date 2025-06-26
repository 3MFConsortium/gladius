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
        
        /**
         * @brief Renders a dropdown for selecting a VolumeData resource for a mesh
         * 
         * @param document The document containing the resources
         * @param model3mf The 3MF model containing the resources
         * @param mesh The mesh object to associate with a VolumeData resource
         * 
         * @return true if the VolumeData association was modified
         */
        bool renderVolumeDataDropdown(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            std::shared_ptr<Lib3MF::CMeshObject> mesh) const;
    };
}