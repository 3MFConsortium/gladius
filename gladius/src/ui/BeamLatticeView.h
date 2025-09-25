#pragma once

#include "Document.h"
#include <memory>

namespace Lib3MF
{
    class CModel;
    typedef std::shared_ptr<CModel> PModel;
}

namespace gladius::ui
{
    /**
     * @brief Class for displaying and managing beam lattice resources
     */
    class BeamLatticeView
    {
      public:
        /**
         * @brief Renders the beam lattice properties in the outline view
         *
         * @param document The document containing the beam lattice resources
         *
         * @return true if the beam lattice properties were modified
         */
        bool render(SharedDocument document) const;

      private:
        /**
         * @brief Gets the name of a beam lattice resource
         *
         * @param key The resource key to get the name for
         *
         * @return std::string The display name of the beam lattice
         */
        static std::string getBeamLatticeName(const ResourceKey & key);
    };
}
