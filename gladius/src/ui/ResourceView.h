#pragma once

#include "Document.h"

namespace gladius::ui
{

    class ResourceView
    {
      public:
        void render(SharedDocument document);

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
        bool renderVolumeDataDropdown(SharedDocument document,
                                      Lib3MF::PModel model3mf,
                                      std::shared_ptr<Lib3MF::CMeshObject> mesh) const;

        // Dialog state for custom box creation
        bool m_showCustomBoxDialog = false;
        float m_boxWidth = 400.0f;
        float m_boxHeight = 400.0f;
        float m_boxDepth = 400.0f;
        float m_boxStartX = 0.0f;
        float m_boxStartY = 0.0f;
        float m_boxStartZ = 0.0f;

        // Dialog state for STL to beam lattice import
        bool m_showStlToBeamLatticeDialog = false;
        std::string m_beamLatticeStlFilename;
        float m_beamDiameter = 1.0f;
    };
}