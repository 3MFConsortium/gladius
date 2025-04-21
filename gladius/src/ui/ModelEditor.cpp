#include <exception>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <imguinodeeditor.h>
#include <iostream>

#include <algorithm>

#include "../CLMath.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include <nodes/Assembly.h>
#include <nodes/GraphFlattener.h>
#include <nodes/Model.h>
#include "Document.h"
#include "MeshResource.h"
#include "ModelEditor.h"
#include "NodeView.h"
#include "Port.h"
#include "ResourceView.h"
#include "Style.h"
#include "Widgets.h"
#include "imgui.h"
#include "nodesfwd.h"
#include "ui/LevelSetView.h"

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
            if (ImGui::TreeNodeEx("VolumeData", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TreePop();
            }            ImGui::EndGroup();
            frameOverlay(ImVec4(1.0f, 0.0f, 1.0f, 0.1f));

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
            frameOverlay(ImVec4(1.0f, 1.0f, 0.0f, 0.1f));

            resourceOutline();

            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx("Functions", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
            {
                functionOutline();
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(ImVec4(0.0f, 0.5f, 1.0f, 0.1f));

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

                if (!isAssembly || model.second->isManaged())
                {
                    // Check if function can be safely deleted
                    auto safeResult = m_doc->isItSafeToDeleteResource(ResourceKey(model.second->getResourceId()));
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
                            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
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
                ImGui::Text("Please enter the name of the new function");
                ImGui::Separator();

                ImGui::InputText("Function name", &m_newModelName);
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    auto & newModel = m_doc->createNewFunction();
                    newModel.setDisplayName(m_newModelName);
                    switchModel();

                    m_showAddModel = false;
                    ImGui::CloseCurrentPopup();
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
                      if (node.getCategory() == category &&
                          (hasRequiredField || !showOnlyLinkableNodes))
                      {
                          if (ImGui::Button(node.name().c_str()))
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
                              markModelAsModified();
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
                        autoLayout(m_nodeDistance);
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

                    if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_HAMMER "\tCompile")))
                    {
                        m_isManualCompileRequested = true;
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
                }
                onCreateNode();
                onDeleteNode();

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

        if (!m_currentModel->hasBeenLayouted() && m_nodeWidthsInitialized)
        {
            autoLayout(m_nodeDistance);
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

    void ModelEditor::functionToolBox(ImVec2 mousePos)
    {
        auto functions = m_assembly->getFunctions();
        for (auto & [id, model] : functions)
        {
            if (!model || model->isManaged() || model == m_currentModel)
            {
                continue;
            }
            if (ImGui::Button(model->getDisplayName().value_or("function").c_str()))
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

            if (ImGui::Button(key.getDisplayName().c_str()))
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
        if (!m_autoCompile)
        {
            return;
        }

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

    void ModelEditor::autoLayout(float distance)
    {
        if (currentModel() == nullptr)
        {
            return;
        }

        if (!m_nodeWidthsInitialized)
        {
            return;
        }

        auto const graph = currentModel()->getGraph();

        if (graph.getSize() < 2)
        {
            return;
        }
        createUndoRestorePoint("Autolayout");

        currentModel()->updateGraphAndOrderIfNeeded();

        if (!(currentModel()->getBeginNode()))
        {
            return;
        }
        auto const beginId = currentModel()->getBeginNode()->getId();
        auto depthMap = determineDepth(graph, beginId);

        auto getDepth = [&](nodes::NodeId nodeId)
        {
            auto const depthIter = depthMap.find(nodeId);
            if (depthIter != std::end(depthMap))
            {
                return depthIter->second;
            }
            return 0;
        };

        auto getDepthCloseToSuccessor = [&](nodes::NodeId nodeId)
        {
            auto successsor = graph::determineSuccessor(graph, nodeId);
            // find  lowest depth of the successor
            int lowestDepth = std::numeric_limits<int>::max();
            for (auto const succ : successsor)
            {
                auto const depthIter = depthMap.find(succ);
                if (depthIter != std::end(depthMap))
                {
                    lowestDepth = std::min(lowestDepth, depthIter->second);
                }
            }

            if (lowestDepth != std::numeric_limits<int>::max())
            {
                return lowestDepth - 1;
            }
            return 0;
        };

        auto determineDepth = [&](nodes::NodeId nodeId)
        {
            auto const depth = getDepth(nodeId);
            if (depth == 0)
            {
                return getDepthCloseToSuccessor(nodeId);
            }
            return depth;
        };

        // Step 1: Assign Layers
        std::map<int, std::vector<nodes::NodeBase *>> layers;
        std::map<int, float> layersWidth;
        for (auto & [id, node] : *currentModel())
        {
            auto const depth = (id == beginId) ? 0 : determineDepth(id);
            layers[depth].push_back(node.get());
            auto const nodeWidth = ed::GetNodeSize(node->getId()).x;
            layersWidth[depth] = std::max(layersWidth[depth], nodeWidth);
        }

        // Step 2: Order Nodes within Layers (simple topological order for now)
        for (auto & [depth, nodes] : layers)
        {
            std::sort(nodes.begin(),
                      nodes.end(),
                      [](nodes::NodeBase * a, nodes::NodeBase * b)
                      { return a->getOrder() < b->getOrder(); });
        }

        std::vector<float> layerX;
        float x = 0.f;
        for (auto & [depth, width] : layersWidth)
        {
            layerX.push_back(x);
            x += width + distance;
        }

        // Step 3: Assign Coordinates
        for (auto & [depth, nodes] : layers)
        {
            // substract distance from every second layer to create a zigzag pattern
            float y = (depth % 2 == 0) ? 0.f : -distance;
            for (auto & node : nodes)
            {
                auto & pos = node->screenPos();
                auto nodeWidth = ed::GetNodeSize(node->getId()).x;
                auto nodeHeight = ed::GetNodeSize(node->getId()).y;
                if (nodeWidth == 0.f || nodeHeight == 0.f)
                {
                    bool isResourceNode = dynamic_cast<nodes::Resource *>(node) != nullptr;
                    if (isResourceNode)
                    {
                        continue;
                    }
                    else
                    {
                        return;
                    }
                }
                pos.x = layerX.at(depth);
                pos.y = y;
                y += nodeHeight + distance; // Adjust y position to avoid overlap
            }
        }

        currentModel()->markAsLayouted();

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

} // namespace gladius::ui