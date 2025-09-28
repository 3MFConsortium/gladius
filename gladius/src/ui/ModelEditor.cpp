#include <exception>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <imguinodeeditor.h>
#include <iostream>

#include <algorithm>
#include <cctype>
#include <fmt/format.h>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>

// #include "../ExpressionToGraphConverter.h"
// #include "../ExpressionParser.h"
#include "../ExpressionParser.h"
#include "../ExpressionToGraphConverter.h"
#include "../FunctionArgument.h"

#include "../CLMath.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "BeamLatticeView.h"
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

        // Setup expression dialog callbacks
        m_expressionDialog.setOnApplyCallback(
          [this](std::string const & functionName,
                 std::string const & expression,
                 std::vector<FunctionArgument> const & arguments,
                 FunctionOutput const & output)
          { onCreateFunctionFromExpression(functionName, expression, arguments, output); });

        m_expressionDialog.setOnPreviewCallback(
          [this](std::string const & expression)
          {
              // TODO: Preview the expression (maybe show variable values or graph structure)
              // For now, this is a placeholder
          });
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
            m_editorContext = nullptr;
        }
        m_editorContext = ed::CreateEditor();
    }

    void ModelEditor::outline()
    {
        // Minimal, robust outline window that contains resources and functions
        ImGui::Begin("Outline");

        // Resources section
        resourceOutline();

        ImGui::Separator();

        // Functions section (expanded by default)
        ImGui::BeginGroup();
        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth |
                                             ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::TreeNodeEx("Functions", baseFlags))
        {
            functionOutline();
            ImGui::TreePop();
        }
        ImGui::EndGroup();

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

    ImGui::SameLine();
    if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_CALCULATOR "\tExpression")))
    {
        showExpressionDialog();
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

        // if (isAssembly)
        // {
        //     continue;
        // }

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
            // Use history-aware navigation
            navigateToFunction(model.second->getResourceId());
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
                    auto const start =
                      ImVec2(ImGui::GetCursorScreenPos().x + 10.f, ImGui::GetCursorScreenPos().y);
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
                    navigateToFunction(model.second->getResourceId());
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
                    availableFunctionNames.push_back(model->getDisplayName().value_or("function"));
                }
                availableFunctionCount = static_cast<int>(availableFunctions.size());
                if (availableFunctionCount == 0)
                {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No user functions available to copy.");
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

            bool canCreate =
              !m_newModelName.empty() && ((m_selectedFunctionType != FunctionType::CopyExisting &&
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

bool ModelEditor::isNodeSelected(nodes::NodeId nodeId)
{
    // Query selection state from the node editor
    return ed::IsNodeSelected(ed::NodeId(static_cast<uint64_t>(nodeId)));
}

void ModelEditor::onCreateNode()
{
    // Intentionally left empty. Creation of nodes is handled via explicit UI/popups.
}

void ModelEditor::onDeleteNode()
{
    // Intentionally left empty. Deletion of nodes is handled elsewhere.
}

void ModelEditor::switchModel()
{
    // Mark the editor to re-apply node positions and refresh view on next frame
    m_nodePositionsNeedUpdate = true;
    m_dirty = true;
    // Defer selection clearing until an editor is active to avoid calling NodeEditor APIs out of context
    m_pendingClearSelection = true;

    // Schedule an initial auto-layout for models that have no meaningful positions yet,
    // but only once per function to preserve user edits.
    if (m_currentModel)
    {
        m_pendingAutoLayout = !m_currentModel->hasBeenLayouted();
    }
    else
    {
        m_pendingAutoLayout = false;
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

        if (ImGui::InputText("##NodeFilter", &m_nodeFilterText, ImGuiInputTextFlags_AutoSelectAll))
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

            staticFor(nodeTypes,
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
                                        srcPortId,
                                        createdNode->parameter()[requiredFieldName].getId());
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
                // Function extraction refactoring
                {
                    auto selection = selectedNodes(m_editorContext);
                    bool canExtract = !selection.empty();
                    if (!canExtract)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                        ImGui::MenuItem(
                          reinterpret_cast<const char *>(ICON_FA_CODE_BRANCH "\tExtract Function"));
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_CODE_BRANCH
                                                                           "\tExtract Function")))
                        {
                            m_showExtractDialog = true;
                            m_extractFunctionName = "ExtractedFunction";
                        }
                    }
                }

                if (ImGui::MenuItem("Autolayout"))
                {
                    autoLayout();
                }
                if (ImGui::MenuItem(
                      reinterpret_cast<const char *>(ICON_FA_COMPRESS_ARROWS_ALT "\tCenter View")))
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

                // Copy / Paste
                auto selectionForCopy = selectedNodes(m_editorContext);
                if (selectionForCopy.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                    ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_COPY "\tCopy"));
                    ImGui::PopStyleColor();
                }
                else
                {
                    if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_COPY "\tCopy")))
                    {
                        copySelectionToClipboard();
                    }
                }

                if (!hasClipboard())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                    ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_PASTE "\tPaste"));
                    ImGui::PopStyleColor();
                }
                else
                {
                    if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_PASTE "\tPaste")))
                    {
                        // Defer paste until editor is active
                        m_pendingPasteRequest = true;
                    }
                }

                toggleButton(
                  {reinterpret_cast<const char *>(ICON_FA_ROBOT "\tCompile automatically")},
                  &m_autoCompile);

                if (!m_autoCompile)
                {
                    if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_HAMMER "\tCompile")))
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
                toggleButton({reinterpret_cast<const char *>(ICON_FA_DATABASE "\tResource Nodes")},
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
                    ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_TAGS "\tAdd to Group"));
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

            // Clear selection now that the editor context is active
            if (m_pendingClearSelection)
            {
                ed::ClearSelection();
                m_pendingClearSelection = false;
            }

            // Handle any deferred paste request once editor is active
            if (m_pendingPasteRequest)
            {
                m_pendingPasteRequest = false;
                pasteClipboardAtMouse();
            }

            m_nodeViewVisitor.setAssembly(m_assembly);
            m_nodeViewVisitor.setModelEditor(this);
            if (m_currentModel)
            {
                m_nodeWidthsInitialized = m_nodeViewVisitor.columnWidthsAreInitialized();
                m_currentModel->visitNodes(m_nodeViewVisitor);

                // Update node groups after nodes are rendered and positioned
                m_nodeViewVisitor.updateNodeGroups();

                // Perform pending initial auto-layout after nodes are first rendered,
                // so widths/metrics are available to the layout engine.
                if (m_pendingAutoLayout && m_nodeWidthsInitialized)
                {
                    m_pendingAutoLayout = false;
                    autoLayout();
                }
            }
            onCreateNode();
            onDeleteNode();

            // Keyboard copy/paste when editor is focused
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            {
                ImGuiIO & io = ImGui::GetIO();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
                {
                    copySelectionToClipboard();
                }
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
                {
                    m_pendingPasteRequest = true;
                }
            }

            // Handle group movement - detect when nodes are moved and move their group members
            m_nodeViewVisitor.handleGroupMovement();

            // Handle group dragging via header/border areas - must be called before rendering
            m_nodeViewVisitor.handleGroupDragging();

            // Render node group last, to prioritize node interaction
            m_nodeViewVisitor.renderNodeGroups();

            // Allow quick navigation with mouse back/forward buttons when editor is hovered
            if (isHovered())
            {
                // Prefer key-based detection for mouse X buttons using ImGuiKey_* constants
                if (ImGui::IsKeyPressed(ImGuiKey_MouseX1, false))
                {
                    goBack();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_MouseX2, false))
                {
                    goForward();
                }
            }

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

    // Extract Function dialog
    if (m_showExtractDialog)
    {
        ImVec2 const center(ImGui::GetIO().DisplaySize.x * 0.5f,
                            ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Extract Function");
        if (ImGui::BeginPopupModal(
              "Extract Function", &m_showExtractDialog, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create a new function from the selected nodes.");
            ImGui::Separator();
            ImGui::InputText("Function name", &m_extractFunctionName);

            // --- Validation helpers (local lambdas) ---
            auto trimCopy = [](std::string s)
            {
                auto notSpace = [](unsigned char c) { return !std::isspace(c); };
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
                s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
                return s;
            };
            auto isIdentifier = [](std::string const & name)
            {
                if (name.empty())
                    return false;
                auto c0 = static_cast<unsigned char>(name[0]);
                if (!(std::isalpha(c0) || c0 == '_'))
                    return false;
                for (size_t i = 1; i < name.size(); ++i)
                {
                    unsigned char c = static_cast<unsigned char>(name[i]);
                    if (!(std::isalnum(c) || c == '_'))
                        return false;
                }
                return true;
            };
            auto isReserved = [](std::string const & name)
            {
                // Disallow known reserved parameter names
                return name == "FunctionId";
            };

            // Gather selection ids and compute name proposals on first open
            static bool initializedProposals = false;
            if (!initializedProposals)
            {
                initializedProposals = true;
                m_extractInputNames.clear();
                m_extractOutputNames.clear();

                auto selectionIds = selectedNodes(m_editorContext);
                std::set<nodes::NodeId> selection;
                for (auto const & nid : selectionIds)
                    selection.insert(static_cast<nodes::NodeId>(nid.Get()));
                if (m_currentModel && !selection.empty())
                {
                    auto props = nodes::FunctionExtractor::proposeNames(*m_currentModel, selection);
                    for (auto const & e : props.inputs)
                        m_extractInputNames.push_back({e.uniqueKey, e.defaultName, e.type});
                    for (auto const & e : props.outputs)
                        m_extractOutputNames.push_back({e.uniqueKey, e.defaultName, e.type});
                }
            }

            // Compute validation for current names
            std::vector<bool> inputValid(m_extractInputNames.size(), true);
            std::vector<bool> outputValid(m_extractOutputNames.size(), true);
            std::unordered_set<std::string> seenInputs;
            std::unordered_set<std::string> seenOutputs;
            bool allNamesValid = true;
            for (size_t i = 0; i < m_extractInputNames.size(); ++i)
            {
                auto nameTrim = trimCopy(m_extractInputNames[i].name);
                bool v = isIdentifier(nameTrim) && !isReserved(nameTrim) && !nameTrim.empty();
                if (v)
                {
                    if (seenInputs.count(nameTrim))
                        v = false; // duplicate in inputs
                    else
                        seenInputs.insert(nameTrim);
                }
                inputValid[i] = v;
                allNamesValid = allNamesValid && v;
            }
            for (size_t i = 0; i < m_extractOutputNames.size(); ++i)
            {
                auto nameTrim = trimCopy(m_extractOutputNames[i].name);
                bool v = isIdentifier(nameTrim) && !isReserved(nameTrim) && !nameTrim.empty();
                if (v)
                {
                    if (seenOutputs.count(nameTrim))
                        v = false; // duplicate in outputs
                    else
                        seenOutputs.insert(nameTrim);
                }
                outputValid[i] = v;
                allNamesValid = allNamesValid && v;
            }

            // Editable list of argument names
            if (!m_extractInputNames.empty())
            {
                ImGui::Separator();
                ImGui::Text("Inputs (arguments):");
                ImGui::BeginChild("##extract_inputs", ImVec2(500, 150), true);
                for (size_t i = 0; i < m_extractInputNames.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Text("%s", m_extractInputNames[i].key.c_str());
                    ImGui::SameLine();
                    ImGui::PushItemWidth(260.0f * m_uiScale);
                    ImGui::InputText("##argname", &m_extractInputNames[i].name);
                    if (!inputValid[i])
                    {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "invalid");
                    }
                    ImGui::PopItemWidth();
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }

            // Editable list of output names
            if (!m_extractOutputNames.empty())
            {
                ImGui::Separator();
                ImGui::Text("Outputs:");
                ImGui::BeginChild("##extract_outputs", ImVec2(500, 150), true);
                for (size_t i = 0; i < m_extractOutputNames.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(10000 + i));
                    ImGui::Text("%s", m_extractOutputNames[i].key.c_str());
                    ImGui::SameLine();
                    ImGui::PushItemWidth(260.0f * m_uiScale);
                    ImGui::InputText("##outname", &m_extractOutputNames[i].name);
                    ImGui::PopItemWidth();
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            bool valid = !m_extractFunctionName.empty();
            if (!valid)
            {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Please enter a function name.");
            }
            if (valid && ImGui::Button("Extract", ImVec2(120, 0)))
            {
                // Build override maps from edited names
                std::unordered_map<std::string, std::string> inputOverrides;
                std::unordered_map<std::string, std::string> outputOverrides;
                for (auto const & e : m_extractInputNames)
                    inputOverrides[e.key] = e.name;
                for (auto const & e : m_extractOutputNames)
                    outputOverrides[e.key] = e.name;

                // Perform extraction with overrides
                if (m_doc && m_currentModel)
                {
                    auto selectionIds = selectedNodes(m_editorContext);
                    std::set<nodes::NodeId> selection;
                    for (auto const & nid : selectionIds)
                        selection.insert(static_cast<nodes::NodeId>(nid.Get()));
                    nodes::Model & newModel = m_doc->createNewFunction();
                    newModel.setDisplayName(m_extractFunctionName);
                    createUndoRestorePoint("Extract Function");
                    nodes::FunctionExtractor::Result result;
                    bool ok = nodes::FunctionExtractor::extractInto(*m_currentModel,
                                                                    newModel,
                                                                    selection,
                                                                    inputOverrides,
                                                                    outputOverrides,
                                                                    result);
                    if (!ok)
                    {
                        // rollback
                        m_doc->deleteFunction(newModel.getResourceId());
                    }
                    else
                    {
                        if (result.functionCall)
                        {
                            result.functionCall->setFunctionId(newModel.getResourceId());
                            result.functionCall->updateInputsAndOutputs(newModel);
                            m_currentModel->registerInputs(*result.functionCall);
                            m_currentModel->registerOutputs(*result.functionCall);
                            // Place the node near selection center
                            ImVec2 minP{std::numeric_limits<float>::max(),
                                        std::numeric_limits<float>::max()};
                            ImVec2 maxP{-std::numeric_limits<float>::max(),
                                        -std::numeric_limits<float>::max()};
                            for (auto sid : selection)
                            {
                                auto opt = m_currentModel->getNode(sid);
                                if (!opt.has_value())
                                    continue;
                                auto p = opt.value()->screenPos();
                                minP.x = std::min(minP.x, p.x);
                                minP.y = std::min(minP.y, p.y);
                                maxP.x = std::max(maxP.x, p.x);
                                maxP.y = std::max(maxP.y, p.y);
                            }
                            ImVec2 center{(minP.x + maxP.x) * 0.5f, (minP.y + maxP.y) * 0.5f};
                            ed::SetNodePosition(result.functionCall->getId(), center);
                            requestNodeFocus(result.functionCall->getId());
                        }

                        if (m_assembly)
                        {
                            m_assembly->updateInputsAndOutputs();
                        }

                        m_currentModel->setLogger(m_doc->getSharedLogger());
                        m_currentModel->updateTypes();
                        markModelAsModified();
                        switchModel();
                        m_nodePositionsNeedUpdate = true;
                    }
                }
                m_showExtractDialog = false;
                initializedProposals = false;
                m_extractInputNames.clear();
                m_extractOutputNames.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                m_showExtractDialog = false;
                initializedProposals = false;
                m_extractInputNames.clear();
                m_extractOutputNames.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
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
    // Initialize navigation history with the current model
    initNavigationHistory();
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
            m_currentModel->addLink(createdNode->getOutputValue().getId(),
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
        ImVec4 const colorDark = ImVec4(color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, color.w);
        ImVec4 const colorHovered = ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, color.w);
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
    auto selectedNode = currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
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
        auto selNode = currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
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

    auto selectedNode = currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
    if (selectedNode)
    {
        auto const screenPos = selectedNode.value()->screenPos();
        createdNode.screenPos().x = screenPos.x + 400.0f;
        createdNode.screenPos().y = screenPos.y;
    }
    auto const shapeOutPut = createdNode.getOutputs().at(FieldNames::Shape);
    std::array<nodes::ParameterId, 2> shapeInputs{createdNode.parameter()[FieldNames::A].getId(),
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
    auto selectedNode = currentModel()->getNode(static_cast<nodes::NodeId>(selection.back().Get()));
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

    ImVec2 const center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
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

                    ImGui::Text(
                      "%s #%u (%s)", resourceName.c_str(), modelResourceId, resourceType.c_str());
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

void ModelEditor::showExpressionDialog()
{
    m_expressionDialog.show();
}

void ModelEditor::extractSelectedNodesToFunction(const std::string & functionName)
{
    if (!m_doc || !m_currentModel)
    {
        return;
    }

    auto selectionIds = selectedNodes(m_editorContext);
    if (selectionIds.empty())
    {
        return;
    }

    std::set<nodes::NodeId> selection;
    for (auto const & nid : selectionIds)
    {
        selection.insert(static_cast<nodes::NodeId>(nid.Get()));
    }

    // Create new function via Document to ensure 3MF resource exists
    nodes::Model & newModel = m_doc->createNewFunction();
    newModel.setDisplayName(functionName);

    createUndoRestorePoint("Extract Function");

    nodes::FunctionExtractor::Result result;
    bool ok = nodes::FunctionExtractor::extractInto(*m_currentModel, newModel, selection, result);
    if (!ok)
    {
        // Rollback by removing the created empty function
        m_doc->deleteFunction(newModel.getResourceId());
        return;
    }

    // Find the FunctionCall we just created and assign the function id
    nodes::FunctionCall * fc = result.functionCall;
    if (fc)
    {
        fc->setFunctionId(newModel.getResourceId());
        fc->updateInputsAndOutputs(newModel);
        m_currentModel->registerInputs(*fc);
        m_currentModel->registerOutputs(*fc);
        if (auto dn = newModel.getDisplayName(); dn.has_value())
        {
            fc->setDisplayName(dn.value());
        }
        // Place the node near selection center
        ImVec2 minP{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        ImVec2 maxP{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
        for (auto sid : selection)
        {
            auto opt = m_currentModel->getNode(sid);
            if (!opt.has_value())
                continue;
            auto p = opt.value()->screenPos();
            minP.x = std::min(minP.x, p.x);
            minP.y = std::min(minP.y, p.y);
            maxP.x = std::max(maxP.x, p.x);
            maxP.y = std::max(maxP.y, p.y);
        }
        ImVec2 center{(minP.x + maxP.x) * 0.5f, (minP.y + maxP.y) * 0.5f};
        ed::SetNodePosition(fc->getId(), ImVec2(center.x, center.y));
        requestNodeFocus(fc->getId());
    }

    // After wiring, update assembly IOs so downstream validation doesn't crash
    if (m_assembly)
    {
        m_assembly->updateInputsAndOutputs();
    }

    // Track state and UI
    m_currentModel->setLogger(m_doc->getSharedLogger());
    m_currentModel->updateTypes();
    markModelAsModified();
    switchModel();
    m_nodePositionsNeedUpdate = true;
}

void ModelEditor::onCreateFunctionFromExpression(std::string const & functionName,
                                                 std::string const & expression,
                                                 std::vector<FunctionArgument> const & arguments,
                                                 FunctionOutput const & output)
{
    if (!m_doc || !m_assembly || functionName.empty() || expression.empty())
    {
        return;
    }

    try
    {
        // Create a new function model
        nodes::Model & newModel = m_doc->createNewFunction();
        newModel.setDisplayName(functionName);

        // Create a parser instance
        ExpressionParser parser;

        // Convert expression to node graph
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, newModel, parser, arguments, output);

        if (resultNodeId != 0)
        {
            // Successfully created the graph - switch to the new function
            m_currentModel = m_assembly->findModel(newModel.getResourceId());
            switchModel();
            markModelAsModified();

            // Close the dialog
            m_expressionDialog.hide();
        }
        else
        {
            // Failed to convert expression - remove the created function
            m_doc->deleteFunction(newModel.getResourceId());

            // TODO: Show error message to user
            std::cerr << "Failed to convert expression to graph: " << expression << std::endl;
        }
    }
    catch (std::exception const & ex)
    {
        // Handle conversion errors
        std::cerr << "Error creating function from expression: " << ex.what() << std::endl;
        // TODO: Show error message to user
    }
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

bool ModelEditor::navigateToFunction(nodes::ResourceId functionId)
{
    if (!m_assembly)
    {
        return false;
    }

    auto target = m_assembly->findModel(functionId);
    if (!target)
    {
        return false;
    }

    // Don't record no-op navigations
    nodes::ResourceId const currentId = m_currentModel ? m_currentModel->getResourceId() : 0u;
    if (currentId == functionId)
    {
        return true;
    }

    if (!m_inHistoryNav)
    {
        // If we're not at the end, truncate forward history
        if (!m_navHistory.empty() && (m_navIndex + 1u) < m_navHistory.size())
        {
            m_navHistory.erase(m_navHistory.begin() + static_cast<long>(m_navIndex + 1u),
                               m_navHistory.end());
        }
        // If history is empty, seed with current
        if (m_navHistory.empty() && currentId != 0u)
        {
            m_navHistory.push_back(currentId);
        }
        // Push new target and advance index
        m_navHistory.push_back(functionId);
        m_navIndex = m_navHistory.size() - 1u;
    }

    return switchToFunction(functionId);
}

bool ModelEditor::canGoBack() const
{
    return !m_navHistory.empty() && m_navIndex > 0u;
}

bool ModelEditor::canGoForward() const
{
    return !m_navHistory.empty() && (m_navIndex + 1u) < m_navHistory.size();
}

bool ModelEditor::goBack()
{
    if (!canGoBack())
    {
        return false;
    }
    m_inHistoryNav = true;
    m_navIndex -= 1u;
    auto const targetId = m_navHistory[m_navIndex];
    bool const ok = switchToFunction(targetId);
    m_inHistoryNav = false;
    return ok;
}

bool ModelEditor::goForward()
{
    if (!canGoForward())
    {
        return false;
    }
    m_inHistoryNav = true;
    m_navIndex += 1u;
    auto const targetId = m_navHistory[m_navIndex];
    bool const ok = switchToFunction(targetId);
    m_inHistoryNav = false;
    return ok;
}

void ModelEditor::initNavigationHistory()
{
    m_navHistory.clear();
    m_navIndex = 0u;
    if (m_currentModel)
    {
        m_navHistory.push_back(m_currentModel->getResourceId());
        m_navIndex = 0u;
    }
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

bool ModelEditor::hasClipboard() const
{
    return static_cast<bool>(m_clipboardModel);
}

void ModelEditor::copySelectionToClipboard()
{
    if (!m_currentModel)
    {
        return;
    }

    auto selection = selectedNodes(m_editorContext);
    if (selection.empty())
    {
        return;
    }

    std::set<nodes::NodeId> selectedIds;
    for (auto const & n : selection)
    {
        selectedIds.insert(static_cast<nodes::NodeId>(n.Get()));
    }

    m_clipboardModel = std::make_unique<nodes::Model>();

    std::unordered_map<nodes::NodeId, nodes::NodeBase *> cloneMap;

    // Clone nodes
    for (auto const & [id, nodePtr] : *m_currentModel)
    {
        if (!nodePtr || selectedIds.find(id) == selectedIds.end())
            continue;
        auto cloned = nodePtr->clone();
        cloned->screenPos() = nodePtr->screenPos();
        auto * inserted = m_clipboardModel->insert(std::move(cloned));
        cloneMap[id] = inserted;
    }

    // Recreate intra-selection links
    for (auto const & [origId, clonedNode] : cloneMap)
    {
        (void) clonedNode;
        auto origOpt = m_currentModel->getNode(origId);
        if (!origOpt.has_value())
            continue;
        nodes::NodeBase const * origNode = origOpt.value();
        for (auto const & [paramName, param] : origNode->constParameter())
        {
            if (!param.getConstSource().has_value())
                continue;
            auto const & src = param.getConstSource().value();
            nodes::Port const * srcPort = m_currentModel->getPort(src.portId);
            if (!srcPort)
                continue;
            nodes::NodeId const srcNodeId = srcPort->getParentId();
            if (cloneMap.find(srcNodeId) == cloneMap.end())
                continue;

            nodes::Port * clonedSrcPort =
              cloneMap[srcNodeId]->findOutputPort(srcPort->getShortName());
            nodes::VariantParameter * clonedTarget = cloneMap[origId]->getParameter(paramName);
            if (clonedSrcPort && clonedTarget)
            {
                m_clipboardModel->addLink(clonedSrcPort->getId(), clonedTarget->getId(), true);
            }
        }
    }
}

void ModelEditor::pasteClipboardAtMouse()
{
    if (!m_currentModel || !m_clipboardModel)
    {
        return;
    }

    // Make sure we only use NodeEditor API when an editor is active
    ImVec2 mouse = ImGui::GetMousePos();
    ImVec2 canvas = ed::ScreenToCanvas(mouse);
    // If user pastes repeatedly, nudge new paste to avoid complete overlap
    if (m_hadLastPastePos && std::abs(canvas.x - m_lastPasteCanvasPos.x) < 1.0f &&
        std::abs(canvas.y - m_lastPasteCanvasPos.y) < 1.0f)
    {
        ++m_consecutivePasteCount;
        canvas.x += m_pasteOffsetStep * (m_consecutivePasteCount % 5);
        canvas.y += m_pasteOffsetStep * (m_consecutivePasteCount % 5);
    }
    else
    {
        m_consecutivePasteCount = 0;
    }

    bool first = true;
    ImVec2 minPos{0, 0}, maxPos{0, 0};
    for (auto const & [id, nodePtr] : *m_clipboardModel)
    {
        (void) id;
        if (!nodePtr)
            continue;
        ImVec2 p{nodePtr->screenPos().x, nodePtr->screenPos().y};
        if (first)
        {
            minPos = maxPos = p;
            first = false;
        }
        else
        {
            minPos.x = std::min(minPos.x, p.x);
            minPos.y = std::min(minPos.y, p.y);
            maxPos.x = std::max(maxPos.x, p.x);
            maxPos.y = std::max(maxPos.y, p.y);
        }
    }

    ImVec2 const center{(minPos.x + maxPos.x) * 0.5f, (minPos.y + maxPos.y) * 0.5f};
    ImVec2 const delta{canvas.x - center.x, canvas.y - center.y};

    createUndoRestorePoint("Paste node(s)");

    std::unordered_map<std::string, nodes::NodeBase *> pastedMap;
    for (auto const & [id, nodePtr] : *m_clipboardModel)
    {
        (void) id;
        if (!nodePtr)
            continue;
        auto cloned = nodePtr->clone();
        cloned->screenPos().x = nodePtr->screenPos().x + delta.x;
        cloned->screenPos().y = nodePtr->screenPos().y + delta.y;
        nodes::NodeBase * inserted = m_currentModel->insert(std::move(cloned));
        pastedMap[nodePtr->getUniqueName()] = inserted;
        ed::SetNodePosition(inserted->getId(),
                            ImVec2(inserted->screenPos().x, inserted->screenPos().y));
    }

    std::unordered_map<std::string, nodes::NodeBase const *> clipboardByName;
    for (auto const & [id, nodePtr] : *m_clipboardModel)
    {
        (void) id;
        if (nodePtr)
            clipboardByName[nodePtr->getUniqueName()] = nodePtr.get();
    }

    for (auto const & [origName, newNode] : pastedMap)
    {
        auto it = clipboardByName.find(origName);
        if (it == clipboardByName.end())
            continue;
        nodes::NodeBase const * origNode = it->second;
        for (auto const & [paramName, param] : origNode->constParameter())
        {
            if (!param.getConstSource().has_value())
                continue;
            auto const & src = param.getConstSource().value();
            nodes::Port const * origSrcPort = m_clipboardModel->getPort(src.portId);
            if (!origSrcPort)
                continue;
            std::string const srcNodeUnique = origSrcPort->getParent()->getUniqueName();
            auto pastedSrcIt = pastedMap.find(srcNodeUnique);
            if (pastedSrcIt == pastedMap.end())
                continue;
            nodes::Port * newSrcPort =
              pastedSrcIt->second->findOutputPort(origSrcPort->getShortName());
            nodes::VariantParameter * newTarget = newNode->getParameter(paramName);
            if (newSrcPort && newTarget)
            {
                m_currentModel->addLink(newSrcPort->getId(), newTarget->getId(), true);
            }
        }
    }

    // Select newly pasted nodes and focus
    ed::ClearSelection();
    for (auto const & [_, node] : pastedMap)
    {
        (void) _;
        ed::SelectNode(node->getId(), true);
    }
    ed::NavigateToSelection(true);

    markModelAsModified();

    // Track last paste canvas position
    m_lastPasteCanvasPos = canvas;
    m_hadLastPastePos = true;
}
} // namespace gladius::ui
