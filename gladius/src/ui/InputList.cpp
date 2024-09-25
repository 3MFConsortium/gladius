#include "InputList.h"
#include "Style.h"
#include "graph/GraphAlgorithms.h"
#include <imguinodeeditor.h>

namespace gladius::ui
{
    OptionalPortId inputMenu(nodes::Model & nodes, nodes::ParameterId targetId)
    {
        const auto & ports = nodes.getPortRegistry();

        const auto targetIter = nodes.getParameterRegistry().find(targetId);
        if (targetIter == std::end(nodes.getParameterRegistry()))
        {
            return {};
        }

        const auto isSuccessor = [&](nodes::NodeId nodeId)
        { return isDependingOn(nodes.getGraph(), nodeId, targetIter->second->getParentId()); };

        if (ImGui::BeginPopup("Ports"))
        {
            for (const auto & port : ports)
            {
                auto sourceNode = nodes.getNode(port.second->getParentId());
                if (!sourceNode.has_value())
                {
                    std::cerr << "could not finde node the parent node with id"
                              << port.second->getParentId() << "\n";
                    continue;
                }

                if (isSuccessor(sourceNode.value()->getId()))
                {
                    continue;
                }

                auto popStyle = false;
                const auto styleIter = NodeColors.find(sourceNode.value()->getCategory());
                if (styleIter != std::end(NodeColors))
                {
                    const auto style = styleIter->second;

                    ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImU32>(style.color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                          static_cast<ImU32>(style.activeColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                          static_cast<ImU32>(style.hoveredColor));
                    popStyle = true;
                }

                const auto displayName = sourceNode.value()->getDisplayName();
                const auto shortName = port.second->getShortName();
                auto inputName = displayName;
                inputName += "::";
                inputName += shortName;
                const auto clicked = ImGui::Button(inputName.c_str());

                if (popStyle)
                {
                    ImGui::PopStyleColor(3);
                }

                if (clicked)
                {
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return {port.second->getId()};
                }
            }
            ImGui::EndPopup();
        }

        return {};
    }
} // namespace gladius::ui
