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
        frameOverlay(ImVec4(0.9f, 0.6f, 0.3f, 0.1f),
                     "Document Information\n\n"
                     "Add title, author, and other details about your design here.\n"
                     "This information helps identify your model when sharing with others or\n"
                     "when sending to manufacturing services.");

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
        frameOverlay(ImVec4(1.0f, 0.9f, 0.6f, 0.1f),
                     "Objects to Manufacture\n\n"
                     "This section shows the parts that will be sent to the printer.\n"
                     "You can:\n"
                     " Add new objects to your build\n"
                     " Position and rotate parts\n"
                     " Combine multiple objects in your design\n"
                     " Arrange items for optimal printing");

        return propertiesChanged;
    }

} // namespace gladius::ui