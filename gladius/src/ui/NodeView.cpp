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
        m_nodeGroupsNeedUpdate = true;
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

        const auto styleIter = NodeColors.find(baseNode.getCategory());
        m_popStyle = false;
        if (styleIter != std::end(NodeColors))
        {
            const auto style = styleIter->second;
            ed::PushStyleColor(ed::StyleColor_NodeBorder, style.color);
            ed::PushStyleColor(
              ed::StyleColor_NodeBg,
              ImColor(style.color.Value.x * 0.2f, style.color.Value.y * 0.2f, style.color.Value.z * 0.2f, 0.8f));
            m_popStyle = true;
        }

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

        // Add tag editing functionality
        ImGui::SameLine();
        std::string tag = baseNode.getTag();
        bool hasTag = !tag.empty();

        // If node has a tag, show a colored indicator
        if (hasTag)
        {
            ImVec4 tagColor = generateGroupColor(tag);
            ImVec4 brightTagColor(tagColor.x, tagColor.y, tagColor.z, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, brightTagColor);

            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(("Tag: " + tag).c_str());
                ImGui::EndTooltip();
            }
        }
        else
        {
            // No tag, show add tag button
            if (ImGui::Button(ICON_FA_PLUS ICON_FA_TAG))
            {
                ImGui::OpenPopup(("Tag_" + std::to_string(baseNode.getId())).c_str());
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("Add Tag");
                ImGui::EndTooltip();
            }
        }

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
                    columnWidths[7] = std::max(columnWidths[7], ImGui::GetItemRectSize().x);
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

    void NodeView::updateNodeGroups()
    {
        if (!m_currentModel || !m_nodeGroupsNeedUpdate)
        {
            return;
        }

        // Clear existing groups
        m_nodeGroups.clear();

        // Map to temporarily store nodes by tag
        std::unordered_map<std::string, std::vector<nodes::NodeId>> nodesByTag;

        // Since Model doesn't have direct iterator access, we need to use the NodeRegistry
        // We can approximate this by visiting all nodes and collecting tags
        class TagCollector : public nodes::Visitor
        {
          public:
            std::unordered_map<std::string, std::vector<nodes::NodeId>> & nodesByTag;

            explicit TagCollector(
              std::unordered_map<std::string, std::vector<nodes::NodeId>> & byTag)
                : nodesByTag(byTag)
            {
            }

            void visit(nodes::NodeBase & node) override
            {
                const std::string & tag = node.getTag();

                // Skip nodes without tags
                if (!tag.empty())
                {
                    nodesByTag[tag].push_back(node.getId());
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

        } collector(nodesByTag);

        // Visit all nodes in the model to collect tag information
        if (m_currentModel)
        {
            m_currentModel->visitNodes(collector);
        }

        // Create node groups from the map
        for (const auto & [tag, nodeIds] : nodesByTag)
        {
            // Only create groups with at least 2 nodes
            if (nodeIds.size() >= 2)
            {
                NodeGroup group;
                group.tag = tag;
                group.nodes = nodeIds;
                group.color = generateGroupColor(tag);

                // Calculate group bounds
                calculateGroupBounds(group);

                m_nodeGroups.push_back(std::move(group));
            }
        }

        m_nodeGroupsNeedUpdate = false;
    }

    void NodeView::calculateGroupBounds(NodeGroup & group)
    {
        if (group.nodes.empty() || !m_currentModel)
        {
            return;
        }

        // Initialize with extreme values
        group.minBound = ImVec2(FLT_MAX, FLT_MAX);
        group.maxBound = ImVec2(-FLT_MAX, -FLT_MAX);

        // Padding around nodes (in screen coordinates)
        const float PADDING = 20.0f * m_uiScale;

        for (const auto & nodeId : group.nodes)
        {
            auto node = findNodeById(nodeId);
            if (!node)
            {
                continue;
            }

            // Get position and size from node editor
            ImVec2 pos = ImVec2(node->screenPos().x, node->screenPos().y);
            ImVec2 size = ed::GetNodeSize(nodeId);

            // Update bounds
            group.minBound.x = std::min(group.minBound.x, pos.x - PADDING);
            group.minBound.y = std::min(group.minBound.y, pos.y - PADDING);
            group.maxBound.x = std::max(group.maxBound.x, pos.x + size.x + PADDING);
            group.maxBound.y = std::max(group.maxBound.y, pos.y + size.y + PADDING);
        }
    }

    bool NodeView::hasGroup(const std::string & tag) const
    {
        return std::any_of(m_nodeGroups.begin(),
                           m_nodeGroups.end(),
                           [&tag](const NodeGroup & group) { return group.tag == tag; });
    }

    ImVec4 NodeView::generateGroupColor(const std::string & tag) const
    {
        // Check if we have a custom color for this tag
        auto customColorIt = m_customGroupColors.find(tag);
        if (customColorIt != m_customGroupColors.end())
        {
            return customColorIt->second;
        }

        // Simple hash function to generate a color from the tag string
        unsigned int hash = 0;
        for (char c : tag)
        {
            hash = hash * 31 + static_cast<unsigned int>(c);
        }

        // Generate a pastel color with good alpha for background
        const float hue = static_cast<float>(hash % 360) / 360.0f;
        const float saturation = 0.3f + 0.2f * static_cast<float>((hash / 360) % 100) / 100.0f;
        const float value = 0.7f + 0.3f * static_cast<float>((hash / 36000) % 100) / 100.0f;

        // Convert HSV to RGB
        int h_i = static_cast<int>(hue * 6);
        float f = hue * 6 - h_i;
        float p = value * (1 - saturation);
        float q = value * (1 - f * saturation);
        float t = value * (1 - (1 - f) * saturation);

        float r, g, b;
        switch (h_i % 6)
        {
        case 0:
            r = value;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = value;
            b = p;
            break;
        case 2:
            r = p;
            g = value;
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = value;
            break;
        case 4:
            r = t;
            g = p;
            b = value;
            break;
        case 5:
            r = value;
            g = p;
            b = q;
            break;
        default:
            r = g = b = 0;
            break;
        }

        return ImVec4(r, g, b, 0.3f); // Semi-transparent background
    }

    void NodeView::renderNodeGroups()
    {
        if (m_nodeGroups.empty())
        {
            return;
        }

        // Update group bounds since nodes might have moved
        for (auto & group : m_nodeGroups)
        {
            calculateGroupBounds(group);
        }

        // Sort groups by size (largest first) to handle nested groups properly
        std::sort(m_nodeGroups.begin(),
                  m_nodeGroups.end(),
                  [](const NodeGroup & a, const NodeGroup & b)
                  {
                      float areaA = (a.maxBound.x - a.minBound.x) * (a.maxBound.y - a.minBound.y);
                      float areaB = (b.maxBound.x - b.minBound.x) * (b.maxBound.y - b.minBound.y);
                      return areaA > areaB;
                  });

        // Draw all groups
        for (auto & group : m_nodeGroups)
        {
            // Begin group hint with the first node in the group as the "owner"
            if (ed::BeginGroupHint(group.nodes[0]))
            {
                // Get the background and foreground draw lists
                auto * drawList = ed::GetHintBackgroundDrawList();
                auto * foreDrawList = ed::GetHintForegroundDrawList();

                // Draw a filled rect with the group color
                drawList->AddRectFilled(group.minBound,
                                        group.maxBound,
                                        ImColor(group.color),
                                        ed::GetStyle().GroupRounding);

                // Draw the border
                const float borderWidth = ed::GetStyle().GroupBorderWidth;
                drawList->AddRect(group.minBound,
                                  group.maxBound,
                                  ImColor(group.color.x, group.color.y, group.color.z, 1.0f),
                                  ed::GetStyle().GroupRounding,
                                  ImDrawFlags_None,
                                  borderWidth);

                // Draw the tag name in the top-left corner with an icon
                ImVec2 textPos = ImVec2(group.minBound.x + 10, group.minBound.y + 5);
                ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
                std::string tagLabel = ICON_FA_TAG " " + group.tag;
                foreDrawList->AddText(textPos, ImColor(textColor), tagLabel.c_str());

                // Check if right-clicked on the group
                ImVec2 mousePos = ImGui::GetMousePos();
                bool isInside = mousePos.x >= group.minBound.x && mousePos.x <= group.maxBound.x &&
                                mousePos.y >= group.minBound.y && mousePos.y <= group.maxBound.y;

                if (isInside && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    ImGui::OpenPopup(("GroupContext_" + group.tag).c_str());
                }

                // Group context menu
                if (ImGui::BeginPopup(("GroupContext_" + group.tag).c_str()))
                {
                    ImGui::Text("Group: %s", group.tag.c_str());
                    ImGui::Separator();

                    // Edit group color
                    ImVec4 color = group.color;
                    if (ImGui::ColorEdit4("Group Color",
                                          (float *) &color,
                                          ImGuiColorEditFlags_NoInputs |
                                            ImGuiColorEditFlags_AlphaBar))
                    {
                        m_customGroupColors[group.tag] = color;
                        group.color = color;
                    }

                    // Button to reset to default color
                    if (ImGui::Button("Reset Color"))
                    {
                        m_customGroupColors.erase(group.tag);
                        group.color = generateGroupColor(group.tag);
                    }

                    ImGui::Separator();

                    // Show nodes in this group
                    if (ImGui::BeginMenu("Nodes in Group"))
                    {
                        for (const auto & nodeId : group.nodes)
                        {
                            auto node = findNodeById(nodeId);
                            if (node)
                            {
                                std::string nodeName = node->getDisplayName();
                                if (ImGui::MenuItem(nodeName.c_str()))
                                {
                                    // Center on node when clicked
                                    ed::CenterNodeOnScreen(nodeId);
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }

                    ImGui::EndPopup();
                }

                ed::EndGroupHint();
            }
        }
    }

    nodes::NodeBase * NodeView::findNodeById(nodes::NodeId nodeId)
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

} // namespace gladius::ui
