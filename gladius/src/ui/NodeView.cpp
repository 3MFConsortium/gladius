#include "NodeView.h"

#include "Assembly.h"
#include "FileChooser.h"
#include "InputList.h"
#include "LinkColors.h"
#include "ModelEditor.h"
#include "Parameter.h"
#include "Style.h"
#include "Widgets.h"
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
        if (baseNode.parameterChangeInvalidatesPayload() && m_parameterChanged)
        {
            m_modelEditor->invalidatePrimitiveData();
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
        m_modelEditor->showPopupMenu(
          [&]()
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

              const auto newSource = inputMenu(*model, parameter.second, parameter.first);
              if (newSource.has_value())
              {
                  model->addLink(newSource.value(), parameter.second.getId(), false);
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
            // Only render if bounds are valid
            if (group.minBound.x < group.maxBound.x && group.minBound.y < group.maxBound.y)
            {
                ax::NodeEditor::NodeId const groupId = ax::NodeEditor::NodeId(
                  std::hash<std::string>{}(tag)); // Use a hash of the tag as the node ID

                // Add padding around the group
                constexpr float PADDING = 20.0f;
                constexpr float HEADER_HEIGHT = 50.0f;
                constexpr float BORDER_WIDTH = 10.0f;

                ImVec2 const paddedMin =
                  ImVec2(group.minBound.x - PADDING, group.minBound.y - PADDING - HEADER_HEIGHT);
                ImVec2 const paddedMax =
                  ImVec2(group.maxBound.x + PADDING, group.maxBound.y + PADDING);
                ImVec2 const groupSize =
                  ImVec2(paddedMax.x - paddedMin.x, paddedMax.y - paddedMin.y);

                // Calculate tag dimensions for positioning
                ImVec2 const tagSize = ImGui::CalcTextSize(tag.c_str());
                constexpr float TAG_PADDING = 20.0f;

                // Position tag at top-left with some offset
                ImVec2 const tagPos = ImVec2(paddedMin.x + TAG_PADDING, paddedMin.y + TAG_PADDING);

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

                ed::SetNodePosition(groupId, paddedMin);

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
                    ImU32 const borderHighlight = ImGui::ColorConvertFloat4ToU32(ImVec4(
                      group.color.x + 0.3f, group.color.y + 0.3f, group.color.z + 0.3f, 0.7f));

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
                        drawList->AddLine(ImVec2(handlePos.x, y),
                                          ImVec2(handlePos.x + 16.0f, y),
                                          handleColor,
                                          2.0f);
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
                ImGui::PushStyleColor(ImGuiCol_FrameBg,
                                      IM_COL32(0, 0, 0, 0)); // Transparent background
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
            return;
        }

        // Reset bounds
        group.minBound = ImVec2(FLT_MAX, FLT_MAX);
        group.maxBound = ImVec2(-FLT_MAX, -FLT_MAX);

        bool hasValidNodes = false;

        // Calculate bounds from all nodes in the group
        for (nodes::NodeId const nodeId : group.nodes)
        {
            // Get node position from ImGui Node Editor
            ImVec2 const nodePos = ed::GetNodePosition(nodeId);
            ImVec2 const nodeSize = ed::GetNodeSize(nodeId);

            // Skip nodes with invalid positions or sizes
            if (nodeSize.x <= 0.0f || nodeSize.y <= 0.0f)
            {
                continue;
            }

            hasValidNodes = true;

            // Update bounds
            group.minBound.x = std::min(group.minBound.x, nodePos.x);
            group.minBound.y = std::min(group.minBound.y, nodePos.y);
            group.maxBound.x = std::max(group.maxBound.x, nodePos.x + nodeSize.x);
            group.maxBound.y = std::max(group.maxBound.y, nodePos.y + nodeSize.y);
        }

        // If no valid nodes were found, set default bounds
        if (!hasValidNodes)
        {
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
        ImGuiIO const & io = ImGui::GetIO();

        // Check for starting a drag operation
        if (!m_isDraggingGroup && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            std::string const groupUnderMouse = getGroupUnderMouseHeader(mousePos);

            // Debug output
            if (!groupUnderMouse.empty())
            {
                printf("DEBUG: Found group under mouse: %s\n", groupUnderMouse.c_str());
            }
            else
            {
                printf("DEBUG: No group under mouse at pos (%.1f, %.1f)\n", mousePos.x, mousePos.y);
            }

            if (!groupUnderMouse.empty())
            {
                // Start dragging this group
                printf("DEBUG: Starting group drag for: %s\n", groupUnderMouse.c_str());
                m_isDraggingGroup = true;
                m_draggingGroup = groupUnderMouse;
                m_groupDragStartPos = mousePos;
                printf("DEBUG: m_isDraggingGroup is now: %s\n",
                       m_isDraggingGroup ? "true" : "false");
            }
        }

        // Handle active dragging
        if (m_isDraggingGroup)
        {
            printf("DEBUG: In dragging mode, checking if mouse is dragging...\n");
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                printf("DEBUG: Mouse is dragging, applying movement...\n");
                auto groupIt = m_nodeGroups.find(m_draggingGroup);
                if (groupIt != m_nodeGroups.end())
                {
                    // Calculate drag delta for this frame
                    ImVec2 const currentMousePos = ImGui::GetMousePos();
                    ImVec2 const frameDelta = ImVec2(currentMousePos.x - m_groupDragStartPos.x,
                                                     currentMousePos.y - m_groupDragStartPos.y);

                    // Only apply movement if there's significant delta to avoid jitter
                    if (std::abs(frameDelta.x) > 0.5f || std::abs(frameDelta.y) > 0.5f)
                    {
                        // Apply delta to all nodes in the group
                        m_skipGroupMovement = true; // Prevent handleGroupMovement from interfering

                        for (nodes::NodeId const nodeId : groupIt->second.nodes)
                        {
                            // Get current position and apply this frame's delta
                            ImVec2 const currentPos = ed::GetNodePosition(nodeId);
                            ImVec2 const newPos =
                              ImVec2(currentPos.x + frameDelta.x, currentPos.y + frameDelta.y);
                            ed::SetNodePosition(nodeId, newPos);
                        }

                        m_skipGroupMovement = false;

                        // Update start position for next frame to avoid accumulating error
                        m_groupDragStartPos = currentMousePos;
                    }
                }
            }
        }

        // End dragging
        if (m_isDraggingGroup && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            printf("DEBUG: Ending group drag\n");
            m_isDraggingGroup = false;
            m_draggingGroup.clear();
        }
    }

    std::string NodeView::getGroupUnderMouseHeader(const ImVec2 & mousePos) const
    {
        // Debug output
        printf("DEBUG: getGroupUnderMouseHeader called with mouse at (%.1f, %.1f)\n",
               mousePos.x,
               mousePos.y);
        printf("DEBUG: Number of groups: %zu\n", m_nodeGroups.size());

        // Check each group to see if the mouse is over its header/border area
        for (const auto & [tag, group] : m_nodeGroups)
        {
            printf("DEBUG: Checking group '%s' with %zu nodes\n", tag.c_str(), group.nodes.size());

            if (group.nodes.empty())
            {
                printf("DEBUG: Group '%s' has no nodes, skipping\n", tag.c_str());
                continue;
            }

            // Calculate fresh bounds for this group (same logic as calculateGroupBounds)
            ImVec2 minBound = ImVec2(FLT_MAX, FLT_MAX);
            ImVec2 maxBound = ImVec2(-FLT_MAX, -FLT_MAX);
            bool hasValidNodes = false;

            // Calculate bounds from all nodes in the group
            for (nodes::NodeId const nodeId : group.nodes)
            {
                // Get node position from ImGui Node Editor
                ImVec2 const nodePos = ed::GetNodePosition(nodeId);
                ImVec2 const nodeSize = ed::GetNodeSize(nodeId);

                // Skip nodes with invalid positions or sizes
                if (nodeSize.x <= 0.0f || nodeSize.y <= 0.0f)
                {
                    continue;
                }

                hasValidNodes = true;

                // Update bounds
                minBound.x = std::min(minBound.x, nodePos.x);
                minBound.y = std::min(minBound.y, nodePos.y);
                maxBound.x = std::max(maxBound.x, nodePos.x + nodeSize.x);
                maxBound.y = std::max(maxBound.y, nodePos.y + nodeSize.y);
            }

            // Only check if bounds are valid
            if (!hasValidNodes || minBound.x >= maxBound.x || minBound.y >= maxBound.y)
            {
                printf("DEBUG: Group '%s' has invalid bounds or no valid nodes\n", tag.c_str());
                continue;
            }

            printf("DEBUG: Group '%s' bounds: min(%.1f, %.1f) max(%.1f, %.1f)\n",
                   tag.c_str(),
                   minBound.x,
                   minBound.y,
                   maxBound.x,
                   maxBound.y);

            // Calculate the group rectangle with padding (same as in renderNodeGroups)
            constexpr float PADDING = 20.0f;
            constexpr float HEADER_HEIGHT = 50.0f;
            constexpr float BORDER_WIDTH = 10.0f; // Draggable border width

            ImVec2 const paddedMin =
              ImVec2(minBound.x - PADDING, minBound.y - PADDING - HEADER_HEIGHT);
            ImVec2 const paddedMax = ImVec2(maxBound.x + PADDING, maxBound.y + PADDING);
            ImVec2 const groupSize = ImVec2(paddedMax.x - paddedMin.x, paddedMax.y - paddedMin.y);

            // Use the calculated bounds directly as the group rectangle position
            ImVec2 const groupRectPos = paddedMin;

            printf("DEBUG: Group '%s' padded rect: pos(%.1f, %.1f) size(%.1f, %.1f)\n",
                   tag.c_str(),
                   groupRectPos.x,
                   groupRectPos.y,
                   groupSize.x,
                   groupSize.y);

            // Define header area (top part of the group)
            ImRect const headerRect(groupRectPos.x,
                                    groupRectPos.y,
                                    groupRectPos.x + groupSize.x,
                                    groupRectPos.y + HEADER_HEIGHT);

            // Define border areas (edges of the group excluding interior)
            ImRect const leftBorder(groupRectPos.x,
                                    groupRectPos.y + HEADER_HEIGHT,
                                    groupRectPos.x + BORDER_WIDTH,
                                    groupRectPos.y + groupSize.y);

            ImRect const rightBorder(groupRectPos.x + groupSize.x - BORDER_WIDTH,
                                     groupRectPos.y + HEADER_HEIGHT,
                                     groupRectPos.x + groupSize.x,
                                     groupRectPos.y + groupSize.y);

            ImRect const bottomBorder(groupRectPos.x + BORDER_WIDTH,
                                      groupRectPos.y + groupSize.y - BORDER_WIDTH,
                                      groupRectPos.x + groupSize.x - BORDER_WIDTH,
                                      groupRectPos.y + groupSize.y);

            // Check if mouse is in header or any border area
            bool inHeader = headerRect.Contains(mousePos);
            bool inLeftBorder = leftBorder.Contains(mousePos);
            bool inRightBorder = rightBorder.Contains(mousePos);
            bool inBottomBorder = bottomBorder.Contains(mousePos);

            printf("DEBUG: Group '%s' mouse check: header=%d left=%d right=%d bottom=%d\n",
                   tag.c_str(),
                   inHeader,
                   inLeftBorder,
                   inRightBorder,
                   inBottomBorder);

            if (inHeader || inLeftBorder || inRightBorder || inBottomBorder)
            {
                printf("DEBUG: Mouse is over group '%s' draggable area!\n", tag.c_str());
                return tag;
            }
        }

        printf("DEBUG: Mouse not over any group header/border\n");
        return "";
    }

    bool NodeView::isMouseOverGroupInterior(const ImVec2 & mousePos) const
    {
        // Check each group to see if the mouse is over its interior (content) area
        for (const auto & [tag, group] : m_nodeGroups)
        {
            if (group.nodes.empty())
            {
                continue;
            }

            // Calculate bounds (same logic as above)
            ImVec2 minBound = ImVec2(FLT_MAX, FLT_MAX);
            ImVec2 maxBound = ImVec2(-FLT_MAX, -FLT_MAX);
            bool hasValidNodes = false;

            for (nodes::NodeId const nodeId : group.nodes)
            {
                ImVec2 const nodePos = ed::GetNodePosition(nodeId);
                ImVec2 const nodeSize = ed::GetNodeSize(nodeId);

                if (nodeSize.x <= 0.0f || nodeSize.y <= 0.0f)
                {
                    continue;
                }

                hasValidNodes = true;
                minBound.x = std::min(minBound.x, nodePos.x);
                minBound.y = std::min(minBound.y, nodePos.y);
                maxBound.x = std::max(maxBound.x, nodePos.x + nodeSize.x);
                maxBound.y = std::max(maxBound.y, nodePos.y + nodeSize.y);
            }

            if (!hasValidNodes || minBound.x >= maxBound.x || minBound.y >= maxBound.y)
            {
                continue;
            }

            constexpr float PADDING = 20.0f;
            constexpr float HEADER_HEIGHT = 50.0f;
            constexpr float BORDER_WIDTH = 10.0f;

            ImVec2 const paddedMin =
              ImVec2(minBound.x - PADDING, minBound.y - PADDING - HEADER_HEIGHT);
            ImVec2 const paddedMax = ImVec2(maxBound.x + PADDING, maxBound.y + PADDING);
            ImVec2 const groupSize = ImVec2(paddedMax.x - paddedMin.x, paddedMax.y - paddedMin.y);

            // Use the calculated bounds directly as the group rectangle position
            ImVec2 const groupRectPos = paddedMin;

            // Define interior area (excluding header and borders)
            ImRect const interiorRect(groupRectPos.x + BORDER_WIDTH,
                                      groupRectPos.y + HEADER_HEIGHT,
                                      groupRectPos.x + groupSize.x - BORDER_WIDTH,
                                      groupRectPos.y + groupSize.y - BORDER_WIDTH);

            if (interiorRect.Contains(mousePos))
            {
                return true;
            }
        }

        return false;
    }

    bool NodeView::shouldSuppressSelectionRect() const
    {
        // Suppress selection rectangle if we're currently dragging a group
        if (m_isDraggingGroup)
        {
            return true;
        }

        // Also suppress if mouse is over a group header (indicating potential drag start)
        ImVec2 const mousePos = ImGui::GetMousePos();
        return !getGroupUnderMouseHeader(mousePos).empty();
    }

    std::string NodeView::checkForGroupClick() const
    {
        // Only check for double-clicks if there are groups and the background was double-clicked
        if (m_nodeGroups.empty() || !ed::IsBackgroundClicked())
        {
            return "";
        }

        // Get the mouse position in screen coordinates
        ImVec2 const mousePos = ImGui::GetMousePos();
        return getGroupUnderMouseHeader(mousePos);
    }

    void NodeView::handleGroupClick(const std::string & groupTag)
    {
        if (groupTag.empty() || !m_currentModel)
        {
            return;
        }

        // Find the group
        auto groupIt = m_nodeGroups.find(groupTag);
        if (groupIt == m_nodeGroups.end())
        {
            return;
        }

        // Clear current selection
        ed::ClearSelection();

        // Select all nodes in the group
        for (nodes::NodeId const nodeId : groupIt->second.nodes)
        {
            ed::SelectNode(nodeId, true); // append = true to add to selection
        }
    }
} // namespace gladius::ui
