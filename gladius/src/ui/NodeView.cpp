#include "NodeView.h"

#include "Assembly.h"
#include "FileChooser.h"
#include "InputList.h"
#include "LinkColors.h"
#include "ModelEditor.h"
#include "Parameter.h"
#include "Style.h"
#include "Widgets.h"
#include "nodes/DerivedNodes.h"
#include "nodesfwd.h"

#include "imguinodeeditor.h"
#include <IconFontCppHeaders/IconsFontAwesome5.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <filesystem>
#include <fmt/format.h>
#include <set>
#include <unordered_map>

namespace ed = ax::NodeEditor;

namespace gladius::ui
{
    using namespace nodes;

    std::string typeToString(std::type_index typeIndex)
    {
        static std::vector<std::pair<std::string, std::type_index>> const types = {
          {"Scalar", std::type_index(typeid(float))},
          {"Vector", std::type_index(typeid(nodes::float3))},
          {"matrix4x4", std::type_index(typeid(nodes::Matrix4x4))},
          {"resourceId", std::type_index(typeid(uint32_t))},
        };

        auto const it =
          std::find_if(types.begin(),
                       types.end(),
                       [&typeIndex](auto const & pair) { return pair.second == typeIndex; });

        if (it == types.end())
        {
            return "any";
        }

        return it->first;
    }

    NodeView::NodeView()
    {
        m_nodeTypeToColor = createNodeTypeToColors();
    }

    void NodeView::visit(NodeBase & baseNode)
    {
        show(baseNode);
    }

    void NodeView::visit(Begin & beginNode)
    {
        header(beginNode);
        if (ImGui::BeginTable("beginNodeTable",
                              4,
                              ImGuiTableFlags_SizingStretchProp,
                              ImVec2(400 * m_uiScale, 100 * m_uiScale)))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 200.f * m_uiScale);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_None, 80.f * m_uiScale);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 100.f * m_uiScale);
            ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_None, 20.f * m_uiScale);

            auto outputs = beginNode.getOutputs();
            for (auto & output : outputs)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                std::string outputName = output.first;
                ImGui::SetNextItemWidth(170.f * m_uiScale);
                // ImGui::InputText("", &outputName);
                ImGui::TextUnformatted(output.first.c_str());
                ImGui::TableNextColumn();
                // Remove
                // if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_TRASH)))
                // {
                //     // ToDo: remove output
                // }
                ImGui::SameLine();
                ImGui::TableNextColumn();

                std::type_index typeIndex = output.second.getTypeIndex();
                // typeControl("", typeIndex);
                ImGui::TextUnformatted(typeToString(typeIndex).c_str());
                ImGui::TableNextColumn();
                BeginPin(output.second.getId(), ed::PinKind::Output);

                ImGui::PushStyleColor(ImGuiCol_Text, typeToColor(typeIndex));
                ImGui::TextUnformatted(reinterpret_cast<const char *>(ICON_FA_CARET_RIGHT));
                ImGui::PopStyleColor();
                ed::EndPin();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(200.f * m_uiScale * m_uiScale);
            if (ImGui::CollapsingHeader("Add Argument", ImGuiTreeNodeFlags_Framed))
            {
                ImGui::PushID("AddArgument");
                auto & newParameterName = m_newChannelProperties[beginNode.getId()].name;
                ImGui::SetNextItemWidth(100.f * m_uiScale);
                ImGui::InputText("name", &newParameterName);
                // ImGui::SameLine();
                std::type_index & typeIndex = m_newChannelProperties[beginNode.getId()].typeIndex;
                typeControl("type", typeIndex);
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_PLUS)))
                {
                    // TODO: check if name is unique and valid

                    m_modelEditor->currentModel()->addArgument(
                      newParameterName, createVariantTypeFromTypeIndex(typeIndex));
                    m_assembly->updateInputsAndOutputs();
                    m_parameterChanged = true;
                    m_modelChanged = true;
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        footer(beginNode);
    }

    void NodeView::visit(nodes::End & endNode)
    {
        header(endNode);
        if (ImGui::BeginTable("beginNodeTable",
                              4,
                              ImGuiTableFlags_SizingStretchProp,
                              ImVec2(400 * m_uiScale, 100 * m_uiScale)))
        {
            ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_None, 20.f * m_uiScale);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 200.f * m_uiScale);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_None, 80.f * m_uiScale);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 100.f * m_uiScale);

            for (auto & input : endNode.parameter())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                BeginPin(input.second.getId(), ed::PinKind::Input);
                ImGui::PushStyleColor(ImGuiCol_Text, typeToColor(input.second.getTypeIndex()));
                ImGui::TextUnformatted(reinterpret_cast<const char *>(ICON_FA_CARET_RIGHT));
                ImGui::PopStyleColor();
                ed::EndPin();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(input.first.c_str());
                ImGui::TableNextColumn();
                // Remove
                // if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_TRASH)))
                // {
                //     // ToDo: remove input
                // }
                ImGui::TableNextColumn();
                std::type_index typeIndex = input.second.getTypeIndex();
                // typeControl("", typeIndex);
                ImGui::TextUnformatted(typeToString(typeIndex).c_str());
                if (input.second.getSource().has_value())
                {
                    ImVec4 const linkColor = (input.second.isValid())
                                               ? typeToColor(input.second.getTypeIndex())
                                               : LinkColors::ColorInvalid;

                    ed::Link(++m_currentLinkId,
                             input.second.getSource().value().portId,
                             input.second.getId(),
                             linkColor);
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(200.f);
            if (ImGui::CollapsingHeader("Add Output", ImGuiTreeNodeFlags_Framed))
            {
                ImGui::PushID("AddOutput");
                auto & newParameterName = m_newOutputChannelProperties[endNode.getId()].name;

                ImGui::SetNextItemWidth(100.f);
                ImGui::InputText("name", &newParameterName);
                std::type_index & typeIndex =
                  m_newOutputChannelProperties[endNode.getId()].typeIndex;
                typeControl("type", typeIndex);
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_PLUS)))
                {
                    m_modelEditor->currentModel()->addFunctionOutput(
                      newParameterName, createVariantTypeFromTypeIndex(typeIndex));

                    m_assembly->updateInputsAndOutputs();
                    m_parameterChanged = true;
                    m_modelChanged = true;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        footer(endNode);
    }

    void NodeView::visit(nodes::ConstantScalar & constantScalarNode)
    {
        viewInputNode(constantScalarNode);
    }

    void NodeView::visit(nodes::ConstantVector & constantVectorNode)
    {
        viewInputNode(constantVectorNode);
    }

    void NodeView::visit(nodes::ConstantMatrix & constantMatrixNode)
    {
        viewInputNode(constantMatrixNode);
    }

    void NodeView::visit(nodes::Transformation & transformationNode)
    {
        //      eviewInputNode(transformationNode);
        show(transformationNode);
    }

    void NodeView::visit(nodes::Resource & resourceNode)
    {
        if (m_resoureIdNodesVisible)
        {
            viewInputNode(resourceNode);
        }
    }

    void NodeView::setModelEditor(ModelEditor * editor)
    {
        m_modelEditor = editor;
        reset();
    }

    bool NodeView::haveParameterChanged() const
    {
        return m_parameterChanged;
    }

    bool NodeView::hasModelChanged() const
    {
        return m_modelChanged;
    }

    void NodeView::setAssembly(nodes::SharedAssembly assembly)
    {
        m_assembly = std::move(assembly);
        if (m_modelEditor)
        {
            setCurrentModel(m_modelEditor->currentModel());
        }
        reset();
    }

    void NodeView::reset()
    {
        m_currentLinkId = 0;
        m_parameterChanged = false;
        m_modelChanged = false;
        m_previousNodePositions.clear(); // Clear position tracking when resetting
    }

    void NodeView::setCurrentModel(SharedModel model)
    {
        if (m_currentModel == model)
        {
            return;
        }
        m_currentModel = std::move(model);
        reset();
        m_columnWidths.clear();
        m_previousNodePositions.clear(); // Clear position tracking when switching models
    }

    void NodeView::setResourceNodesVisible(bool visible)
    {
        m_resoureIdNodesVisible = visible;
    }

    bool NodeView::areResourceNodesVisible() const
    {
        return m_resoureIdNodesVisible;
    }

    void NodeView::show(NodeBase & baseNode)
    {
        header(baseNode);
        content(baseNode);

        // Check for double-click on FunctionCall nodes to navigate to referenced function
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            nodes::ResourceId functionId = 0;

            if (auto * functionCallNode = dynamic_cast<nodes::FunctionCall *>(&baseNode))
            {
                functionId = functionCallNode->getFunctionId();
            }
            else if (auto * functionGradientNode =
                       dynamic_cast<nodes::FunctionGradient *>(&baseNode))
            {
                functionGradientNode->resolveFunctionId();
                functionId = functionGradientNode->getFunctionId();
            }

            if (m_modelEditor && functionId != 0)
            {
                // Use navigateToFunction so history is recorded
                m_modelEditor->navigateToFunction(functionId);
            }
        }

        footer(baseNode);
    }

    void NodeView::header(NodeBase & baseNode)
    {
        m_uiScale = ImGui::GetIO().FontGlobalScale * 2.0f;

        const auto colorIter = m_nodeTypeToColor.find(typeid(baseNode));
        m_popStyle = false;
        if (colorIter != std::end(m_nodeTypeToColor))
        {
            const auto color = colorIter->second;
            ed::PushStyleColor(ed::StyleColor_NodeBorder, color);
            ed::PushStyleColor(ed::StyleColor_NodeBg,
                               ImColor(color.x * 0.1f, color.y * 0.1f, color.z * 0.1f, 0.9f));
            m_popStyle = true;
        }

        ed::SetNodeZPosition(baseNode.getId(), 1.f);
        ed::BeginNode(baseNode.getId());
        ImGui::PushID(baseNode.getId());

        ImGui::PushItemWidth(150 * m_uiScale);
        std::string displayName = baseNode.getDisplayName();
        if (ImGui::InputText("", &displayName))
        {
            if (displayName.empty())
            {
                baseNode.setDisplayName(baseNode.name());
            }
            else
            {
                baseNode.setDisplayName(displayName);
            }
            m_parameterChanged = true;
        }
        ImGui::PopItemWidth();

        ImGui::Indent(20.f * m_uiScale);
        ImGui::SetWindowFontScale(0.8f);
        ImGui::TextUnformatted(baseNode.name().c_str());
        ImGui::SetWindowFontScale(1.f);
        ImGui::Unindent(20.f * m_uiScale);
    }

    void NodeView::content(NodeBase & baseNode)
    {
        showInputAndOutputs(baseNode);

        if (auto * functionGradientNode = dynamic_cast<nodes::FunctionGradient *>(&baseNode))
        {
            functionGradientControls(*functionGradientNode);
        }

        if (baseNode.parameterChangeInvalidatesPayload() && m_parameterChanged)
        {
            m_modelEditor->invalidatePrimitiveData();
        }
    }

    void NodeView::functionGradientControls(nodes::FunctionGradient & node)
    {
        ImGui::Spacing();
        ImGui::TextUnformatted("Gradient Configuration");

        if (m_assembly == nullptr)
        {
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f),
                               "Assembly not available – cannot resolve function outputs.");
            return;
        }

        node.resolveFunctionId();
        auto const functionId = node.getFunctionId();
        if (functionId == 0)
        {
            ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f),
                               "Select a function to compute its gradient.");
            return;
        }

        auto referencedModel = m_assembly->findModel(functionId);
        if (!referencedModel)
        {
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f),
                               "Referenced function not found in the assembly.");
            return;
        }

        auto const & functionOutputs = referencedModel->getOutputs();
        std::vector<std::string> scalarOutputs;
        scalarOutputs.reserve(functionOutputs.size());
        for (auto const & [name, parameter] : functionOutputs)
        {
            if (parameter.getTypeIndex() == ParameterTypeIndex::Float)
            {
                scalarOutputs.push_back(name);
            }
        }

        std::vector<std::string> vectorInputs;
        vectorInputs.reserve(node.parameter().size());
        for (auto const & [name, parameter] : node.constParameter())
        {
            if (!parameter.isArgument())
            {
                continue;
            }
            if (parameter.getTypeIndex() == ParameterTypeIndex::Float3)
            {
                vectorInputs.push_back(name);
            }
        }

        auto const warningColor = ImVec4(1.f, 0.6f, 0.2f, 1.f);

        bool selectionChanged = false;

        std::string selectedScalar = node.getSelectedScalarOutput();
        std::string scalarPreview =
          selectedScalar.empty() ? "Select scalar output" : selectedScalar;
        bool const hasScalarOutputs = !scalarOutputs.empty();

        if (!hasScalarOutputs)
        {
            scalarPreview = "No scalar outputs available";
            ImGui::BeginDisabled();
        }

        // Popup-based selector for scalar output (more reliable in node editor)
        if (ImGui::Button(scalarPreview.c_str()))
        {
            m_showContextMenu = true;
            auto popupName = fmt::format("FG_ScalarOutput_{}", node.getId());
            // Capture copies to avoid dangling references across frames
            auto scalarOutputsCopy = scalarOutputs;   // by-value
            auto selectedScalarCopy = selectedScalar; // by-value
            auto * nodePtr = &node;                   // pointer is stable while node exists
            m_modelEditor->showPopupMenu(
              [this, popupName, scalarOutputsCopy, selectedScalarCopy, nodePtr]()
              {
                  if (m_showContextMenu)
                  {
                      ImGui::OpenPopup(popupName.c_str());
                      m_showContextMenu = false;
                  }

                  if (ImGui::BeginPopup(popupName.c_str()))
                  {
                      bool const isNoneSelected = selectedScalarCopy.empty();
                      if (ImGui::Selectable("None", isNoneSelected))
                      {
                          if (!isNoneSelected)
                          {
                              nodePtr->setSelectedScalarOutput("");
                              m_parameterChanged = true;
                              if (m_modelEditor)
                                  m_modelEditor->markModelAsModified();
                          }
                      }

                      for (auto const & option : scalarOutputsCopy)
                      {
                          bool const isSelected = (option == selectedScalarCopy);
                          if (ImGui::Selectable(option.c_str(), isSelected))
                          {
                              nodePtr->setSelectedScalarOutput(option);
                              m_parameterChanged = true;
                              if (m_modelEditor)
                                  m_modelEditor->markModelAsModified();
                          }
                          if (isSelected)
                          {
                              ImGui::SetItemDefaultFocus();
                          }
                      }
                      ImGui::EndPopup();
                  }
              });
        }

        if (!hasScalarOutputs)
        {
            ImGui::EndDisabled();
            ImGui::TextColored(warningColor, "The referenced function exposes no scalar outputs.");
        }
        else if (!selectedScalar.empty() &&
                 std::find(scalarOutputs.begin(), scalarOutputs.end(), selectedScalar) ==
                   scalarOutputs.end())
        {
            ImGui::TextColored(warningColor,
                               "Previously selected scalar output is no longer available.");
        }

        std::string selectedVector = node.getSelectedVectorInput();
        std::string vectorPreview = selectedVector.empty() ? "Select vector input" : selectedVector;
        bool const hasVectorInputs = !vectorInputs.empty();

        if (!hasVectorInputs)
        {
            vectorPreview = "No vector inputs available";
            ImGui::BeginDisabled();
        }

        // Popup-based selector for vector input
        if (ImGui::Button(vectorPreview.c_str()))
        {
            m_showContextMenu = true;
            auto popupName = fmt::format("FG_VectorInput_{}", node.getId());
            auto vectorInputsCopy = vectorInputs;     // by-value
            auto selectedVectorCopy = selectedVector; // by-value
            auto * nodePtr = &node;                   // pointer is stable while node exists
            m_modelEditor->showPopupMenu(
              [this, popupName, vectorInputsCopy, selectedVectorCopy, nodePtr]()
              {
                  if (m_showContextMenu)
                  {
                      ImGui::OpenPopup(popupName.c_str());
                      m_showContextMenu = false;
                  }

                  if (ImGui::BeginPopup(popupName.c_str()))
                  {
                      bool const isNoneSelected = selectedVectorCopy.empty();
                      if (ImGui::Selectable("None", isNoneSelected))
                      {
                          if (!isNoneSelected)
                          {
                              nodePtr->setSelectedVectorInput("");
                              m_parameterChanged = true;
                              if (m_modelEditor)
                                  m_modelEditor->markModelAsModified();
                          }
                      }

                      for (auto const & option : vectorInputsCopy)
                      {
                          bool const isSelected = (option == selectedVectorCopy);
                          if (ImGui::Selectable(option.c_str(), isSelected))
                          {
                              nodePtr->setSelectedVectorInput(option);
                              m_parameterChanged = true;
                              if (m_modelEditor)
                                  m_modelEditor->markModelAsModified();
                          }
                          if (isSelected)
                          {
                              ImGui::SetItemDefaultFocus();
                          }
                      }
                      ImGui::EndPopup();
                  }
              });
        }

        if (!hasVectorInputs)
        {
            ImGui::EndDisabled();
            ImGui::TextColored(warningColor, "The mirrored arguments provide no vector inputs.");
        }
        else if (!selectedVector.empty() &&
                 std::find(vectorInputs.begin(), vectorInputs.end(), selectedVector) ==
                   vectorInputs.end())
        {
            ImGui::TextColored(warningColor,
                               "Previously selected vector input is no longer available.");
        }

        if (selectionChanged)
        {
            m_parameterChanged = true;
            if (m_modelEditor)
            {
                m_modelEditor->markModelAsModified();
            }
        }

        // Step size control
        if (auto it = node.parameter().find(FieldNames::StepSize); it != node.parameter().end())
        {
            auto & stepVar = it->second.Value();
            if (auto pStep = std::get_if<float>(&stepVar))
            {
                float prev = *pStep;
                ImGui::SetNextItemWidth(150.f * m_uiScale);
                if (ImGui::DragFloat("Step Size", pStep, 0.001f, 0.0f, 1000.0f, "%.6f"))
                {
                    if (*pStep < 0.0f)
                    {
                        *pStep = 0.0f; // keep non-negative; kernel clamps to >= 1e-8
                    }
                    if (!it->second.isModifiable())
                    {
                        it->second.setModifiable(true);
                    }
                    m_parameterChanged = true;
                    if (m_modelEditor)
                    {
                        m_modelEditor->markModelAsModified();
                    }
                }
            }
        }

        if (!node.hasValidConfiguration())
        {
            ImGui::TextColored(
              warningColor,
              "Select both a scalar output and a vector input to enable the gradient.");
        }
        else
        {
            ImGui::TextColored(
              ImVec4(0.6f, 0.8f, 1.f, 1.f),
              "Gradient output is normalized and falls back to zero for near-zero magnitudes.");
        }
    }

    void NodeView::footer(NodeBase & baseNode)
    {
        ed::EndNode();

        if (m_popStyle)
        {
            ed::PopStyleColor(2); // We push 2 colors in header(), so pop 2 here
        }
        ImGui::PopID();

        auto & columnWidths = getOrCreateColumnWidths(baseNode.getId());
        // Add padding to all columns of this node
        for (auto & columnWidth : columnWidths)
        {
            if (columnWidth > 0.f)
            {
                columnWidth += 10.f * m_uiScale;
            }
        }
    }

    std::string sourceName(Model & nodes, PortId portId)
    {
        return nodes.getSourceName(portId);
    }

    bool NodeView::viewString(NodeBase const & node,
                              ParameterMap::reference parameter,
                              VariantType & val)
    {
        if (const auto name = std::get_if<std::string>(&val))
        {
            const auto previousText = *name;
            val = *name;
            ImGui::SameLine();
            ImGui::PushItemWidth(200 * m_uiScale);

            // Check if this node should receive focus (keyboard-driven workflow)
            bool shouldFocus = m_modelEditor && m_modelEditor->shouldFocusNode(node.getId());
            if (shouldFocus && parameter.first == node.constParameter().begin()->first)
            {
                // Focus on the first input field
                ImGui::SetKeyboardFocusHere();
                m_modelEditor->clearNodeFocus();
            }

            bool changed = ImGui::InputText("", name);

            if (parameter.first == FieldNames::Filename)
            {
                ImGui::SameLine();
                const auto baseDir = m_assembly->getFilename().remove_filename();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN)))
                {

                    const auto queriedFilename = queryLoadFilename({{"*.stl"}}, baseDir);

                    if (queriedFilename && queriedFilename->has_filename())
                    {
                        const std::filesystem::path filename{queriedFilename.value()};

                        *name = filename.string();
                        changed = true;
                        m_modelEditor->invalidatePrimitiveData();
                        m_modelEditor->markModelAsModified();
                        m_parameterChanged = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Make relative"))
                {
                    const std::filesystem::path filename{*name};
                    const auto relativePath = relative(filename, baseDir);
                }
            }

            if (changed && (parameter.first == FieldNames::Filename) && (previousText != *name))
            {
                m_modelEditor->invalidatePrimitiveData();
                m_parameterChanged = true;
            }
            val = *name;
            ImGui::PopItemWidth();
        }
        return false;
    }

    void NodeView::viewFloat(nodes::NodeBase const & node,
                             nodes::ParameterMap::reference parameter,
                             nodes::VariantType & val)
    {
        if (const auto pval = std::get_if<float>(&val))
        {
            ImGui::SameLine();

            // Check if this node should receive focus (keyboard-driven workflow)
            bool shouldFocus = m_modelEditor && m_modelEditor->shouldFocusNode(node.getId());
            if (shouldFocus && parameter.first == node.constParameter().begin()->first)
            {
                // Focus on the first input field
                ImGui::SetKeyboardFocusHere();
                m_modelEditor->clearNodeFocus();
            }

            auto increment = 0.01f;
            bool changed = false;

            switch (parameter.second.getContentType())
            {
            case ContentType::Length:
            {
                std::string formatString{"%.3f "};
                changed = ui::floatEdit(parameter.first, *pval);
                // changed = ImGui::DragFloat(parameter.first.c_str(), pval, increment);
                break;
            }
            case ContentType::Angle:
            {
                changed = angleEdit(parameter.first.c_str(), pval);
                break;
            }
            default:
            {
                changed = ImGui::DragFloat(parameter.first.c_str(), pval, increment);
                break;
            }
            }

            bool const modifiable = parameter.second.isModifiable();

            if (changed && !modifiable)
            {
                parameter.second.setModifiable(true);
                m_modelEditor->markModelAsModified();
            }

            m_parameterChanged |= changed;
        }
    }

    void NodeView::viewFloat3(nodes::NodeBase const & node,
                              nodes::ParameterMap::reference parameter,
                              nodes::VariantType & val)
    {
        if (const auto pval = std::get_if<float3>(&val))
        {
            ImGui::TextUnformatted("Vector");
            bool changed = false;
            ImGui::PushItemWidth(300 * m_uiScale);
            const auto increment = 0.1f;

            // Check if this node should receive focus (keyboard-driven workflow)
            bool shouldFocus = m_modelEditor && m_modelEditor->shouldFocusNode(node.getId());
            if (shouldFocus && parameter.first == node.constParameter().begin()->first)
            {
                // Focus on the first input field
                ImGui::SetKeyboardFocusHere();
                m_modelEditor->clearNodeFocus();
            }

            if (parameter.second.getContentType() == ContentType::Length)
            {
                changed |= ImGui::DragFloat("x", &pval->x, increment);
                changed |= ImGui::DragFloat("y", &pval->y, increment);
                changed |= ImGui::DragFloat("z", &pval->z, increment);
            }
            else if (parameter.second.getContentType() == ContentType::Color)
            {
                changed = ImGui::ColorEdit3(
                  "",
                  &pval->x,
                  ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoPicker |
                    ImGuiColorEditFlags_NoTooltip |
                    ImGuiColorEditFlags_Float); // popups do no not work properly inside the
                // canvas, so we have to deactivate them
            }
            else
            {
                changed |= ImGui::DragFloat("x", &pval->x, increment);
                changed |= ImGui::DragFloat("y", &pval->y, increment);
                changed |= ImGui::DragFloat("z", &pval->z, increment);
            }

            bool const modifiable = parameter.second.isModifiable();

            if (changed && !modifiable)
            {
                parameter.second.setModifiable(true);
                m_modelEditor->markModelAsModified();
            }

            m_parameterChanged |= changed;

            ImGui::PopItemWidth();
        }
    }

    void NodeView::viewMatrix(nodes::NodeBase const & node,
                              nodes::ParameterMap::reference parameter,
                              nodes::VariantType & val)
    {
        if (const auto pval = std::get_if<Matrix4x4>(&val))
        {
            ImGui::PushItemWidth(300 * m_uiScale);

            // Check if this node should receive focus (keyboard-driven workflow)
            bool shouldFocus = m_modelEditor && m_modelEditor->shouldFocusNode(node.getId());
            if (shouldFocus && parameter.first == node.constParameter().begin()->first)
            {
                // Focus on the first input field
                ImGui::SetKeyboardFocusHere();
                m_modelEditor->clearNodeFocus();
            }

            bool const changed = matrixEdit("", *pval);
            ImGui::PopItemWidth();

            bool const modifiable = parameter.second.isModifiable();

            if (changed && !modifiable)
            {
                parameter.second.setModifiable(true);
                m_modelEditor->markModelAsModified();
            }

            m_parameterChanged |= changed;
        }
    }

    void NodeView::viewResource(nodes::NodeBase & node,
                                nodes::ParameterMap::reference parameter,
                                nodes::VariantType & val)
    {
        if (auto * pval = std::get_if<ResourceId>(&val))
        {
            ImGui::SameLine();
            auto resId = static_cast<int>(*pval);

            // Check if this node should receive focus (keyboard-driven workflow)
            bool shouldFocus = m_modelEditor && m_modelEditor->shouldFocusNode(node.getId());
            if (shouldFocus && parameter.first == node.constParameter().begin()->first)
            {
                // Focus on the first input field
                ImGui::SetKeyboardFocusHere();
                m_modelEditor->clearNodeFocus();
            }

            if (ImGui::InputInt("ResourceId", &resId))
            {
                *pval = static_cast<ResourceId>(resId);
                m_parameterChanged = true;
                m_modelEditor->markModelAsModified();
            }

            auto funcitionIter = m_assembly->getFunctions().find(*pval);
            auto functionName = funcitionIter != std::end(m_assembly->getFunctions())
                                  ? funcitionIter->second->getDisplayName().value_or("")
                                  : "";

            // show popup menu with all functions
            if (ImGui::Button(functionName.c_str()))
            {
                m_showContextMenu = true;
                m_modelEditor->showPopupMenu(
                  [&]()
                  {
                      if (m_showContextMenu)
                      {
                          ImGui::OpenPopup("Functions");
                          m_showContextMenu = false;
                      }

                      if (ImGui::BeginPopup("Functions"))
                      {
                          for (auto & [id, model] : m_assembly->getFunctions())
                          {
                              if (ImGui::Button(model->getDisplayName()
                                                  .value_or(fmt::format("# {}", id))
                                                  .c_str()))
                              {
                                  nodes::FunctionCall * functionCallNode =
                                    dynamic_cast<nodes::FunctionCall *>(&node);
                                  if (functionCallNode)
                                  {
                                      functionCallNode->setFunctionId(id);
                                      if (m_assembly)
                                      {
                                          if (auto referencedModel = m_assembly->findModel(id))
                                          {
                                              functionCallNode->updateInputsAndOutputs(
                                                *referencedModel);
                                          }
                                      }
                                      m_parameterChanged = true;
                                      m_modelEditor->markModelAsModified();
                                      m_modelEditor->closePopupMenu();
                                  }
                                  else if (auto * functionGradientNode =
                                             dynamic_cast<nodes::FunctionGradient *>(&node))
                                  {
                                      functionGradientNode->setFunctionId(id);
                                      if (m_assembly)
                                      {
                                          if (auto referencedModel = m_assembly->findModel(id))
                                          {
                                              functionGradientNode->updateInputsAndOutputs(
                                                *referencedModel);
                                          }
                                      }
                                      m_parameterChanged = true;
                                      m_modelEditor->markModelAsModified();
                                      m_modelEditor->closePopupMenu();
                                  }
                              }
                          }
                          ImGui::EndPopup();
                      }
                  });
            }
        }
    }

    void NodeView::viewInt(nodes::NodeBase const & node,
                           nodes::ParameterMap::reference parameter,
                           nodes::VariantType & val)
    {
        if (const auto pval = std::get_if<int>(&val))
        {
            ImGui::SameLine();
            ImGui::PushItemWidth(200 * m_uiScale);

            // Check if this node should receive focus (keyboard-driven workflow)
            bool shouldFocus = m_modelEditor && m_modelEditor->shouldFocusNode(node.getId());
            if (shouldFocus && parameter.first == node.constParameter().begin()->first)
            {
                // Focus on the first input field
                ImGui::SetKeyboardFocusHere();
                m_modelEditor->clearNodeFocus();
            }

            bool changed = ImGui::DragInt("", pval);
            ImGui::PopItemWidth();

            bool const modifiable = parameter.second.isModifiable();

            if (changed && !modifiable)
            {
                parameter.second.setModifiable(true);
                m_modelEditor->markModelAsModified();
            }

            m_parameterChanged |= changed;
        }
    }

    bool NodeView::typeControl(std::string const & label, std::type_index & typeIndex)
    {
        static std::vector<std::pair<std::string, std::type_index>> const types = {
          {"Scalar", std::type_index(typeid(float))},
          {"Vector", std::type_index(typeid(nodes::float3))},
          {"matrix4x4", std::type_index(typeid(nodes::Matrix4x4))},
          {"resourceId", std::type_index(typeid(uint32_t))},
        };

        auto const it =
          std::find_if(types.begin(),
                       types.end(),
                       [&typeIndex](auto const & pair) { return pair.second == typeIndex; });

        if (it == types.end())
        {
            return false;
        }

        auto const index = std::distance(types.begin(), it);
        auto const labelId = fmt::format("##{}", label);

        bool changed = false;

        if (ImGui::Button(it->first.c_str()))
        {
            m_showContextMenu = true;
            m_modelEditor->showPopupMenu(
              [&]()
              {
                  if (m_showContextMenu)
                  {
                      ImGui::OpenPopup("Types");
                      m_showContextMenu = false;
                  }

                  if (ImGui::BeginPopup("Types"))
                  {
                      for (int i = 0; i < types.size(); ++i)
                      {
                          bool const isSelected = i == index;
                          if (ImGui::Button(types[i].first.c_str()))
                          {
                              typeIndex = types[i].second;
                              changed = true;
                              m_modelEditor->markModelAsModified();
                          }
                          if (isSelected)
                          {
                              ImGui::SetItemDefaultFocus();
                          }
                      }
                      ImGui::EndPopup();
                  }
              });
        }

        return changed;
    }

    void NodeView::inputControls(NodeBase & node, ParameterMap::reference parameter)
    {
        auto & val = parameter.second.Value();
        if (!parameter.second.isVisible())
        {
            return;
        }

        ImGui::Indent(20 * m_uiScale);
        if (parameter.first != FieldNames::Shape)
        {
            if (parameter.second.getTypeIndex() == ParameterTypeIndex::Int)
            {
                viewInt(node, parameter, val);
            }
            if (parameter.second.getTypeIndex() == ParameterTypeIndex::Float)
            {
                viewFloat(node, parameter, val);
            }
            if (parameter.second.getTypeIndex() == ParameterTypeIndex::Float3)
            {
                viewFloat3(node, parameter, val);
            }
            if (parameter.second.getTypeIndex() == ParameterTypeIndex::Matrix4)
            {
                viewMatrix(node, parameter, val);
            }
            if (parameter.second.getTypeIndex() == ParameterTypeIndex::ResourceId)
            {
                viewResource(node, parameter, val);
            }
        }

        // update column width
        auto & columnWidths = getOrCreateColumnWidths(node.getId());
        columnWidths[0] = std::max(columnWidths[0], ImGui::GetItemRectSize().x);

        if (m_assembly == nullptr)
        {
            throw std::runtime_error("NodeView: Assembly has to be set");
        }

        if (m_modelEditor == nullptr)
        {
            throw std::runtime_error("NodeView: ModelEditor has to be set");
        }

        if (viewString(node, parameter, val))
        {
            columnWidths[0] = std::max(columnWidths[0], ImGui::GetItemRectSize().x);
            return;
        }

        if (const auto resKey = std::get_if<ResourceKey>(&val))
        {
            if (resKey->getResourceId().has_value())
            {
                ImGui::TextUnformatted(resKey->getDisplayName().c_str());
            }
        }
        columnWidths[0] = std::max(columnWidths[0], ImGui::GetItemRectSize().x);
        ImGui::Indent(-20 * m_uiScale);
    }

    void NodeView::showLinkAssignmentMenu(ParameterMap::reference parameter)
    {
        m_showLinkAssignmentMenu = true;
        // Copy the required data so the callback does not hold dangling references across frames
        nodes::VariantParameter const targetParameterCopy = parameter.second; // by-value copy
        nodes::ParameterId const targetParamId = parameter.second.getId();
        std::string const targetNameCopy = parameter.first; // by-value copy

        m_modelEditor->showPopupMenu(
          [this, targetParameterCopy, targetParamId, targetNameCopy]()
          {
              if (m_showLinkAssignmentMenu)
              {
                  ImGui::OpenPopup("Ports");
                  m_showLinkAssignmentMenu = false;
              }
              const auto model = m_modelEditor->currentModel();
              if (!model)
              {
                  return;
              }

              // Use the safe copies captured above
              const auto newSource = inputMenu(*model, targetParameterCopy, targetNameCopy);
              if (newSource.has_value())
              {
                  model->addLink(newSource.value(), targetParamId, false);
                  m_modelEditor->markModelAsModified();
              }
          });
    }

    void NodeView::showInputAndOutputs(NodeBase & node)
    {
        if (!m_currentModel)
        {
            return;
        }

        auto & columnWidths = getOrCreateColumnWidths(node.getId());
        constexpr float minWidth = 170.f;
        // sum of all column widths
        float tableWidth = 0.f;
        for (auto width : columnWidths)
        {
            tableWidth += width;
        }

        float const fillSpace = std::max(0.f, minWidth - tableWidth - 20.f * m_uiScale);
        tableWidth = std::max(tableWidth, minWidth);

        bool const needsFillSpace = fillSpace > 0.f;

        if (ImGui::BeginTable("InputAndOutputs",
                              (needsFillSpace) ? 3 : 2,
                              ImGuiTableFlags_SizingStretchProp,
                              ImVec2(tableWidth, 0)))
        {
            ImGui::TableSetupColumn(
              "Inputs", ImGuiTableColumnFlags_WidthFixed, columnWidths[1] + columnWidths[2]);
            if (needsFillSpace)
            {
                ImGui::TableSetupColumn("Seperation", ImGuiTableColumnFlags_WidthFixed, fillSpace);
            }
            ImGui::TableSetupColumn(
              "Outputs", ImGuiTableColumnFlags_WidthFixed, columnWidths[6] + columnWidths[7]);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            inputPins(node);
            if (needsFillSpace)
            {
                ImGui::TableNextColumn();
            }
            ImGui::TableNextColumn();
            outputPins(node);
        }
        ImGui::EndTable();
    }

    void NodeView::inputPins(nodes::NodeBase & node)
    {
        std::set<ParameterId> usedPins;

        auto & columnWidths = getOrCreateColumnWidths(node.getId());

        auto const tableWidth = columnWidths[1] + columnWidths[2];
        if (ImGui::BeginTable("table", 2, ImGuiTableFlags_SizingStretchProp, ImVec2(tableWidth, 0)))
        {
            ImGui::TableSetupColumn("InputPin", ImGuiTableColumnFlags_WidthFixed, columnWidths[1]);
            ImGui::TableSetupColumn("InputName", ImGuiTableColumnFlags_WidthFixed, columnWidths[2]);

            columnWidths[1] = 0.f;
            columnWidths[2] = 0.f;

            for (auto & parameter : node.parameter())
            {
                if (parameter.second.getId() == -1)
                {
                    // not completly initialized yet
                    continue;
                }

                if (usedPins.find(parameter.second.getId()) != std::end(usedPins))
                {
                    throw std::runtime_error("Duplicate pin id");
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                usedPins.insert(parameter.second.getId());

                if (!parameter.second.isVisible())
                {
                    continue;
                }

                // show resource id pins only if the flag is set
                if (!m_resoureIdNodesVisible &&
                    parameter.second.getTypeIndex() == ParameterTypeIndex::ResourceId)
                {
                    continue;
                }

                ImGui::PushID(parameter.second.getId()); // required for reusing the same labels
                                                         // (that are used as unique ids in ImgUI)
                {
                    bool const inputMissing = !parameter.second.getSource().has_value() &&
                                              parameter.second.isInputSourceRequired();

                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          typeToColor(parameter.second.getTypeIndex()));
                    const ed::PinId pinId = parameter.second.getId();
                    BeginPin(pinId, ed::PinKind::Input);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8 * m_uiScale, 0});

                    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 1.5f); // Scale the button width

                    // Check if this node should receive focus (keyboard-driven workflow)
                    bool shouldFocus =
                      m_modelEditor && m_modelEditor->shouldFocusNode(node.getId());
                    if (shouldFocus && parameter.first == node.constParameter().begin()->first)
                    {
                        // Focus on the first input pin/button
                        ImGui::SetKeyboardFocusHere();
                        m_modelEditor->clearNodeFocus();
                    }

                    if (ImGui::Button(
                          reinterpret_cast<const char *>(ICON_FA_CARET_RIGHT),
                          ImVec2(ImGui::GetFontSize() * 1.5f, ImGui::GetFontSize() * 1.5f)))
                    {
                        columnWidths[1] = std::max(columnWidths[1], ImGui::GetItemRectSize().x);
                        showLinkAssignmentMenu(parameter);
                    }

                    ImGui::PopStyleVar();
                    ed::EndPin();
                    ImGui::PopStyleColor(); // Pop the style color for pin text

                    columnWidths[1] = std::max(columnWidths[1], ImGui::GetItemRectSize().x);
                    ImGui::TableNextColumn();

                    if (inputMissing)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, LinkColors::ColorInvalid);
                    }
                    ImGui::TextUnformatted((parameter.first).c_str());
                    columnWidths[2] = std::max(columnWidths[2], ImGui::GetItemRectSize().x);

                    // Add a label below the button with the type name
                    if (!inputMissing)
                    {
                        // decreaes font size
                        ImGui::SetWindowFontScale(0.5f);
                        ImGui::TextUnformatted(
                          typeToString(parameter.second.getTypeIndex()).c_str());
                        columnWidths[2] = std::max(columnWidths[2], ImGui::GetItemRectSize().x);
                        ImGui::SetWindowFontScale(1.0f);
                    }
                    if (inputMissing)
                    {
                        // decreaes font size
                        ImGui::SetWindowFontScale(0.5f);
                        ImGui::TextUnformatted(
                          fmt::format("Add a input of {} type",
                                      typeToString(parameter.second.getTypeIndex()))
                            .c_str());
                        ImGui::PopStyleColor(); // Pop style color for missing input
                        columnWidths[2] = std::max(columnWidths[2], ImGui::GetItemRectSize().x);
                        ImGui::SetWindowFontScale(1.0f);
                    }
                }
                ImGui::PopID();

                if (parameter.second.getSource().has_value())
                {
                    ImVec4 const linkColor = (parameter.second.isValid())
                                               ? typeToColor(parameter.second.getTypeIndex())
                                               : LinkColors::ColorInvalid;

                    ed::Link(++m_currentLinkId,
                             parameter.second.getSource().value().portId,
                             parameter.second.getId(),
                             linkColor);
                }
            }
        }
        ImGui::EndTable();
    }

    void NodeView::outputPins(nodes::NodeBase & node)
    {
        std::set<ParameterId> usedPins;

        auto & columnWidths = getOrCreateColumnWidths(node.getId());

        if (ImGui::BeginTable("outputs",
                              2,
                              ImGuiTableFlags_SizingStretchProp,
                              ImVec2(columnWidths[6] + columnWidths[7], 0)))
        {
            ImGui::TableSetupColumn(
              "OutputName", ImGuiTableColumnFlags_WidthFixed, columnWidths[6]);
            ImGui::TableSetupColumn("OutputPin", ImGuiTableColumnFlags_WidthFixed, columnWidths[7]);

            columnWidths[6] = 0.f;
            columnWidths[7] = 0.f;
            for (auto & output : node.getOutputs())
            {
                // check if output.second.getId() is already used as a pin
                if (usedPins.find(output.second.getId()) != std::end(usedPins))
                {
                    throw std::runtime_error(
                      fmt::format("Duplicate pin id {}", output.second.getId()).c_str());
                }
                usedPins.insert(output.second.getId());

                if (!output.second.isVisible())
                {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushID(output.second.getId()); // required for reusing the same labels
                                                      // (that are used as unique ids in ImgUI)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, typeToColor(output.second.getTypeIndex()));
                    ImGui::TextUnformatted((output.first).c_str());
                    columnWidths[6] = std::max(columnWidths[6], ImGui::GetItemRectSize().x);

                    // Add a label below the button with the type name

                    ImGui::SetWindowFontScale(0.5f);

                    ImGui::TextUnformatted(typeToString(output.second.getTypeIndex()).c_str());
                    ImGui::SetWindowFontScale(1.0f);
                    columnWidths[6] = std::max(columnWidths[6], ImGui::GetItemRectSize().x);

                    ImGui::TableNextColumn();

                    const ed::PinId pinId = output.second.getId();
                    BeginPin(pinId, ed::PinKind::Output);
                    ImGui::SetWindowFontScale(1.5f); // Scale up the font by 1.5
                    ImGui::TextUnformatted(reinterpret_cast<const char *>(ICON_FA_CARET_RIGHT));
                    ImGui::SetWindowFontScale(1.0f); // Reset the font scale to default

                    ed::EndPin();

                    columnWidths[7] = std::max(columnWidths[7], ImGui::GetItemRectSize().x);
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    void NodeView::viewInputNode(nodes::NodeBase & node)
    {
        if (!m_currentModel)
        {
            return;
        }
        header(node);

        auto & columnWidths = getOrCreateColumnWidths(node.getId());

        ImGui::PushID(node.getId());
        auto widthOutputs = columnWidths[6] + columnWidths[7];
        if (ImGui::BeginTable("InputAndOutputs",
                              2,
                              ImGuiTableFlags_SizingStretchProp,
                              ImVec2(columnWidths[0] + widthOutputs, 0)))
        {
            ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthFixed, columnWidths[0]);
            ImGui::TableSetupColumn("Outputs", ImGuiTableColumnFlags_WidthFixed, widthOutputs);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            outputPins(node);
            columnWidths[0] = 0.f;
            for (auto & parameter : node.parameter())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                inputControls(node, parameter);
            }
        }
        ImGui::EndTable();
        ImGui::PopID();
        footer(node);
    }

    ImVec4 NodeView::typeToColor(std::type_index tyepIndex)
    {
        ImVec4 color = {1.f, 1.f, 1.f, 1.f};
        if (tyepIndex == ParameterTypeIndex::Float)
        {
            color = LinkColors::ColorFloat;
        }
        else if (tyepIndex == ParameterTypeIndex::Float3)
        {
            color = LinkColors::ColorFloat3;
        }
        else if (tyepIndex == ParameterTypeIndex::Matrix4)
        {
            color = LinkColors::ColorMatrix;
        }
        else if (tyepIndex == ParameterTypeIndex::ResourceId)
        {
            color = LinkColors::ColorResource;
        }
        else if (tyepIndex == ParameterTypeIndex::String)
        {
            color = LinkColors::ColorString;
        }
        else if (tyepIndex == ParameterTypeIndex::Int)
        {
            color = LinkColors::ColorInt;
        }
        return color;
    }

    ColumnWidths & NodeView::getOrCreateColumnWidths(nodes::NodeId nodeId)
    {
        auto it = m_columnWidths.find(nodeId);
        if (it == std::end(m_columnWidths))
        {
            it = m_columnWidths.insert({nodeId, ColumnWidths{0, 0, 0, 0, 0, 0, 0, 0}}).first;
        }
        return it->second;
    }

    bool NodeView::columnWidthsAreInitialized() const
    {
        return !m_columnWidths.empty();
    }

    nodes::NodeBase * NodeView::findNodeById(nodes::NodeId nodeId) const
    {
        if (!m_currentModel)
        {
            return nullptr;
        }

        // Create a finder class that searches for a node with specific ID
        class NodeFinder : public nodes::Visitor
        {
          public:
            explicit NodeFinder(nodes::NodeId targetId)
                : m_targetId(targetId)
                , m_foundNode(nullptr)
            {
            }

            void visit(nodes::NodeBase & node) override
            {
                if (node.getId() == m_targetId)
                {
                    m_foundNode = &node;
                }
            }

            // Implement all the visit methods from the base visitor class
            void visit(nodes::Begin & node) override
            {
                visit(static_cast<nodes::NodeBase &>(node));
            }
            void visit(nodes::End & node) override
            {
                visit(static_cast<nodes::NodeBase &>(node));
            }
            void visit(nodes::ConstantScalar & node) override
            {
                visit(static_cast<nodes::NodeBase &>(node));
            }
            void visit(nodes::ConstantVector & node) override
            {
                visit(static_cast<nodes::NodeBase &>(node));
            }
            void visit(nodes::ConstantMatrix & node) override
            {
                visit(static_cast<nodes::NodeBase &>(node));
            }
            void visit(nodes::Transformation & node) override
            {
                visit(static_cast<nodes::NodeBase &>(node));
            }
            void visit(nodes::Resource & node) override
            {
                visit(static_cast<nodes::NodeBase &>(node));
            }

            nodes::NodeBase * getFoundNode() const
            {
                return m_foundNode;
            }

          private:
            nodes::NodeId m_targetId;
            nodes::NodeBase * m_foundNode;
        };

        NodeFinder finder(nodeId);
        m_currentModel->visitNodes(finder);
        return finder.getFoundNode();
    }

    void NodeView::updateNodeGroups()
    {
        if (!m_currentModel)
        {
            m_nodeGroups.clear();
            return;
        }

        // Clear existing groups
        m_nodeGroups.clear();

        // Collect all nodes with tags
        class TagCollector : public nodes::Visitor
        {
          public:
            std::unordered_map<std::string, std::vector<nodes::NodeId>> tagToNodes;

            void visit(nodes::NodeBase & node) override
            {
                std::string const tag = node.getTag();
                if (!tag.empty())
                {
                    tagToNodes[tag].push_back(node.getId());
                }
            }
        };

        TagCollector collector;
        m_currentModel->visitNodes(collector);

        // Create groups for each tag
        for (auto const & [tag, nodeIds] : collector.tagToNodes)
        {
            if (nodeIds.size() > 1) // Only create groups with multiple nodes
            {
                NodeGroup group;
                group.tag = tag;
                group.nodes = nodeIds;
                group.color = generateGroupColor(tag);
                group.minBound = ImVec2(0, 0);
                group.maxBound = ImVec2(500, 500);

                m_nodeGroups[tag] = std::move(group);
            }
        }
    }

    void NodeView::renderNodeGroups()
    {
        if (m_nodeGroups.empty())
        {
            return;
        }

        // Calculate bounds for all groups
        for (auto & [tag, group] : m_nodeGroups)
        {
            calculateGroupBounds(group);
        }

        // Get mouse position for hover effects
        ImVec2 const mousePos = ImGui::GetMousePos();
        std::string const hoveredGroupHeader = getGroupUnderMouseHeader(mousePos);

        for (auto const & [tag, group] : m_nodeGroups)
        {
            // Calculate group rectangle with padding
            ImVec2 groupMin, groupMax;
            if (!calculateGroupRect(group, groupMin, groupMax))
            {
                continue; // Skip groups with no valid nodes
            }

            ax::NodeEditor::NodeId const groupId = ax::NodeEditor::NodeId(
              std::hash<std::string>{}(tag)); // Use a hash of the tag as the node ID

            ImVec2 const groupSize = ImVec2(groupMax.x - groupMin.x, groupMax.y - groupMin.y);

            // Calculate tag dimensions for positioning
            ImVec2 const tagSize = ImGui::CalcTextSize(tag.c_str());
            constexpr float TAG_PADDING = 20.0f;
            constexpr float HEADER_HEIGHT = 50.0f;
            constexpr float BORDER_WIDTH = 10.0f;

            // Position tag at top-left with some offset
            ImVec2 const tagPos = ImVec2(groupMin.x + TAG_PADDING, groupMin.y + TAG_PADDING);

            // Determine if this group's header is hovered for visual feedback
            bool const isHeaderHovered = (hoveredGroupHeader == tag);

            // Set node color based on group color
            ed::PushStyleColor(ed::StyleColor_NodeBg,
                               ImVec4(group.color.x, group.color.y, group.color.z, 0.2f));

            ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 10.0f);

            // Put the z order of the group node to the back
            ed::SetNodeZPosition(groupId, -100.0f);
            ed::BeginNode(groupId);

            ed::BeginGroupHint(groupId);

            ed::SetNodePosition(groupId, groupMin);

            // Style the group node with semi-transparent background
            ImDrawList * drawList = ImGui::GetWindowDrawList();
            ImVec2 const nodeScreenPos = ed::GetNodePosition(groupId);

            // Draw background rectangle with group color (semi-transparent)
            ImVec4 const bgColor = ImVec4(group.color.x, group.color.y, group.color.z, 0.4f);
            ImU32 const bgColorU32 = ImGui::ColorConvertFloat4ToU32(bgColor);
            drawList->AddRectFilled(
              nodeScreenPos,
              ImVec2(nodeScreenPos.x + groupSize.x, nodeScreenPos.y + groupSize.y),
              bgColorU32,
              8.0f);

            // Draw draggable header area with visual feedback
            ImVec4 headerColor =
              isHeaderHovered
                ? ImVec4(group.color.x + 0.2f, group.color.y + 0.2f, group.color.z + 0.2f, 0.8f)
                : ImVec4(group.color.x, group.color.y, group.color.z, 0.6f);
            ImU32 const headerColorU32 = ImGui::ColorConvertFloat4ToU32(headerColor);

            drawList->AddRectFilled(
              nodeScreenPos,
              ImVec2(nodeScreenPos.x + groupSize.x, nodeScreenPos.y + HEADER_HEIGHT),
              headerColorU32,
              8.0f,
              ImDrawFlags_RoundCornersTop);

            // Draw draggable border areas with visual feedback
            if (isHeaderHovered)
            {
                ImU32 const borderHighlight = ImGui::ColorConvertFloat4ToU32(
                  ImVec4(group.color.x + 0.3f, group.color.y + 0.3f, group.color.z + 0.3f, 0.7f));

                // Left border
                drawList->AddRectFilled(
                  ImVec2(nodeScreenPos.x, nodeScreenPos.y + HEADER_HEIGHT),
                  ImVec2(nodeScreenPos.x + BORDER_WIDTH, nodeScreenPos.y + groupSize.y),
                  borderHighlight,
                  0.0f);

                // Right border
                drawList->AddRectFilled(
                  ImVec2(nodeScreenPos.x + groupSize.x - BORDER_WIDTH,
                         nodeScreenPos.y + HEADER_HEIGHT),
                  ImVec2(nodeScreenPos.x + groupSize.x, nodeScreenPos.y + groupSize.y),
                  borderHighlight,
                  0.0f);

                // Bottom border
                drawList->AddRectFilled(ImVec2(nodeScreenPos.x + BORDER_WIDTH,
                                               nodeScreenPos.y + groupSize.y - BORDER_WIDTH),
                                        ImVec2(nodeScreenPos.x + groupSize.x - BORDER_WIDTH,
                                               nodeScreenPos.y + groupSize.y),
                                        borderHighlight,
                                        8.0f,
                                        ImDrawFlags_RoundCornersBottom);
            }

            // Draw drag handle icon in header
            if (isHeaderHovered)
            {
                ImVec2 const handlePos =
                  ImVec2(nodeScreenPos.x + groupSize.x - 30.0f, nodeScreenPos.y + 15.0f);
                ImU32 const handleColor = IM_COL32(255, 255, 255, 200);

                // Draw simple grip lines
                for (int i = 0; i < 3; ++i)
                {
                    float const y = handlePos.y + i * 6.0f;
                    drawList->AddLine(
                      ImVec2(handlePos.x, y), ImVec2(handlePos.x + 16.0f, y), handleColor, 2.0f);
                }
            }

            ImVec2 const tagScreenPos =
              ImVec2(nodeScreenPos.x + TAG_PADDING, nodeScreenPos.y - TAG_PADDING);
            ImVec2 const tagBgMin = ImVec2(tagScreenPos.x - 14.0f, tagScreenPos.y - 12.0f);
            ImVec2 const tagBgMax =
              ImVec2(tagScreenPos.x + tagSize.x + 14.0f, tagScreenPos.y + tagSize.y + 12.0f);

            // Draw tag background
            ImVec4 const tagBgColor = ImVec4(group.color.x, group.color.y, group.color.z, 0.8f);
            ImU32 const tagBgColorU32 = ImGui::ColorConvertFloat4ToU32(tagBgColor);

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0)); // Transparent background
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImU32(tagBgColorU32));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImU32(tagBgColorU32));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

            // Use unique ID for each tag input
            ImGui::PushID(("tag_input_" + tag).c_str());

            static std::string inputBuffer;
            inputBuffer = tag;
            ImGui::SetNextItemWidth(tagSize.x + 20.0f);
            if (ImGui::InputText("##tag_input",
                                 &inputBuffer,
                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                   ImGuiInputTextFlags_AutoSelectAll))
            {
                if (!inputBuffer.empty() && inputBuffer != tag)
                {
                    replaceGroupTag(tag, inputBuffer);
                }
            }

            ImGui::PopID();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);

            ed::EndGroupHint();
            ed::EndNode();

            ed::PopStyleVar();   // Pop the node border width style
            ed::PopStyleColor(); // Pop the node background color style
        }
    }

    bool NodeView::hasGroup(std::string const & tag) const
    {
        return m_nodeGroups.find(tag) != m_nodeGroups.end();
    }

    bool NodeView::replaceGroupTag(const std::string & oldTag, const std::string & newTag)
    {
        if (!m_currentModel || oldTag.empty() || newTag.empty() || oldTag == newTag)
        {
            return false;
        }

        // Check if the old tag exists
        auto groupIt = m_nodeGroups.find(oldTag);
        if (groupIt == m_nodeGroups.end())
        {
            return false;
        }

        // Update all nodes in the group with the new tag
        for (nodes::NodeId nodeId : groupIt->second.nodes)
        {
            if (auto * node = findNodeById(nodeId))
            {
                node->setTag(newTag);
            }
        }

        // Update the group data structure
        NodeGroup updatedGroup = std::move(groupIt->second);
        updatedGroup.tag = newTag;
        m_nodeGroups.erase(groupIt);
        m_nodeGroups[newTag] = std::move(updatedGroup);

        // Mark model as modified
        if (m_modelEditor)
        {
            m_modelEditor->markModelAsModified();
        }

        m_parameterChanged = true;
        m_modelChanged = true;

        return true;
    }

    void NodeView::calculateGroupBounds(NodeGroup & group)
    {
        if (group.nodes.empty())
        {
            // Set default bounds for empty groups
            group.minBound = ImVec2(0, 0);
            group.maxBound = ImVec2(200, 100);
            return;
        }

        // Use helper function to calculate bounds
        ImVec2 groupMin, groupMax;
        if (calculateGroupRect(group, groupMin, groupMax))
        {
            // Extract the actual node bounds (without padding/header)
            constexpr float PADDING = 20.0f;
            constexpr float HEADER_HEIGHT = 50.0f;
            group.minBound = ImVec2(groupMin.x + PADDING, groupMin.y + PADDING + HEADER_HEIGHT);
            group.maxBound = ImVec2(groupMax.x - PADDING, groupMax.y - PADDING);
        }
        else
        {
            // Set default bounds if no valid nodes found
            group.minBound = ImVec2(0, 0);
            group.maxBound = ImVec2(200, 100);
        }
    }

    ImVec4 NodeView::generateGroupColor(std::string const & tag) const
    {
        // Generate a color based on the hash of the tag for consistency
        std::size_t const hash = std::hash<std::string>{}(tag);

        // Use the hash to generate RGB values
        float const hue = static_cast<float>(hash % 360) / 360.0f;
        float const saturation = 0.7f;
        float const value = 0.8f;

        // Convert HSV to RGB
        auto const hsvToRgb = [](float h, float s, float v) -> ImVec4
        {
            float r, g, b;
            int const i = static_cast<int>(h * 6.0f);
            float const f = h * 6.0f - static_cast<float>(i);
            float const p = v * (1.0f - s);
            float const q = v * (1.0f - f * s);
            float const t = v * (1.0f - (1.0f - f) * s);

            switch (i % 6)
            {
            case 0:
                r = v, g = t, b = p;
                break;
            case 1:
                r = q, g = v, b = p;
                break;
            case 2:
                r = p, g = v, b = t;
                break;
            case 3:
                r = p, g = q, b = v;
                break;
            case 4:
                r = t, g = p, b = v;
                break;
            case 5:
                r = v, g = p, b = q;
                break;
            default:
                r = g = b = 0.0f;
                break;
            }

            return ImVec4(r, g, b, 1.0f);
        };

        return hsvToRgb(hue, saturation, value);
    }

    void NodeView::setUiScale(float scale)
    {
        m_uiScale = scale;
    }

    float NodeView::getUiScale() const
    {
        return m_uiScale;
    }

    const std::unordered_map<std::string, NodeGroup> & NodeView::getNodeGroups() const
    {
        return m_nodeGroups;
    }

    std::vector<nodes::NodeId> NodeView::getNodesInSameGroup(nodes::NodeId nodeId) const
    {
        if (!m_currentModel)
        {
            return {};
        }

        // Find the node to get its tag
        nodes::NodeBase * node = findNodeById(nodeId);
        if (!node)
        {
            return {};
        }

        std::string const nodeTag = node->getTag();
        if (nodeTag.empty())
        {
            return {}; // Node is not in a group
        }

        // Find the group and return all nodes in it
        auto groupIt = m_nodeGroups.find(nodeTag);
        if (groupIt != m_nodeGroups.end())
        {
            return groupIt->second.nodes;
        }

        return {};
    }

    void NodeView::handleGroupMovement()
    {
        if (m_nodeGroups.empty() || m_skipGroupMovement)
        {
            return;
        }

        // Check each group node for position changes
        for (const auto & [tag, group] : m_nodeGroups)
        {
            // Generate the group node ID (same way as in renderNodeGroups)
            ed::NodeId const groupNodeId = ed::NodeId(std::hash<std::string>{}(tag));

            ImVec2 const currentPos = ed::GetNodePosition(groupNodeId);

            // Check if we have a previous position for this group node
            auto prevPosIt =
              m_previousNodePositions.find(static_cast<nodes::NodeId>(groupNodeId.Get()));
            if (prevPosIt == m_previousNodePositions.end())
            {
                // Store the initial position
                m_previousNodePositions[static_cast<nodes::NodeId>(groupNodeId.Get())] = currentPos;
                continue;
            }

            ImVec2 const previousPos = prevPosIt->second;

            // Calculate movement delta (with small threshold to avoid floating point precision
            // issues)
            constexpr float MOVEMENT_THRESHOLD = 0.5f;
            ImVec2 const delta = ImVec2(currentPos.x - previousPos.x, currentPos.y - previousPos.y);

            if (std::abs(delta.x) > MOVEMENT_THRESHOLD || std::abs(delta.y) > MOVEMENT_THRESHOLD)
            {
                // Group node has moved, move all nodes in the group by the same delta
                m_skipGroupMovement = true; // Prevent recursion

                for (nodes::NodeId const nodeId : group.nodes)
                {
                    ImVec2 const nodeCurrentPos = ed::GetNodePosition(nodeId);
                    ImVec2 const newPos =
                      ImVec2(nodeCurrentPos.x + delta.x, nodeCurrentPos.y + delta.y);
                    ed::SetNodePosition(nodeId, newPos);
                }

                m_skipGroupMovement = false; // Re-enable group movement

                // Update the stored position for the group node
                m_previousNodePositions[static_cast<nodes::NodeId>(groupNodeId.Get())] = currentPos;
            }
        }
    }
    void NodeView::handleGroupDragging()
    {
        if (m_nodeGroups.empty())
        {
            return;
        }

        ImVec2 const mousePos = ImGui::GetMousePos();

        // Check for starting a drag operation
        if (!m_isDraggingGroup && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            std::string const groupUnderMouse = getGroupUnderMouseHeader(mousePos);
            if (!groupUnderMouse.empty())
            {
                m_isDraggingGroup = true;
                m_draggingGroup = groupUnderMouse;
                m_groupDragStartPos = mousePos;
            }
        }

        // Handle active dragging
        if (m_isDraggingGroup && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto groupIt = m_nodeGroups.find(m_draggingGroup);
            if (groupIt != m_nodeGroups.end())
            {
                ImVec2 const currentMousePos = ImGui::GetMousePos();
                ImVec2 const frameDelta = ImVec2(currentMousePos.x - m_groupDragStartPos.x,
                                                 currentMousePos.y - m_groupDragStartPos.y);

                // Apply delta to all nodes in the group
                m_skipGroupMovement = true;
                for (nodes::NodeId const nodeId : groupIt->second.nodes)
                {
                    ImVec2 const currentPos = ed::GetNodePosition(nodeId);
                    ed::SetNodePosition(
                      nodeId, ImVec2(currentPos.x + frameDelta.x, currentPos.y + frameDelta.y));
                }
                m_skipGroupMovement = false;
                m_groupDragStartPos = currentMousePos;
            }
        }

        // End dragging
        if (m_isDraggingGroup && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_isDraggingGroup = false;
            m_draggingGroup.clear();
        }
    }

    // Helper function to calculate group bounds
    bool
    NodeView::calculateGroupRect(const NodeGroup & group, ImVec2 & minOut, ImVec2 & maxOut) const
    {
        if (group.nodes.empty())
            return false;

        ImVec2 minBound = ImVec2(FLT_MAX, FLT_MAX);
        ImVec2 maxBound = ImVec2(-FLT_MAX, -FLT_MAX);
        bool hasValidNodes = false;

        for (nodes::NodeId const nodeId : group.nodes)
        {
            ImVec2 const nodePos = ed::GetNodePosition(nodeId);
            ImVec2 const nodeSize = ed::GetNodeSize(nodeId);

            if (nodeSize.x <= 0.0f || nodeSize.y <= 0.0f)
                continue;

            hasValidNodes = true;
            minBound.x = std::min(minBound.x, nodePos.x);
            minBound.y = std::min(minBound.y, nodePos.y);
            maxBound.x = std::max(maxBound.x, nodePos.x + nodeSize.x);
            maxBound.y = std::max(maxBound.y, nodePos.y + nodeSize.y);
        }

        if (!hasValidNodes || minBound.x >= maxBound.x || minBound.y >= maxBound.y)
            return false;

        constexpr float PADDING = 20.0f;
        constexpr float HEADER_HEIGHT = 50.0f;

        minOut = ImVec2(minBound.x - PADDING, minBound.y - PADDING - HEADER_HEIGHT);
        maxOut = ImVec2(maxBound.x + PADDING, maxBound.y + PADDING);

        return true;
    }

    std::string NodeView::getGroupUnderMouseHeader(const ImVec2 & mousePos) const
    {
        constexpr float HEADER_HEIGHT = 50.0f;
        constexpr float BORDER_WIDTH = 10.0f;

        for (const auto & [tag, group] : m_nodeGroups)
        {
            ImVec2 groupMin, groupMax;
            if (!calculateGroupRect(group, groupMin, groupMax))
                continue;

            // Check header (top area)
            if (mousePos.y >= groupMin.y && mousePos.y <= groupMin.y + HEADER_HEIGHT &&
                mousePos.x >= groupMin.x && mousePos.x <= groupMax.x)
            {
                return tag;
            }

            // Check borders (left, right, bottom)
            bool inLeftBorder =
              (mousePos.x >= groupMin.x && mousePos.x <= groupMin.x + BORDER_WIDTH &&
               mousePos.y >= groupMin.y + HEADER_HEIGHT && mousePos.y <= groupMax.y);

            bool inRightBorder =
              (mousePos.x >= groupMax.x - BORDER_WIDTH && mousePos.x <= groupMax.x &&
               mousePos.y >= groupMin.y + HEADER_HEIGHT && mousePos.y <= groupMax.y);

            bool inBottomBorder =
              (mousePos.y >= groupMax.y - BORDER_WIDTH && mousePos.y <= groupMax.y &&
               mousePos.x >= groupMin.x + BORDER_WIDTH && mousePos.x <= groupMax.x - BORDER_WIDTH);

            if (inLeftBorder || inRightBorder || inBottomBorder)
            {
                return tag;
            }
        }

        return "";
    }

    bool NodeView::isMouseOverGroupInterior(const ImVec2 & mousePos) const
    {
        constexpr float HEADER_HEIGHT = 50.0f;
        constexpr float BORDER_WIDTH = 10.0f;

        for (const auto & [tag, group] : m_nodeGroups)
        {
            ImVec2 groupMin, groupMax;
            if (!calculateGroupRect(group, groupMin, groupMax))
                continue;

            // Check interior (excluding header and borders)
            ImVec2 interiorMin(groupMin.x + BORDER_WIDTH, groupMin.y + HEADER_HEIGHT);
            ImVec2 interiorMax(groupMax.x - BORDER_WIDTH, groupMax.y - BORDER_WIDTH);

            if (mousePos.x >= interiorMin.x && mousePos.x <= interiorMax.x &&
                mousePos.y >= interiorMin.y && mousePos.y <= interiorMax.y)
            {
                return true;
            }
        }

        return false;
    }

    std::string NodeView::checkForGroupClick() const
    {
        if (!ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            return "";
        }

        ImVec2 const mousePos = ImGui::GetMousePos();

        // Check if mouse is over any group
        for (const auto & [tag, group] : m_nodeGroups)
        {
            ImVec2 groupMin, groupMax;
            if (!calculateGroupRect(group, groupMin, groupMax))
                continue;

            // Check if mouse is within group bounds
            if (mousePos.x >= groupMin.x && mousePos.x <= groupMax.x && mousePos.y >= groupMin.y &&
                mousePos.y <= groupMax.y)
            {
                return tag;
            }
        }

        return "";
    }

    void NodeView::handleGroupClick(const std::string & groupTag)
    {
        if (groupTag.empty() || !m_currentModel)
        {
            return;
        }

        auto groupIt = m_nodeGroups.find(groupTag);
        if (groupIt == m_nodeGroups.end())
        {
            return;
        }

        // Clear current selection
        ed::ClearSelection();

        // Select all nodes in the group
        for (nodes::NodeId nodeId : groupIt->second.nodes)
        {
            ed::SelectNode(nodeId, true);
        }
    }

} // namespace gladius::ui
