#include "Outline.h"
#include "Widgets.h"
#include "imgui.h"
#include "nodes/BuildItem.h"
#include "nodes/Components.h"
#include "nodes/Object.h"

namespace gladius::ui
{
    void Outline::setDocument(SharedDocument document)
    {
        m_document = std::move(document);
    }

    bool Outline::render() const
    {
        if (!m_document)
        {
            return false;
        }

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        bool propertiesChanged = false;

        // Metadata section
        ImGui::BeginGroup();

        if (ImGui::TreeNodeEx("Metadata", baseFlags))
        {
            // Render the metadata view
            MetaDataView metaDataView;
            if (metaDataView.render(m_document))
            {
                // If metadata was modified, mark the document as changed
                propertiesChanged = true;
            }
            ImGui::TreePop();
        }
        ImGui::EndGroup();
        frameOverlay(ImVec4(0.9f, 0.6f, 0.3f, 0.1f));

        ImGui::BeginGroup();
        // Build Items section
        if (ImGui::TreeNodeEx("Build Items", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Replace direct rendering with BuildItemView
            BuildItemView buildItemView;
            if (buildItemView.render(m_document))
            {
                // If build items were modified, mark the document as changed
                propertiesChanged = true;
            }
            ImGui::TreePop();
        }

        ImGui::EndGroup();
        frameOverlay(ImVec4(1.0f, 0.9f, 0.6f, 0.1f));

        return propertiesChanged;
    }

} // namespace gladius::ui