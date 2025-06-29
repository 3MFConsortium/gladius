#include <exception>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <imguinodeeditor.h>
#include <iostream>

#include <algorithm>
#include <fmt/format.h>
#include <set>
#include <unordered_map>

#include "../CLMath.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "ComponentsObjectView.h"
#include "Document.h"
#include "MeshResource.h"
#include "ModelEditor.h"
#include "NodeLayoutEngine.h"
#include "NodeView.h"
#include "Port.h"
#include "ResourceView.h"
#include "Style.h"
#include "Widgets.h"
#include "imgui.h"
#include "nodesfwd.h"
#include "ui/LevelSetView.h"
#include "ui/VolumeDataView.h"
#include <nodes/Assembly.h>
#include <nodes/Model.h>

namespace gladius::ui
{
    using namespace nodes;
    ModelEditor::ModelEditor()
    {
        m_editorContext = ed::CreateEditor();
        m_nodeTypeToColor = createNodeTypeToColors();
    }

    ModelEditor::~ModelEditor()
    {
        DestroyEditor(m_editorContext);
    }

    void ModelEditor::resetEditorContext()
    {
        if (m_editorContext != nullptr)
        {
            DestroyEditor(m_editorContext);
        }
        m_editorContext = ed::CreateEditor();
        m_popupMenuFunction = noOp;
        m_assembly = nullptr;
        m_currentModel = nullptr;
    }

    void ModelEditor::onCreateNode()
    {

        if (ed::BeginCreate())
        {
            ed::PinId inputPinId{};
            ed::PinId outputPinId{};
            if (QueryNewLink(&inputPinId, &outputPinId))
            {

                if (inputPinId && outputPinId) // both are valid, let's accept link
                {
                    auto const inId = static_cast<nodes::ParameterId>(inputPinId.Get());
                    auto const outId = static_cast<nodes::PortId>(outputPinId.Get());

                    if (ed::AcceptNewItem())
                    {
                        createUndoRestorePoint("Add link");
                        if (!m_currentModel->addLink(inId, outId) &&
                            !m_currentModel->addLink(outId, inId))
                        {
                            ed::RejectNewItem();
                        }
                        else
                        {
                            markModelAsModified();
                        }
                    }
                    onQueryNewNode();
                }
            }
        }
        ed::EndCreate();

        if (ed::ShowBackgroundContextMenu())
        {
            ed::Suspend();
            auto const currentMousePos = ImGui::GetMousePos();
            ed::Resume();
            showPopupMenu([&, currentMousePos]() { createNodePopup(-1, currentMousePos); });
            m_showCreateNodePopUp = true;
            ImGui::OpenPopup("Create Node");
        }
    }

    void ModelEditor::onDeleteNode()
    {
        if (m_currentModel->isManaged())
        {
            return;
        }

        if (ed::BeginDelete())
        {
            ed::NodeId deletedNodeId;
            createUndoRestorePoint("Delete Node(s)");
            while (QueryDeletedNode(&deletedNodeId))
            {
                if (ed::AcceptDeletedItem())
                {
                    m_currentModel->remove(static_cast<nodes::NodeId>(deletedNodeId.Get()));
                    markModelAsModified();
                }
            }
        }
        ed::EndDelete();
    }

    void ModelEditor::toolBox()
    {

        ImGui::Begin("Tool Box");

        ImGui::SetWindowSize(ImVec2(350, ImGui::GetWindowHeight()));
        auto const frameHeight = ImGui::GetWindowHeight() - 50.f * m_uiScale;
        auto const height = std::max(frameHeight, 150.f * m_uiScale);

        ImGui::BeginChildFrame(ImGui::GetID("Building blocks"),
                               ImVec2(300, height),
                               ImGuiWindowFlags_HorizontalScrollbar);

        // Add a filter text box at the top
        ImGui::TextUnformatted(ICON_FA_SEARCH);
        ImGui::SameLine();
        ImGui::PushItemWidth(200.0f * m_uiScale);
        ImGui::InputText("##NodeFilterToolbox", &m_nodeFilterText);
        ImGui::PopItemWidth();
        ImGui::Separator();

        // Add the user defined functions. Because we do not have a mouse position
        // we use a dummy position
        ImGui::TextUnformatted("Functions");
        functionToolBox(ImVec2(0, 0));

        ImGui::TextUnformatted("Mesh Resources");
        meshResourceToolBox(ImVec2(0, 0));

        ImGui::EndChildFrame();
        ImGui::End();
    }

    bool ModelEditor::isNodeSelected(nodes::NodeId nodeId)
    {
        auto selection = selectedNodes(m_editorContext);
        ed::NodeId id = nodeId;

        return std::find(std::begin(selection), std::end(selection), id) != std::end(selection);
    }

    void ModelEditor::switchModel()
    {
        m_nodePositionsNeedUpdate = true;
    }

    void ModelEditor::outline()
    {

        if (!m_currentModel || !m_assembly)
        {
            return;
        }

        ImGui::Begin("Outline", nullptr, ImGuiWindowFlags_MenuBar);

        // delete unused resources
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>(ICON_FA_TRASH "\tDelete unused resources")))
            {
                m_unusedResources = m_doc->findUnusedResources();

                if (!m_unusedResources.empty())
                {
                    m_showDeleteUnusedResourcesConfirmation = true;
                    ImGui::OpenPopup("Delete Unused Resources");
                }
                else
                {
                    auto logger = m_doc->getSharedLogger();
                    if (logger)
                    {
                        logger->addEvent(
                          {"No unused resources found in the model", events::Severity::Info});
                    }
                }
            }
            ImGui::EndMenuBar();
        }

        if (m_outline.render())
        {
            markModelAsModified();
        }

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;
        ImGui::BeginGroup();
        if (ImGui::TreeNodeEx("Resources", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx("ComponentsObjects", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
            {
                ComponentsObjectView componentsObjectView;
                if (componentsObjectView.render(m_doc))
                {
                    markModelAsModified();
                }
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(ImVec4(0.0f, 0.8f, 0.8f, 0.1f),
                         "Components Objects\n\n"
                         "Design elements that form your model's structure and features.\n"
                         "Use components to reuse repetitive features and to\n"
                         "composite complex parts.\n");

            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx("VolumeData", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
            {
                VolumeDataView volumeDataView;
                if (volumeDataView.render(m_doc))
                {
                    markModelAsModified();
                }
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(
              ImVec4(1.0f, 0.0f, 1.0f, 0.1f),
              "Volume Data\n\n"
              "Define spatially varying properties inside your 3D models.\n"
              "Volume data lets you specify how material properties change throughout\n"
              "the interior of an object, not just on its surface.\n\n"
              "Common uses:\n"
              " Gradual color transitions and material blending\n"
              " Variable density or infill structures\n"
              " Physical properties like elasticity or conductivity\n"
              " Temperature or stress distribution for simulation\n\n"
              "Apply volume data to meshes or level sets using functions with \"pos\" input\n"
              "and appropriate scalar (custom property) or vector (color) outputs for your desired "
              "property.");

            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx("LevelSet", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
            {
                LevelSetView levelSetView;
                if (levelSetView.render(m_doc))
                {
                    markModelAsModified();
                }
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(
              ImVec4(1.0f, 1.0f, 0.0f, 0.1f),
              "Level Sets\n\n"
              "Define your 3D shape using mathematical boundaries instead of triangles.\n"
              "Level sets are perfect for creating smooth, organic shapes and\n"
              "allow for easier mixing between different shapes.\n\n"
              "For a level set you need a function with a \"pos\" vector as input and a scalar "
              "output.\n");

            resourceOutline();

            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx("Functions", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
            {
                functionOutline();
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(ImVec4(0.0f, 0.5f, 1.0f, 0.1f),
                         "Functions\n\n"
                         "These are the building blocks for creating implicit surfaces.\n"
                         "Think of them as tools that let you combine basic shapes like\n"
                         "spheres and cubes into more complex models.\n\n"
                         "You can reference functions in a Level Set to define a geometry\n"
                         "or in a Volume data to define the inner properties of your model.\n");

            ImGui::TreePop();
        }

        ImGui::EndGroup();
        frameOverlay(ImVec4(0.5f, 0.5f, 0.5f, 0.1f));

        ImGui::End();
    }

    void ModelEditor::resourceOutline()
    {
        m_resourceView.render(m_doc);
    }

    void ModelEditor::functionOutline()
    {
        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGui::Indent();

        if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_PLUS "\tAdd function")))
        {
            ImGui::OpenPopup("Add Function");
            m_showAddModel = true;
        }

        ImGui::Unindent();

        for (auto & model : m_assembly->getFunctions())
        {
            if (!model.second)
            {
                continue;
            }

            auto const isAssembly =
              model.second->getResourceId() == m_assembly->assemblyModel()->getResourceId();

            if (isAssembly)
            {
                continue;
            }

            auto & modelName = model.first;
            auto uid = &modelName;
            ImGui::PushID(uid);

            bool const isModelSelected =
              m_currentModel->getResourceId() == model.second->getResourceId();

            if (m_outlineNodeColorLines)
            {
                int i = 0;
                for (auto & node : *model.second)
                {
                    auto & nodeRef = *node.second;
                    auto const colorIter = m_nodeTypeToColor.find(typeid(nodeRef));
                    if (colorIter != std::end(m_nodeTypeToColor))
                    {
                        auto const color = colorIter->second;

                        auto * window = ImGui::GetCurrentWindow();
                        auto const start = ImVec2(i * 2.f, ImGui::GetCursorScreenPos().y);
                        auto const end =
                          ImVec2(start.x + 2.f, start.y + ImGui::GetTextLineHeightWithSpacing());
                        window->DrawList->AddRectFilled(start, end, ImColor(color));
                    }
                    ++i;
                }
            }

            auto modelDisplayName = model.second->getDisplayName();

            std::string const asssemblyLabel = fmt::format("internal graph from builditems");
            std::string const nodeLabel = (isAssembly)
                                            ? asssemblyLabel
                                            : fmt::format("{} #{}",
                                                          modelDisplayName.value_or("function"),
                                                          model.second->getResourceId());
            std::string editableName = nodeLabel;

            if (!model.second->isValid())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
            }

            ImGui::BeginGroup();

            bool nodeOpen =
              ImGui::TreeNodeEx("",
                                baseFlags | (isModelSelected ? ImGuiTreeNodeFlags_Selected : 0),
                                "%s",
                                nodeLabel.c_str());

            if (!model.second->isValid())
            {
                ImGui::PopStyleColor();
            }

            if (ImGui::IsItemClicked())
            {
                readBackNodePositions();
                m_currentModel = model.second;
                m_nodePositionsNeedUpdate = true;
            }

            if (nodeOpen)
            {
                for (auto & [nodeId, node] : *model.second)
                {
                    auto & nodeRef = *node;
                    auto const colorIter = m_nodeTypeToColor.find(typeid(nodeRef));
                    if (colorIter != std::end(m_nodeTypeToColor))
                    {
                        auto const color = colorIter->second;

                        auto * window = ImGui::GetCurrentWindow();
                        auto const start = ImVec2(ImGui::GetCursorScreenPos().x + 10.f,
                                                  ImGui::GetCursorScreenPos().y);
                        auto const end =
                          ImVec2(start.x + 5.f, start.y + ImGui::GetTextLineHeightWithSpacing());
                        window->DrawList->AddRectFilled(start, end, ImColor(color));
                    }

                    ImGuiTreeNodeFlags nodeFlags = baseFlags | ImGuiTreeNodeFlags_Leaf;
                    if (isNodeSelected(nodeId) && isModelSelected)
                    {
                        nodeFlags |= ImGuiTreeNodeFlags_Selected;
                    }

                    bool const isLeafOpen =
                      ImGui::TreeNodeEx(node->getDisplayName().c_str(), nodeFlags);
                    if (ImGui::IsItemClicked())
                    {
                        m_currentModel = model.second;
                        m_nodePositionsNeedUpdate = true;
                        ed::SelectNode(nodeId);
                        ed::NavigateToSelection(true);
                    }
                    if (isLeafOpen)
                    {
                        ImGui::TreePop();
                    }
                }

                if (!isAssembly && !model.second->isManaged())
                {
                    // Check if function can be safely deleted
                    auto safeResult =
                      m_doc->isItSafeToDeleteResource(ResourceKey(model.second->getResourceId()));
                    if (ImGui::Button("Delete"))
                    {
                        if (safeResult.canBeRemoved)
                        {
                            m_doc->deleteFunction(model.second->getResourceId());
                            m_currentModel = m_assembly->assemblyModel();
                            m_dirty = true;
                        }
                    }

                    // Display tooltip with dependency information if function cannot be deleted
                    if (!safeResult.canBeRemoved)
                    {
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextColored(
                              ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                              "Cannot delete, the function is referenced by another item:");
                            for (auto const & depRes : safeResult.dependentResources)
                            {
                                ImGui::BulletText("Resource ID: %u", depRes->GetModelResourceID());
                            }
                            for (auto const & depItem : safeResult.dependentBuildItems)
                            {
                                ImGui::BulletText("Build item: %u", depItem->GetObjectResourceID());
                            }
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Rename"))
                    {
                        m_outlineRenaming = true;
                        ImGui::SetKeyboardFocusHere();
                        ImGui::OpenPopup("Rename");
                        m_newModelName = model.second->getDisplayName().value_or("New function");
                    }

                    if (ImGui::BeginPopup("Rename"))
                    {
                        ImGui::InputText("New Name", &m_newModelName);
                        if (ImGui::Button("Confirm"))
                        {
                            model.second->setDisplayName(m_newModelName);
                            m_outlineRenaming = false;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel"))
                        {
                            m_outlineRenaming = false;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                }

                ImGui::TreePop();
            }

            ImGui::EndGroup();
            frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, isModelSelected ? 0.2f : 0.1f));

            ImGui::PopID();
        }
    }

    void ModelEditor::newModelDialog()
    {
        if (m_showAddModel)
        {
            ImVec2 const center(ImGui::GetIO().DisplaySize.x * 0.5f,
                                ImGui::GetIO().DisplaySize.y * 0.5f);
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::OpenPopup("Add Function");
            if (ImGui::BeginPopupModal(
                  "Add Function", &m_showAddModel, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Create a new function");
                ImGui::Separator();

                // Function name input
                ImGui::InputText("Function name", &m_newModelName);

                // Check for duplicate name
                bool nameExists = false;
                for (auto & [id, model] : m_assembly->getFunctions())
                {
                    if (model && model->getDisplayName().has_value() &&
                        model->getDisplayName().value() == m_newModelName)
                    {
                        nameExists = true;
                        break;
                    }
                }
                if (nameExists)
                {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                                       "Warning: This name is already used for another function.");
                }

                // Function type selection
                static const char * functionTypes[] = {"Empty function",
                                                       "Copy existing function",
                                                       "Levelset template",
                                                       "Wrap existing function"};
                int functionType = static_cast<int>(m_selectedFunctionType);
                ImGui::Combo(
                  "Function type", &functionType, functionTypes, IM_ARRAYSIZE(functionTypes));
                m_selectedFunctionType = static_cast<FunctionType>(functionType);

                // If copy or wrap, show list of available functions
                int availableFunctionCount = 0;
                std::vector<nodes::Model *> availableFunctions;
                std::vector<std::string> availableFunctionNames;
                if (m_selectedFunctionType == FunctionType::CopyExisting ||
                    m_selectedFunctionType == FunctionType::WrapExisting)
                {
                    for (auto & [id, model] : m_assembly->getFunctions())
                    {
                        if (!model || model->isManaged() || model == m_currentModel)
                            continue;
                        availableFunctions.push_back(model.get());
                        availableFunctionNames.push_back(
                          model->getDisplayName().value_or("function"));
                    }
                    availableFunctionCount = static_cast<int>(availableFunctions.size());
                    if (availableFunctionCount == 0)
                    {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1),
                                           "No user functions available to copy.");
                    }
                    else
                    {
                        if (m_selectedSourceFunctionIndex >= availableFunctionCount)
                            m_selectedSourceFunctionIndex = 0;
                        std::vector<const char *> cstrNames;
                        for (auto & s : availableFunctionNames)
                            cstrNames.push_back(s.c_str());
                        ImGui::Combo("Source function",
                                     &m_selectedSourceFunctionIndex,
                                     cstrNames.data(),
                                     availableFunctionCount);
                    }
                }

                bool canCreate = !m_newModelName.empty() &&
                                 ((m_selectedFunctionType != FunctionType::CopyExisting &&
                                   m_selectedFunctionType != FunctionType::WrapExisting) ||
                                  availableFunctionCount > 0);

                if (canCreate && ImGui::Button("Create", ImVec2(120, 0)))
                {
                    nodes::Model * newModel = nullptr;
                    switch (m_selectedFunctionType)
                    {
                    case FunctionType::Empty:
                    default:
                        newModel = &m_doc->createNewFunction();
                        break;
                    case FunctionType::CopyExisting:
                        if (availableFunctionCount > 0)
                            newModel = &m_doc->copyFunction(
                              *availableFunctions[m_selectedSourceFunctionIndex], m_newModelName);
                        break;
                    case FunctionType::WrapExisting:
                        if (availableFunctionCount > 0)
                            newModel = &m_doc->wrapExistingFunction(
                              *availableFunctions[m_selectedSourceFunctionIndex], m_newModelName);
                        break;
                    case FunctionType::LevelsetTemplate:
                        newModel = &m_doc->createLevelsetFunction(m_newModelName);
                        break;
                    }
                    if (newModel)
                    {
                        newModel->setDisplayName(m_newModelName);
                        m_currentModel = m_assembly->findModel(newModel->getResourceId());
                        switchModel();
                        m_showAddModel = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    m_showAddModel = false;
                }
                ImGui::EndPopup();
            }
        }
    }

    void ModelEditor::onQueryNewNode()
    {
        ed::PinId pinId{0};
        if (ed::QueryNewNode(&pinId))
        {
            if (ed::AcceptNewItem())
            {
                auto const portId = static_cast<nodes::PortId>(pinId.Get());
                auto const currentMousePos = ImGui::GetMousePos();
                showPopupMenu([&, portId, currentMousePos]()
                              { createNodePopup(portId, currentMousePos); });
                m_showCreateNodePopUp = true;
                ImGui::OpenPopup("Create Node");
            }
        }
    }

    void ModelEditor::createNodePopup(nodes::PortId srcPortId, ImVec2 mousePos)
    {
        if (m_showCreateNodePopUp)
        {
            ImGui::OpenPopup("Create Node");
            m_showCreateNodePopUp = false;
            // Clear filter text when opening popup
            m_nodeFilterText.clear();
        }

        if (m_currentModel == nullptr)
        {
            throw std::runtime_error("ModelEditor: No model selected");
        }

        static nodes::NodeTypes nodeTypes;
        auto srcPortIter = m_currentModel->getPortRegistry().find(srcPortId);
        bool showOnlyLinkableNodes = true;
        if (srcPortIter == std::end(m_currentModel->getPortRegistry()))
        {
            showOnlyLinkableNodes = false;
        }

        if (ImGui::BeginPopup("Create Node"))
        {
            // Add filter text box at the top of the popup
            ImGui::TextUnformatted(ICON_FA_SEARCH);
            ImGui::SameLine();
            ImGui::PushItemWidth(200.0f * m_uiScale);

            // Auto-focus on the filter input when popup opens
            bool isFirstFrame = ImGui::IsWindowAppearing();

            // Check if any key is pressed and focus the filter input
            bool needsFocus = isFirstFrame;
            auto & io = ImGui::GetIO();
            bool isAnyKeyTyped = io.InputQueueCharacters.Size > 0;

            // Check if backspace is pressed (Backspace isn't in InputQueueCharacters)
            bool isBackspacePressed = io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_Backspace)];

            // Check if a key was pressed and the filter input doesn't already have focus
            if ((isAnyKeyTyped || isBackspacePressed) && !ImGui::IsItemActive())
            {
                needsFocus = true;

                // If any character was typed, update the filter text with it
                if (!isFirstFrame)
                {
                    if (isBackspacePressed)
                    {
                        // Clear filter text on backspace
                        m_nodeFilterText.clear();
                    }
                    else
                    {
                        // Set filter text to the first typed character
                        for (int i = 0; i < io.InputQueueCharacters.Size; i++)
                        {
                            char c = (char) io.InputQueueCharacters[i];
                            if (c >= 32)
                            { // Ignore control characters
                                m_nodeFilterText = c;
                                break; // Only use the first typed character
                            }
                        }
                    }
                }
            }

            if (needsFocus)
            {
                ImGui::SetKeyboardFocusHere();
            }

            if (ImGui::InputText(
                  "##NodeFilter", &m_nodeFilterText, ImGuiInputTextFlags_AutoSelectAll))
            {
                // Filter text changed
            }
            ImGui::PopItemWidth();
            ImGui::Separator();

            // Filter function and mesh resources using the filter text
            functionToolBox(mousePos);
            meshResourceToolBox(mousePos);

            for (auto & [cat, catName] : nodes::CategoryNames)
            {
                if (cat == nodes::Category::Internal)
                {
                    continue;
                }
                auto const category = cat;
                auto const styleIter = NodeColors.find(category);
                if (styleIter != std::end(NodeColors))
                {
                    auto const style = styleIter->second;

                    ImGui::PushStyleColor(ImGuiCol_Button, ImU32(style.color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImU32(style.activeColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImU32(style.hoveredColor));
                    ImGui::PushStyleColor(ImGuiCol_Header, ImU32(style.color));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImU32(style.activeColor));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImU32(style.hoveredColor));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
                }
                auto requiredFieldName =
                  (showOnlyLinkableNodes) ? srcPortIter->second->getShortName() : std::string{};

                staticFor(
                  nodeTypes,
                  [&](auto, auto & node)
                  {
                      auto targetParameterIter = node.parameter().find(requiredFieldName);
                      bool hasRequiredField = targetParameterIter != std::end(node.parameter());

                      pushNodeColor(node);
                      // Check if node matches filter
                      std::string nodeName = node.name();
                      bool matchesFilter = matchesNodeFilter(nodeName);

                      if (matchesFilter && node.getCategory() == category &&
                          (hasRequiredField || !showOnlyLinkableNodes))
                      {
                          if (ImGui::Button(nodeName.c_str()))
                          {
                              createUndoRestorePoint("Create node");
                              auto createdNode = m_currentModel->create(node);
                              auto posOnCanvas = ed::ScreenToCanvas(mousePos);
                              ed::SetNodePosition(createdNode->getId(), posOnCanvas);
                              if (showOnlyLinkableNodes)
                              {
                                  m_currentModel->addLink(
                                    srcPortId, createdNode->parameter()[requiredFieldName].getId());
                              }

                              // Request focus on the newly created node for keyboard-driven
                              // workflow
                              requestNodeFocus(createdNode->getId());

                              markModelAsModified();
                              closePopupMenu();
                          }
                      }
                      popNodeColor(node);
                  });

                if (styleIter != std::end(NodeColors))
                {
                    ImGui::PopStyleColor(7);
                }
            }
            ImGui::EndPopup();
        }
    }

    void ModelEditor::invalidateEverything()
    {
        markModelAsModified();
        m_parameterDirty = true;
        m_dirty = true;
        m_nodePositionsNeedUpdate = true;
    }

    auto ModelEditor::showAndEdit() -> bool
    {
        m_uiScale = ImGui::GetIO().FontGlobalScale * 2.0f;
        if (!m_currentModel || !m_assembly)
        {
            return false;
        }

        bool parameterChanged = false;

        outline();
        newModelDialog();
        showDeleteUnusedResourcesDialog();
        try
        {

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0., 0.));
            if (ImGui::Begin("Model Editor", &m_visible, ImGuiWindowFlags_MenuBar))
            {
                SetCurrentEditor(m_editorContext);

                if (ImGui::BeginMenuBar())
                {
                    if (ImGui::MenuItem("Autolayout"))
                    {
                        autoLayout();
                    }
                    if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_COMPRESS_ARROWS_ALT
                                                                       "\tCenter View")))
                    {
                        ed::NavigateToContent();
                    }

                    m_stateApplyingUndo = false;
                    if (m_history.canUnDo())
                    {
                        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_UNDO "\tUndo")))
                        {
                            undo();
                        }
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                        ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_UNDO "\tUndo"));
                        ImGui::PopStyleColor();
                    }

                    if (m_history.canReDo())
                    {
                        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_REDO "\tRedo")))
                        {
                            redo();
                        }
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                        ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_REDO "\tRedo"));
                        ImGui::PopStyleColor();
                    }

                    toggleButton(
                      {reinterpret_cast<const char *>(ICON_FA_ROBOT "\tCompile automatically")},
                      &m_autoCompile);

                    if (!m_autoCompile)
                    {
                        if (ImGui::MenuItem(
                              reinterpret_cast<const char *>(ICON_FA_HAMMER "\tCompile")))
                        {
                            m_isManualCompileRequested = true;
                        }
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("Compile the model");
                        ImGui::Separator();
                        ImGui::TextUnformatted(
                          "If this option is enabled, the model will be compiled automatically "
                          "when it is modified.\n"
                          "If this option is disabled, you have to compile the model manually.");
                        ImGui::EndTooltip();
                    }

                    auto core = m_doc->getCore();
                    bool optimized = core->getCodeGenerator() == CodeGenerator::Code;
                    auto optimizedNewState = optimized;

                    // The command stream is currently not working
                    // toggleButton(
                    //   {reinterpret_cast<const char *>(ICON_FA_STOPWATCH "\tJIT optimized")},
                    //   &optimizedNewState);

                    if (optimizedNewState != optimized)
                    {
                        core->setCodeGenerator((optimizedNewState) ? CodeGenerator::Code
                                                                   : CodeGenerator::CommandStream);
                        invalidateEverything();
                    }

                    // automatic update of the bounding box
                    bool autoUpdateBoundingBox = core->isAutoUpdateBoundingBoxEnabled();
                    toggleButton(
                      {reinterpret_cast<const char *>(ICON_FA_BOXES "\tAuto update bounding box")},
                      &autoUpdateBoundingBox);
                    // Tooltip for auto update bounding box
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("Auto update bounding box");
                        ImGui::Separator();
                        ImGui::TextUnformatted(
                          "If enabled, the bounding box will be updated automatically when the "
                          "model is modified.\n"
                          "Deactivate this option to speed up the preview of parameter changes.");
                        ImGui::EndTooltip();
                    }
                    core->setAutoUpdateBoundingBox(autoUpdateBoundingBox);

                    if (!autoUpdateBoundingBox)
                    {
                        if (ImGui::MenuItem("Update bounding box"))
                        {
                            core->resetBoundingBox();
                            core->updateBBox();
                            invalidateEverything();
                        }
                    }

                    bool showResourceNodes = m_nodeViewVisitor.areResourceNodesVisible();
                    toggleButton(
                      {reinterpret_cast<const char *>(ICON_FA_DATABASE "\tResource Nodes")},
                      &showResourceNodes);
                    m_nodeViewVisitor.setResourceNodesVisible(showResourceNodes);

                    // Add group assignment functionality
                    auto selection = selectedNodes(m_editorContext);
                    if (!selection.empty())
                    {
                        if (ImGui::MenuItem(
                              reinterpret_cast<const char *>(ICON_FA_TAGS "\tAdd to Group")))
                        {
                            // TODO: Implement group assignment
                            m_showGroupAssignmentDialog = true;
                        }

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted("Assign selected nodes to a group/tag");
                            ImGui::Separator();
                            ImGui::TextUnformatted(
                              fmt::format("Selected nodes: {}", selection.size()).c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                        ImGui::MenuItem(
                          reinterpret_cast<const char *>(ICON_FA_TAGS "\tAdd to Group"));
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted("Select nodes to assign them to a group");
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::EndMenuBar();
                }

                m_popupMenuFunction();

                ed::SetCurrentEditor(m_editorContext);

                ed::PushStyleColor(ax::NodeEditor::StyleColor_Bg,
                                   ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));

                ed::Begin("Model Editor");

                m_nodeViewVisitor.setAssembly(m_assembly);
                m_nodeViewVisitor.setModelEditor(this);
                if (m_currentModel)
                {
                    m_nodeWidthsInitialized = m_nodeViewVisitor.columnWidthsAreInitialized();
                    m_currentModel->visitNodes(m_nodeViewVisitor);

                    // Update node groups after nodes are rendered and positioned
                    m_nodeViewVisitor.updateNodeGroups();
                }
                onCreateNode();
                onDeleteNode();

                // Handle group movement - detect when nodes are moved and move their group members
                m_nodeViewVisitor.handleGroupMovement();

                // Handle group dragging via header/border areas - must be called before rendering
                m_nodeViewVisitor.handleGroupDragging();

                // Render node group last, to prioritize node interaction
                m_nodeViewVisitor.renderNodeGroups();

                // Check for group double-clicks and handle them AFTER rendering (so bounds are
                // updated)
                std::string doubleClickedGroup = m_nodeViewVisitor.checkForGroupClick();
                if (!doubleClickedGroup.empty())
                {
                    m_nodeViewVisitor.handleGroupClick(doubleClickedGroup);
                }

                ed::End();
                ed::PopStyleColor();

                if (m_nodeViewVisitor.haveParameterChanged())
                {
                    m_dirty = true;
                    parameterChanged = true;
                    m_currentModel->setLogger(m_doc->getSharedLogger());
                    m_currentModel->updateTypes();
                    if (!m_stateApplyingUndo)
                    {
                        auto tmpAssembly = *m_assembly;
                        m_history.storeState(tmpAssembly, "Parameter changed");
                    }
                }

                m_modelWasModified |= m_nodeViewVisitor.hasModelChanged();

                if (m_nodePositionsNeedUpdate)
                {
                    applyNodePositions();
                }
                else
                {
                    readBackNodePositions();
                }
            }

            ImGui::End();
            ImGui::PopStyleVar();
        }
        catch (std::exception & e)
        {
            std::cerr << e.what() << "\n";
        }

        // Render the library browser if visible
        if (m_libraryBrowser.isVisible() && m_doc)
        {
            m_libraryBrowser.render(m_doc);
        }

        if (!m_currentModel->hasBeenLayouted() && m_nodeWidthsInitialized)
        {
            autoLayout();
        }

        m_parameterDirty = parameterChanged;
        return m_parameterDirty;
    }

    void ModelEditor::triggerNodePositionUpdate()
    {
        m_nodePositionsNeedUpdate = true;
    }

    void ModelEditor::showPopupMenu(PopupMenuFunction popupMenuFunction)
    {
        m_popupMenuFunction = std::move(popupMenuFunction);
    }

    void ModelEditor::closePopupMenu()
    {
        m_popupMenuFunction = noOp;
    }

    auto ModelEditor::currentModel() const -> nodes::SharedModel
    {
        return m_currentModel;
    }

    void ModelEditor::setDocument(std::shared_ptr<Document> document)
    {
        if (!document)
        {
            return;
        }
        m_doc = std::move(document);
        setAssembly(m_doc->getAssembly());

        if (m_doc)
        {
            m_libraryBrowser.setLogger(m_doc->getSharedLogger());
        }

        m_outline.setDocument(m_doc);
    }

    void ModelEditor::setAssembly(nodes::SharedAssembly assembly)
    {
        if (!assembly)
        {
            return;
        }

        m_assembly = std::move(assembly);
        m_currentModel = m_assembly->assemblyModel();

        // if there are other models, we switch to the first one
        if (m_assembly->getFunctions().size() > 1)
        {
            // find first function that is not the assembly model
            for (auto & [id, model] : m_assembly->getFunctions())
            {
                if (model->getResourceId() != m_assembly->assemblyModel()->getResourceId())
                {
                    m_currentModel = model;
                    break;
                }
            }
        }

        m_nodePositionsNeedUpdate = true;
        m_history = History();
        switchModel();
    }

    bool ModelEditor::matchesNodeFilter(const std::string & text) const
    {
        if (m_nodeFilterText.empty())
        {
            return true; // No filter active, match everything
        }

        // Case-insensitive comparison
        std::string lowerText = text;
        std::string lowerFilter = m_nodeFilterText;
        std::transform(lowerText.begin(),
                       lowerText.end(),
                       lowerText.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::transform(lowerFilter.begin(),
                       lowerFilter.end(),
                       lowerFilter.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        return lowerText.find(lowerFilter) != std::string::npos;
    }

    void ModelEditor::functionToolBox(ImVec2 mousePos)
    {
        auto functions = m_assembly->getFunctions();
        for (auto & [id, model] : functions)
        {
            if (!model || model->isManaged() || model == m_currentModel)
            {
                continue;
            }

            // Get the display name
            std::string displayName = model->getDisplayName().value_or("function");

            // Check if it matches the filter
            if (!matchesNodeFilter(displayName))
            {
                continue; // Skip this item if it doesn't match the filter
            }

            if (ImGui::Button(displayName.c_str()))
            {
                createUndoRestorePoint("Create node");
                auto posOnCanvas = ed::ScreenToCanvas(mousePos);
                // create function call node
                auto createdNode = m_currentModel->create<nodes::FunctionCall>();
                createdNode->setFunctionId(id); // expands to a ResourceId node during writing
                createdNode->updateInputsAndOutputs(*model);
                m_currentModel->registerInputs(*createdNode);
                m_currentModel->registerOutputs(*createdNode);
                ed::SetNodePosition(createdNode->getId(), posOnCanvas);

                if (model->getDisplayName().has_value())
                {
                    createdNode->setDisplayName(model->getDisplayName().value());
                }

                // Request focus on the newly created node for keyboard-driven workflow
                requestNodeFocus(createdNode->getId());

                markModelAsModified();
            }
        }
    }

    void ModelEditor::meshResourceToolBox(ImVec2 mousePos)
    {
        auto & resourceManager = m_doc->getResourceManager();

        auto const & resources = resourceManager.getResourceMap();

        for (auto const & [key, res] : resources)
        {
            auto const * mesh = dynamic_cast<MeshResource const *>(res.get());
            if (!mesh)
            {
                continue;
            }

            // Get the display name
            std::string displayName = key.getDisplayName();

            // Check if it matches the filter
            if (!matchesNodeFilter(displayName))
            {
                continue; // Skip this item if it doesn't match the filter
            }

            if (ImGui::Button(displayName.c_str()))
            {
                createUndoRestorePoint("Create node");
                auto posOnCanvas = ed::ScreenToCanvas(mousePos);
                auto createdNode = m_currentModel->create<nodes::Resource>();
                createdNode->setResourceId(key.getResourceId().value());
                ed::SetNodePosition(createdNode->getId(), posOnCanvas);

                auto signedDistanceToMesh = m_currentModel->create<nodes::SignedDistanceToMesh>();
                ImVec2 const posOnCanvasWithOffset = ImVec2(posOnCanvas.x + 400, posOnCanvas.y);
                m_currentModel->addLink(createdNode->outputs().at("value").getId(),
                                        signedDistanceToMesh->parameter().at("mesh").getId());

                signedDistanceToMesh->setDisplayName("SD to " + key.getDisplayName());
                ed::SetNodePosition(signedDistanceToMesh->getId(), posOnCanvasWithOffset);

                // Request focus on the SignedDistanceToMesh node as it's more useful to focus on
                requestNodeFocus(signedDistanceToMesh->getId());

                markModelAsModified();
            }
        }
    }

    void ModelEditor::undo()
    {
        if (m_history.canUnDo())
        {
            std::optional<ResourceId> modelId;
            if (currentModel())
            {
                modelId = currentModel()->getResourceId();
            }
            m_stateApplyingUndo = true;
            m_history.undo(m_assembly.get());
            if (modelId.has_value())
            {
                m_currentModel = m_assembly->findModel(modelId.value());
            }
            switchModel();
            invalidateEverything();
        }
    }

    void ModelEditor::redo()
    {
        if (m_history.canReDo())
        {
            std::optional<ResourceId> modelId;
            if (currentModel())
            {
                modelId = currentModel()->getResourceId();
            }
            m_stateApplyingUndo = true;
            m_history.redo(m_assembly.get());
            if (modelId.has_value())
            {
                m_currentModel = m_assembly->findModel(modelId.value());
            }
            switchModel();
            invalidateEverything();
        }
    }

    void ModelEditor::pushNodeColor(nodes::NodeBase & node)
    {
        auto const colorIter = m_nodeTypeToColor.find(typeid(node));
        if (colorIter != std::end(m_nodeTypeToColor))
        {
            ImVec4 const color = colorIter->second;
            ImVec4 const colorDark =
              ImVec4(color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, color.w);
            ImVec4 const colorHovered =
              ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, color.w);
            ImGui::PushStyleColor(ImGuiCol_Button, colorDark);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorHovered);

            ImGui::PushStyleColor(ImGuiCol_Header, colorDark);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, color);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colorHovered);
        }
    }

    void ModelEditor::popNodeColor(nodes::NodeBase & node)
    {
        auto const colorIter = m_nodeTypeToColor.find(typeid(node));
        if (colorIter != std::end(m_nodeTypeToColor))
        {
            ImGui::PopStyleColor(6);
        }
    }

    auto ModelEditor::modelWasModified() const -> bool
    {
        return m_modelWasModified;
    }

    bool ModelEditor::isCompileRequested() const
    {
        if (m_isManualCompileRequested)
        {
            return true;
        }

        if (!m_autoCompile)
        {
            return false;
        }
        return m_modelWasModified;
    }

    void ModelEditor::markModelAsModified()
    {
        // if (!m_autoCompile)
        // {
        //     return;
        // }

        m_modelWasModified = true;
        invalidatePrimitiveData();
    }

    void ModelEditor::markModelAsUpToDate()
    {
        m_modelWasModified = false;
        m_isManualCompileRequested = false;
    }

    void ModelEditor::readBackNodePositions()
    {
        if (!currentModel())
        {
            return;
        }
        for (auto & node : *currentModel())
        {
            if (!node.second)
            {
                continue;
            }
            auto const screenPos = ed::GetNodePosition(node.second->getId());
            node.second->screenPos().x = screenPos.x;
            node.second->screenPos().y = screenPos.y;
        }
        m_nodePositionsNeedUpdate = false;
    }

    void ModelEditor::autoLayout()
    {
        if (currentModel() == nullptr)
        {
            return;
        }

        // if (!m_nodeWidthsInitialized)
        // {
        //     return;
        // }

        createUndoRestorePoint("Autolayout");

        // Use the dedicated layout engine for all layout operations
        gladius::ui::NodeLayoutEngine layoutEngine;
        gladius::ui::NodeLayoutEngine::LayoutConfig config;
        config.nodeDistance = m_nodeDistance;
        config.layerSpacing = m_nodeDistance * 1.5f;
        config.groupPadding = m_nodeDistance * 0.5f;

        layoutEngine.performAutoLayout(*currentModel(), config);

        m_nodePositionsNeedUpdate = true;
    }

    void ModelEditor::applyNodePositions()
    {
        if (currentModel() == nullptr)
        {
            return;
        }
        if (!m_nodePositionsNeedUpdate)
        {
            return;
        }

        m_nodePositionsNeedUpdate = false;

        for (auto & node : *currentModel())
        {
            auto const targetPos = node.second->screenPos();
            ed::SetNodePosition(node.first, {targetPos.x, targetPos.y});
        }
        ed::NavigateToContent();
    }

    void ModelEditor::placeTransformation(nodes::NodeBase & createdNode,
                                          std::vector<ed::NodeId> & selection) const
    {
        if (currentModel() == nullptr)
        {
            return;
        }
        auto selectedNode =
          currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
        if (selectedNode)
        {
            auto const screenPos = selectedNode.value()->screenPos();
            createdNode.screenPos().x = screenPos.x - 400.0f;
            createdNode.screenPos().y = screenPos.y;
        }
        auto const csOutPut = createdNode.getOutputs()[nodes::FieldNames::Pos];
        auto & csInPut = createdNode.parameter()[nodes::FieldNames::Pos];
        for (auto const nodeId : selection)
        {
            (void) nodeId; // only for suppressing warnings about unused nodeId
            auto selNode =
              currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
            if (selNode)
            {
                auto csIter = selNode.value()->parameter().find(nodes::FieldNames::Pos);
                if (csIter != std::end(selNode.value()->parameter()))
                {
                    if (csIter->second.getSource().has_value())
                    {
                        currentModel()->addLink(csIter->second.getSource().value().portId,
                                                csInPut.getId());
                    }
                    currentModel()->addLink(csOutPut.getId(), csIter->second.getId());
                }
            }
        }
    }

    void ModelEditor::placeBoolOp(nodes::NodeBase & createdNode,
                                  std::vector<ed::NodeId> & selection) const
    {
        if (currentModel() == nullptr)
        {
            return;
        }
        if (selection.size() != 2)
        {
            defaultPlacement(createdNode, selection);
            return;
        }

        auto selectedNode =
          currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
        if (selectedNode)
        {
            auto const screenPos = selectedNode.value()->screenPos();
            createdNode.screenPos().x = screenPos.x + 400.0f;
            createdNode.screenPos().y = screenPos.y;
        }
        auto const shapeOutPut = createdNode.getOutputs().at(FieldNames::Shape);
        std::array<nodes::ParameterId, 2> shapeInputs{
          createdNode.parameter()[FieldNames::A].getId(),
          createdNode.parameter()[FieldNames::B].getId()};

        for (auto i = 0; i < 2; i++)
        {
            auto selNode = currentModel()->getNode(static_cast<nodes::NodeId>(selection[i].Get()));
            if (selNode)
            {
                auto shapeIter = selNode.value()->getOutputs().find(FieldNames::Shape);
                if (shapeIter != std::end(selNode.value()->getOutputs()))
                {
                    currentModel()->addLink(shapeIter->second.getId(), shapeInputs[i]);
                }
            }
        }
    }

    void ModelEditor::defaultPlacement(nodes::NodeBase & createdNode,
                                       std::vector<ed::NodeId> & selection) const
    {
        if (currentModel() == nullptr)
        {
            return;
        }
        auto selectedNode =
          currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
        if (selectedNode)
        {
            auto const screenPos = selectedNode.value()->screenPos();
            createdNode.screenPos().x = screenPos.x + 400.0F;
            createdNode.screenPos().y = screenPos.y;
        }
    }

    void ModelEditor::placeNode(nodes::NodeBase & createdNode)
    {
        auto selection = selectedNodes(m_editorContext);
        auto const category = createdNode.getCategory();
        if (!selection.empty())
        {
            switch (category)
            {
            case nodes::Category::Transformation:
                placeTransformation(createdNode, selection);
                break;
            case nodes::Category::BoolOperation:
                placeBoolOp(createdNode, selection);
                break;
            case nodes::Category::Internal:
            case nodes::Category::Primitive:
            case nodes::Category::Alteration:

            case nodes::Category::Lattice:
            case nodes::Category::Misc:
            default:
                defaultPlacement(createdNode, selection);
            }
        }
        m_nodePositionsNeedUpdate = true;
    }

    void ModelEditor::setVisibility(bool visible)
    {
        m_visible = visible;
    }

    auto ModelEditor::isVisible() const -> bool
    {
        return m_visible;
    }

    void ModelEditor::createUndoRestorePoint(std::string const & description)
    {
        if (m_stateApplyingUndo)
        {
            return;
        }
        m_history.storeState(*m_assembly, description);
    }

    void ModelEditor::resetUndo()
    {
        m_history = History();
    }

    auto ModelEditor::primitveDataNeedsUpdate() const -> bool
    {
        return m_primitiveDataDirty;
    }

    void ModelEditor::invalidatePrimitiveData()
    {
        m_primitiveDataDirty = true;
    }

    void ModelEditor::markPrimitiveDataAsUpToDate()
    {
        m_primitiveDataDirty = false;
    }

    auto selectedNodes(ed::EditorContext * const editorContext) -> std::vector<ed::NodeId>
    {
        SetCurrentEditor(editorContext);

        auto const numSelectedItems = ed::GetSelectedObjectCount();
        std::vector<ed::NodeId> selectedNodeIds(numSelectedItems);
        GetSelectedNodes(selectedNodeIds.data(), static_cast<int>(numSelectedItems));
        return selectedNodeIds;
    }

    void ModelEditor::showDeleteUnusedResourcesDialog()
    {
        if (!m_showDeleteUnusedResourcesConfirmation)
        {
            return;
        }

        ImVec2 const center(ImGui::GetIO().DisplaySize.x * 0.5f,
                            ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (!ImGui::IsPopupOpen("Delete Unused Resources"))
        {
            ImGui::OpenPopup("Delete Unused Resources");
        }

        if (ImGui::BeginPopupModal("Delete Unused Resources",
                                   &m_showDeleteUnusedResourcesConfirmation,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (m_unusedResources.empty())
            {
                ImGui::TextUnformatted("No unused resources found in the model.");
            }
            else
            {
                ImGui::Text("The following %zu unused resources will be deleted:",
                            m_unusedResources.size());
                ImGui::Separator();

                ImGui::BeginChild("ResourceList", ImVec2(400, 300), true);

                for (auto const & resource : m_unusedResources)
                {
                    try
                    {
                        Lib3MF_uint32 modelResourceId = resource->GetModelResourceID();
                        ResourceKey key{modelResourceId};
                        std::string resourceName = key.getDisplayName();

                        // Try to determine the resource type
                        std::string resourceType = "Unknown";
                        try
                        {
                            if (std::dynamic_pointer_cast<Lib3MF::CFunction>(resource))
                                resourceType = "Function";
                            else if (std::dynamic_pointer_cast<Lib3MF::CMeshObject>(resource))
                                resourceType = "Mesh";
                            else if (std::dynamic_pointer_cast<Lib3MF::CComponentsObject>(resource))
                                resourceType = "Components";
                            else if (std::dynamic_pointer_cast<Lib3MF::CLevelSet>(resource))
                                resourceType = "Level Set";
                        }
                        catch (const std::exception &)
                        {
                            // Fallback to unknown type
                        }

                        ImGui::Text("%s #%u (%s)",
                                    resourceName.c_str(),
                                    modelResourceId,
                                    resourceType.c_str());
                    }
                    catch (const std::exception & e)
                    {
                        ImGui::Text("Error getting resource info: %s", e.what());
                    }
                }

                ImGui::EndChild();
                ImGui::Separator();

                ImGui::Text("Are you sure you want to delete these resources?");
                ImGui::Text("This action cannot be undone.");
                ImGui::Separator();

                if (ImGui::Button("Delete", ImVec2(120, 0)))
                {
                    m_doc->removeUnusedResources();
                    markModelAsModified();
                    m_showDeleteUnusedResourcesConfirmation = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    m_unusedResources.clear();
                    m_showDeleteUnusedResourcesConfirmation = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
    }
    void ModelEditor::setLibraryRootDirectory(const std::filesystem::path & directory)
    {
        m_libraryBrowser.setRootDirectory(directory);
    }

    void ModelEditor::toggleLibraryVisibility()
    {
        m_libraryBrowser.setVisibility(!m_libraryBrowser.isVisible());
    }

    void ModelEditor::setLibraryVisibility(bool visible)
    {
        m_libraryBrowser.setVisibility(visible);
    }

    bool ModelEditor::isLibraryVisible() const
    {
        return m_libraryBrowser.isVisible();
    }

    void ModelEditor::refreshLibraryDirectories()
    {
        m_libraryBrowser.refreshDirectories();
    }

    void ModelEditor::requestManualCompile()
    {
        m_isManualCompileRequested = true;
    }

    void ModelEditor::autoLayoutNodes(float distance)
    {
        autoLayout();
    }

    void ModelEditor::showCreateNodePopup()
    {
        // Get current mouse position and show the create node popup
        ImVec2 currentMousePos = ImGui::GetMousePos();
        showPopupMenu([&, currentMousePos]() { createNodePopup(-1, currentMousePos); });
        m_showCreateNodePopUp = true;
        ImGui::OpenPopup("Create Node");
    }

    bool ModelEditor::switchToFunction(nodes::ResourceId functionId)
    {
        if (!m_assembly)
        {
            return false;
        }

        auto functionModel = m_assembly->findModel(functionId);
        if (!functionModel)
        {
            return false;
        }

        m_currentModel = functionModel;
        switchModel();
        return true;
    }

    bool ModelEditor::isHovered() const
    {
        // Check if any of the editor windows are hovered
        return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && isVisible();
    }

    void ModelEditor::requestNodeFocus(nodes::NodeId nodeId)
    {
        m_nodeToFocus = nodeId;
        m_shouldFocusNode = true;
    }

    bool ModelEditor::shouldFocusNode(nodes::NodeId nodeId) const
    {
        return m_shouldFocusNode && m_nodeToFocus == nodeId;
    }

    void ModelEditor::clearNodeFocus()
    {
        m_shouldFocusNode = false;
        m_nodeToFocus = 0;
    }
} // namespace gladius::ui
