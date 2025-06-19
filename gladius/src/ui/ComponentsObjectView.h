#pragma once

#include "Document.h"
#include <memory>

namespace Lib3MF
{
    class CComponentsObject;
    class CComponent;
    class CModel;
    typedef std::shared_ptr<CComponentsObject> PComponentsObject;
    typedef std::shared_ptr<CComponent> PComponent;
    typedef std::shared_ptr<CModel> PModel;
}

namespace gladius::ui
{
    class ComponentsObjectView
    {
    public:
        // Main render function for ComponentsObjects
        bool render(SharedDocument document) const;

        // Helper functions
        static bool renderObjectDropdown(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PComponent component);
        
        static bool renderTransformControls(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PComponent component);
    };
} // namespace gladius::ui
