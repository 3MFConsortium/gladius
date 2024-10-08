#include "Outline.h"

#include "nodes/BuildItem.h"
#include "nodes/Components.h"
#include "nodes/Object.h"

#include "imgui.h"

namespace gladius::ui
{
    void Outline::setDocument(SharedDocument document)
    {
        m_document = std::move(document);
    }

    void Outline::render() const
    {
        if (!m_document)
        {
            return;
        }

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        if (ImGui::TreeNodeEx("build items", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (auto const & item : m_document->getBuildItems())
            {
                renderBuildItem(item);
            }

            ImGui::TreePop();
        }
    }

    void Outline::renderBuildItem(gladius::nodes::BuildItem const & item) const
    {
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
               if (ImGui::TreeNodeEx(fmt::format("Component_{}", component.id).c_str(), baseFlags | ImGuiTreeNodeFlags_Leaf))
               {
                   ImGui::TreePop();
               }
            }

            ImGui::TreePop();
        }
    }

} // namespace gladius::ui