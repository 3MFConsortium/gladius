#pragma once

#include "../nodes/History.h"
#include "NodeView.h"
#include "imguinodeeditor.h"

#include <filesystem>
#include <string>

#include "Outline.h"
#include "Style.h"
#include "ResourceView.h"
#include "compute/ComputeCore.h"
#include "nodes/Assembly.h"
#include "nodes/Model.h"
#include "nodes/nodesfwd.h"

namespace gladius
{
    class Document;
}

namespace ed = ax::NodeEditor;

namespace gladius::ui
{

    using PopupMenuFunction = std::function<void()>;

    class ModelEditor
    {
      public:
        ModelEditor();
        ~ModelEditor();
        void resetEditorContext();
        bool showAndEdit();

        void triggerNodePositionUpdate();

        void showPopupMenu(PopupMenuFunction popupMenuFunction);
        void closePopupMenu();

        [[nodiscard]] nodes::SharedModel currentModel() const;

        void setDocument(std::shared_ptr<Document> document);

        [[nodiscard]] bool modelWasModified() const;

        [[nodiscard]] bool isCompileRequested() const;

        void markModelAsModified();
        void markModelAsUpToDate();
        void setVisibility(bool visible);
        [[nodiscard]] bool isVisible() const;
        void createUndoRestorePoint(const std::string & description);
        void resetUndo();

        [[nodiscard]] bool primitveDataNeedsUpdate() const;
        void invalidatePrimitiveData();
        void markPrimitiveDataAsUpToDate();

      private:
        void readBackNodePositions();
        void autoLayout(float distance);
        void applyNodePositions();
        void placeTransformation(nodes::NodeBase & createdNode,
                                 std::vector<ed::NodeId> & selection) const;
        void placeBoolOp(nodes::NodeBase & createdNode, std::vector<ed::NodeId> & selection) const;
        void defaultPlacement(nodes::NodeBase & createdNode,
                              std::vector<ed::NodeId> & selection) const;
        void placeNode(nodes::NodeBase & node);
        void onCreateNode();
        void onDeleteNode();
        void toolBox();
        bool isNodeSelected(nodes::NodeId nodeId);
        void switchModel();
        void outline();
        void resourceOutline();
        void functionOutline();
        void newModelDialog();
        void onQueryNewNode();
        void createNodePopup(nodes::PortId srcPortId, ImVec2 mousePos);

        void invalidateEverything();
        void setAssembly(nodes::SharedAssembly assembly);

        void functionToolBox(ImVec2 mousePos);
        void meshResourceToolBox(ImVec2 mousePos);

        void validate();

        void undo();
        void redo();

        void pushNodeColor(nodes::NodeBase & node);
        void popNodeColor(nodes::NodeBase & node);

        bool m_visible = false;
        ed::EditorContext * m_editorContext{};
        bool m_dirty{true};
        bool m_parameterDirty{false};
        bool m_primitiveDataDirty{false};
        bool m_nodePositionsNeedUpdate{false};
        float m_nodeDistance = 50.f;
        float m_scale = 0.5f;
        bool m_nodeWidthsInitialized = false;

        std::string m_newModelName{"New_Part"};
        bool m_showAddModel{false};

        nodes::SharedAssembly m_assembly;
        nodes::SharedModel m_currentModel;

        static void noOp() {};
        PopupMenuFunction m_popupMenuFunction = noOp;
        NodeView m_nodeViewVisitor;

        bool m_modelWasModified{false};
        bool m_outlineRenaming{true};
        bool m_showCreateNodePopUp{false};

        nodes::History m_history;

        bool m_stateApplyingUndo = false;

        bool m_autoCompile = true;
        bool m_isManualCompileRequested = false;
        bool m_outlineNodeColorLines = true;

        std::shared_ptr<Document> m_doc;

        ResourceView m_resourceView;

        Outline m_outline;

        NodeTypeToColor m_nodeTypeToColor;
        float m_uiScale = 1.0f;
    };

    std::vector<ed::NodeId> selectedNodes(ed::EditorContext * editorContext);
} // namespace gladius::ui
