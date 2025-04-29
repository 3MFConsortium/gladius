#include "Outline.h"
#include "Widgets.h"
#include "nodes/BuildItem.h"
#include "nodes/Components.h"
#include "nodes/Object.h"
#include "imgui.h"
#include "BuildItemView.h"

namespace gladius::ui
{
    void Outline::setDocument(SharedDocument document)
    {
        m_document = std::move(document);
    }    bool Outline::render() const
    {
        if (!m_document)
        {
            return false;
        }

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGui::BeginGroup();
        bool propertiesChanged = false;
        if (ImGui::TreeNodeEx("Build Items", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Replace direct rendering with BuildItemView
            BuildItemView buildItemView;
            if (buildItemView.render(m_document))
            {
                // If build items were modified, mark the document as changed
                propertiesChanged = true;
            }            ImGui::TreePop();
        }

        ImGui::EndGroup();
        frameOverlay(ImVec4(1.0f, 0.9f, 0.6f, 0.1f));
        return propertiesChanged;
    }

    void Outline::renderBuildItem(gladius::nodes::BuildItem const & item) const
    {
        // This method is now deprecated in favor of the BuildItemView class
        // Keeping it for backward compatibility
        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGuiTreeNodeFlags nodeFlags = baseFlags;

        if (item.getComponents().empty())
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf;
        }

        if (ImGui::TreeNodeEx(item.getName().c_str(), nodeFlags))
        {
            for (auto const & component : item.getComponents())
            {
                ImGui::BeginGroup();
                if (ImGui::TreeNodeEx(fmt::format("Component_{}", component.id).c_str(),
                                      baseFlags | ImGuiTreeNodeFlags_Leaf))
                {
                    ImGui::TreePop();
                }
                ImGui::EndGroup();
                frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
            }
            
            ImGui::TreePop();
        }
    }

} // namespace gladius::ui