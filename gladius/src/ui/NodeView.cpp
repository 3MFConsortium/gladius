

#include "NodeView.h"

#include "InputList.h"
#include "ModelEditor.h"
#include "Parameter.h"
#include "Style.h"
#include "Widgets.h"
#include "imguinodeeditor.h"

#include <components/IconFontCppHeaders/IconsFontAwesome5.h>
#include <filesystem>

#include <imgui_stdlib.h>

#include "Assembly.h"
#include "FileChooser.h"
#include "imgui.h"
#include "nodesfwd.h"

#include <fmt/format.h>
#include <set>

namespace ed = ax::NodeEditor;

namespace gladius::ui
{
    struct LinkColors
    {
        static constexpr ImVec4 ColorFloat = {0.6f, 0.6f, 1.f, 1.f};
        static constexpr ImVec4 ColorFloat3 = {0.6f, 1.f, 0.6f, 1.f};
        static constexpr ImVec4 ColorMatrix = {1.f, 0.6f, 0.6f, 1.f};
        static constexpr ImVec4 ColorResource = {1.f, 1.f, 0.6f, 1.f};
        static constexpr ImVec4 ColorString = {1.f, 0.6f, 1.f, 1.f};
        static constexpr ImVec4 ColorInt = {0.6f, 1.f, 1.f, 1.f};
        static constexpr ImVec4 ColorInvalid = {1.f, 0.f, 0.f, 1.f};
    };

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
            return "Unknown";
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
    }

    void NodeView::setCurrentModel(SharedModel model)
    {
        m_currentModel = std::move(model);
        reset();
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

            auto nodeColorIter = m_nodeTypeToColor.find(typeid(baseNode));
            if (nodeColorIter != std::end(m_nodeTypeToColor))
            {
                const auto & winBgCol = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
                const auto & nodeColor = nodeColorIter->second;
                ImVec4 bgColor = ImVec4(0.95f * winBgCol.x + 0.05f * nodeColor.x,
                                        0.95f * winBgCol.y + 0.05f * nodeColor.y,
                                        0.95f * winBgCol.z + 0.05f * nodeColor.z,
                                        1.0f);
                ImVec4 hoverColor =
                  ImVec4(nodeColor.x * 0.8f, nodeColor.y * 0.8f, nodeColor.z * 0.8f, 1.0f);
                ImVec4 borderColor =
                  ImVec4(nodeColor.x * 0.8f, nodeColor.y * 0.8f, nodeColor.z * 0.8f, 1.0f);

                PushStyleColor(ed::StyleColor_NodeBg, bgColor);
                PushStyleColor(ed::StyleColor_NodeBorder, nodeColor);
                PushStyleColor(ed::StyleColor_HovNodeBorder, nodeColor);

                ImGui::PushStyleColor(ImGuiCol_FrameBg, bgColor);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            }
            else
            {

                const auto & winBgCol = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
                auto & styleCol = style.color.Value;
                PushStyleColor(ed::StyleColor_NodeBg,
                               ImVec4((0.95f * winBgCol.x + 0.05f * styleCol.x),
                                      (0.95f * winBgCol.y + 0.05f * styleCol.y),
                                      (0.95f * winBgCol.z + 0.05f * styleCol.z),
                                      1.0f));

                PushStyleColor(ed::StyleColor_NodeBorder, style.color);
                PushStyleColor(ed::StyleColor_HovNodeBorder, style.hoveredColor);

                ImGui::PushStyleColor(ImGuiCol_FrameBg, static_cast<ImU32>(style.color));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            }

            m_popStyle = true;
        }
        ed::BeginNode(baseNode.getId());
        ImGui::PushID(baseNode.getId());

        ImGui::PushItemWidth(200 * m_uiScale);
        std::string displayName = baseNode.getDisplayName();
        if (ImGui::InputText("", &displayName))
        {
            baseNode.setDisplayName(displayName);
            m_modelEditor->markModelAsModified();
        }
        ImGui::PopItemWidth();

        // ImGui::TextUnformatted(baseNode.getUniqueName().c_str());
        // ImGui::TextUnformatted(fmt::format("Order id {}", baseNode.getOrder()).c_str());
        // ImGui::TextUnformatted(fmt::format("Depth {}", baseNode.getDepth()).c_str());
        // ImGui::TextUnformatted(fmt::format("Node id {}", baseNode.getId()).c_str());
        if (m_popStyle)
        {
            ImGui::PopStyleColor(2);
        }
    }

    void NodeView::content(NodeBase & baseNode)
    {
        showInputAndOutputs(baseNode);
        if (baseNode.parameterChangeInvalidatesPayload() && m_parameterChanged)
        {
            m_modelEditor->invalidatePrimitiveData();
        }
    }

    void NodeView::footer(NodeBase &)
    {
        ed::EndNode();

        if (m_popStyle)
        {
            ed::PopStyleColor(3);
        }
        ImGui::PopID();
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
                if (parameter.first == FieldNames::RadPerMM)
                {
                    increment = 0.0001f;
                    formatString = "%.6f rad/mm";
                }
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
            if (const auto pval = std::get_if<int>(&val))
            {
                ImGui::SameLine();
                m_parameterChanged |= ImGui::DragInt("", pval);
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

        if (m_assembly == nullptr)
        {
            throw std::runtime_error("NodeView: Assembly has to be set");
        }

        if (m_modelEditor == nullptr)
        {
            throw std::runtime_error("NodeView: ModelEditor has to be set");
        }

        if (parameter.second.getContentType() == ContentType::Part && (m_assembly != nullptr) &&
            (m_modelEditor != nullptr))
        {
            if (const auto name = std::get_if<std::string>(&val))
            {
                val = *name;
                ImGui::SameLine();
                ImGui::PushItemWidth(200 * m_uiScale);
                if (ImGui::Button(name->c_str()))
                {
                    m_showContextMenu = true;
                    m_modelEditor->showPopupMenu(
                      [&]()
                      {
                          if (m_showContextMenu)
                          {
                              ImGui::OpenPopup("Parts");
                              m_showContextMenu = false;
                          }

                          if (ImGui::BeginPopup("Parts"))
                          {
                              auto const selfId = m_currentModel->getResourceId();

                              for (auto & [id, model] : m_assembly->getFunctions())
                              {
                                  if (m_currentModel &&
                                      id != selfId) // TODO create a dependency graph for all
                                                    // sub models and check which ones can be
                                                    // used without circular dependency
                                  {
                                      if (ImGui::Button(model->getDisplayName()
                                                          .value_or(fmt::format("# {}", id))
                                                          .c_str()))
                                      {
                                          m_modelEditor->currentModel()->updatePartArguments(
                                            node.getId(), *model, parameter.first);
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

                ImGui::PopItemWidth();
            }
            ImGui::Indent(-20 * m_uiScale);
            return;
        }

        if (viewString(node, parameter, val))
        {
            return;
        }

        if (const auto resKey = std::get_if<ResourceKey>(&val))
        {
            // TODO: create proper Resource Selection Widget

            if (resKey->getFilename().has_value())
            {
                auto name = resKey->getFilename().value().string();

                ImGui::InputText("", &name);

                ImGui::SameLine();
                const auto baseDir = m_assembly->getFilename().remove_filename();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN)))
                {

                    const auto queriedFilename = queryLoadFilename({{"*.stl"}}, baseDir);

                    if (queriedFilename && queriedFilename->has_filename())
                    {
                        const std::filesystem::path filename{queriedFilename.value()};

                        name = filename.string();
                        m_modelEditor->invalidatePrimitiveData();
                        m_modelEditor->markModelAsModified();
                        m_parameterChanged = true;
                        val = ResourceKey{filename};
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Make relative"))
                {
                    const std::filesystem::path filename{name};
                    const auto relativePath = relative(filename, baseDir);
                }
            }
            if (resKey->getResourceId().has_value())
            {
                ImGui::TextUnformatted(resKey->getDisplayName().c_str());
            }
        }
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
              if (model == nullptr)
              {
                  return;
              }

              const auto newSource = inputMenu(*model, parameter.second.getId());
              if (newSource.has_value())
              {
                  model->addLink(newSource.value(), parameter.second.getId());
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

        if (ImGui::BeginTable(
              "InputAndOutputs", 3, ImGuiTableFlags_SizingStretchProp, ImVec2(300 * m_uiScale, 0)))
        {
            ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthFixed, 140.f * m_uiScale);
            ImGui::TableSetupColumn(
              "Seperation", ImGuiTableColumnFlags_WidthFixed, 20.f * m_uiScale);
            ImGui::TableSetupColumn("Outputs", ImGuiTableColumnFlags_WidthFixed, 140.f * m_uiScale);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            inputPins(node);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            outputPins(node);
        }
        ImGui::EndTable();
    }

    void NodeView::inputPins(nodes::NodeBase & node)
    {
        std::set<ParameterId> usedPins;
        if (ImGui::BeginTable(
              "table", 2, ImGuiTableFlags_SizingStretchProp, ImVec2(140 * m_uiScale, 0)))
        {
            ImGui::TableSetupColumn("InputPin", ImGuiTableColumnFlags_None, 10.f * m_uiScale);
            ImGui::TableSetupColumn("InputName", ImGuiTableColumnFlags_None, 120.f * m_uiScale);

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

                ImGui::PushID(parameter.second.getId()); // required for reusing the same labels
                                                         // (that are used as unique ids in ImgUI)
                {
                    bool const inputMissing = !parameter.second.getSource().has_value() &&
                                              parameter.second.isInputSourceRequired();

                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          typeToColor(parameter.second.getTypeIndex()));
                    const ed::PinId pinId = parameter.second.getId();
                    BeginPin(pinId, ed::PinKind::Input);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4 * m_uiScale, 0});

                    if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_CARET_RIGHT)))
                    {
                        showLinkAssignmentMenu(parameter);
                    }

                    ImGui::PopStyleVar();
                    ed::EndPin();
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(120.f * m_uiScale);
                    if (inputMissing)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, LinkColors::ColorInvalid);
                    }
                    ImGui::TextUnformatted((parameter.first).c_str());
                    if (inputMissing)
                    {
                        ImGui::PopStyleColor();
                    }
                    ImGui::PopStyleColor();
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
        if (ImGui::BeginTable(
              "outputs", 2, ImGuiTableFlags_SizingStretchProp, ImVec2(140 * m_uiScale, 0)))
        {
            ImGui::TableSetupColumn("OutputName", ImGuiTableColumnFlags_None, 120.f * m_uiScale);
            ImGui::TableSetupColumn("OutputPin", ImGuiTableColumnFlags_None, 20.f * m_uiScale);
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
                    ImGui::TableNextColumn();
                    const ed::PinId pinId = output.second.getId();
                    BeginPin(pinId, ed::PinKind::Output);
                    ImGui::TextUnformatted(reinterpret_cast<const char *>(ICON_FA_CARET_RIGHT));
                    ed::EndPin();
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

        ImGui::PushID(node.getId());
        if (ImGui::BeginTable(
              "InputAndOutputs", 2, ImGuiTableFlags_SizingStretchProp, ImVec2(380 * m_uiScale, 0)))
        {
            ImGui::TableSetupColumn(
              "Parameter", ImGuiTableColumnFlags_WidthFixed, 240.f * m_uiScale);
            ImGui::TableSetupColumn("Outputs", ImGuiTableColumnFlags_WidthFixed, 140.f * m_uiScale);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            outputPins(node);

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

} // namespace gladius::ui
