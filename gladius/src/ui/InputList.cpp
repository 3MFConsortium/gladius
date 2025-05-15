#include "InputList.h"
#include "LinkColors.h"
#include "Style.h"
#include "graph/GraphAlgorithms.h"
#include <imguinodeeditor.h>

namespace ed = ax::NodeEditor;

namespace gladius::ui
{
    OptionalPortId inputMenu(nodes::Model & nodes,
                             gladius::nodes::VariantParameter targetParameter,
                             std::string targetName)
    {
        const auto & ports = nodes.getPortRegistry();
        nodes::ParameterId targetId = targetParameter.getId();

        const auto targetIter = nodes.getParameterRegistry().find(targetId);
        if (targetIter == std::end(nodes.getParameterRegistry()))
        {
            return {};
        }

        const auto isSuccessor = [&](nodes::NodeId nodeId)
        { return isDependingOn(nodes.getGraph(), nodeId, targetIter->second->getParentId()); };

        auto const currentMousePos = ImGui::GetMousePos();
        auto posOnCanvas = ed::ScreenToCanvas(currentMousePos);
        posOnCanvas.x -= 400;

        // get the positition of the parent node of the target parameter
        auto const parentNode = nodes.getNode(targetParameter.getParentId());
        if (parentNode.has_value())
        {
            auto const screenPos = ed::GetNodePosition(parentNode.value()->getId());
            posOnCanvas.x = screenPos.x - 300;
            posOnCanvas.y = screenPos.y;
        }
        if (ImGui::BeginPopup("Ports"))
        {
            // Button for creating a new node of (Constant, Vector, Matrix, Resource) depending on
            // the type of the target parameter and use the new node as input
            if (targetParameter.getTypeIndex() == nodes::ParameterTypeIndex::Float)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      LinkColors::DarkColorFloat); // Set button color to blue
                if (ImGui::Button("New Scalar Node"))
                {
                    nodes::ConstantScalar * newNode = nodes.create<nodes::ConstantScalar>();
                    newNode->setDisplayName(targetName);

                    ed::SetNodePosition(newNode->getId(), posOnCanvas);

                    ImGui::CloseCurrentPopup();
                    ImGui::PopStyleColor();
                    ImGui::EndPopup();
                    return {newNode->getValueOutputPort().getId()};
                }
                ImGui::PopStyleColor();
            }

            if (targetParameter.getTypeIndex() == nodes::ParameterTypeIndex::Float3)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      LinkColors::DarkColorFloat3); // Set button color to green
                if (ImGui::Button("New Vector Node"))
                {
                    nodes::ConstantVector * newNode = nodes.create<nodes::ConstantVector>();
                    newNode->setDisplayName(targetName);
                    ed::SetNodePosition(newNode->getId(), posOnCanvas);
                    ImGui::PopStyleColor();
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return {newNode->getVectorOutputPort().getId()};
                }
                ImGui::PopStyleColor();
            }

            if (targetParameter.getTypeIndex() == nodes::ParameterTypeIndex::Matrix4)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      LinkColors::DarkColorMatrix); // Set button color to red
                if (ImGui::Button("New Matrix Node"))
                {
                    nodes::ConstantMatrix * newNode = nodes.create<nodes::ConstantMatrix>();
                    newNode->setDisplayName(targetName);
                    ed::SetNodePosition(newNode->getId(), posOnCanvas);

                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return {newNode->getMatrixOutputPort().getId()};
                }
                ImGui::PopStyleColor();
            }

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
