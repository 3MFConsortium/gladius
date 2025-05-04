#pragma once
#include "Document.h"
#include "BuildItemView.h"
#include "MetaDataView.h"

//forward declaration
namespace gladius::nodes
{
    class BuildItem;
}

namespace gladius::ui
{
    class Outline
    {
      public:
        Outline() = default;
        Outline(SharedDocument document)
            : m_document(std::move(document))
        {
        }

        void setDocument(SharedDocument document);

        bool render() const;

      private:
        SharedDocument m_document;
    };
} // namespace gladius::ui
