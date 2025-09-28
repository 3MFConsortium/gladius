#pragma once

#include "../ExpressionToGraphConverter.h"
#include "../nodes/History.h"
#include "ExpressionDialog.h"
#include "LibraryBrowser.h"
#include "NodeView.h"
#include "imguinodeeditor.h"

#include <filesystem>
#include <string>

#include "Outline.h"
#include "ResourceView.h"
#include "Style.h"
#include "compute/ComputeCore.h"
#include "nodes/Assembly.h"
#include "nodes/FunctionExtractor.h"
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

        // Library browser methods
        void setLibraryRootDirectory(const std::filesystem::path & directory);
        void toggleLibraryVisibility();
        void setLibraryVisibility(bool visible);
        [[nodiscard]] bool isLibraryVisible() const;
        void refreshLibraryDirectories();

        /// Focus management for keyboard-driven workflow
        void requestNodeFocus(nodes::NodeId nodeId);
        [[nodiscard]] bool shouldFocusNode(nodes::NodeId nodeId) const;
        void clearNodeFocus();

        // Public methods for keyboard shortcuts
        void requestManualCompile();
        void autoLayoutNodes(float distance = 200.0f);
        void showCreateNodePopup();
        void showExpressionDialog();

        /**
         * @brief Handle creation of a function from mathematical expression
         * @param functionName The name for the new function
         * @param expression The mathematical expression
         */
        void onCreateFunctionFromExpression(std::string const & functionName,
                                            std::string const & expression,
                                            std::vector<FunctionArgument> const & arguments,
                                            FunctionOutput const & output);

        /**
         * @brief Switch to a specific function by its ResourceId
         * @param functionId The ResourceId of the function to switch to
         * @return true if the function was found and switched to, false otherwise
         */
        bool switchToFunction(nodes::ResourceId functionId);

        /**
         * @brief Check if mouse is hovering over the model editor
         * @return true if the model editor is being hovered
         */
        bool isHovered() const;

      private:
        // Extraction helper
        void extractSelectedNodesToFunction(const std::string & functionName);

        // Copy/Paste helpers
        void copySelectionToClipboard();
        void pasteClipboardAtMouse();
        bool hasClipboard() const;

        void readBackNodePositions();
        void autoLayout();
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

        // New function creation methods
        nodes::Model & createLevelsetFunction(std::string const & name);
        nodes::Model & copyExistingFunction(nodes::Model const & sourceModel,
                                            std::string const & name);
        void meshResourceToolBox(ImVec2 mousePos);
        void showDeleteUnusedResourcesDialog();
        void validate();

        void undo();
        void redo();

        // Helper method to check if a string matches the current filter
        bool matchesNodeFilter(const std::string & text) const;

        void pushNodeColor(nodes::NodeBase & node);
        void popNodeColor(nodes::NodeBase & node);

        bool m_visible = false;
        ed::EditorContext * m_editorContext{};
        bool m_dirty{true};
        bool m_parameterDirty{false};
        bool m_primitiveDataDirty{false};
        bool m_nodePositionsNeedUpdate{false};
        bool m_pendingPasteRequest{false};
        // Paste UX helpers
        bool m_hadLastPastePos{false};
        ImVec2 m_lastPasteCanvasPos{0.f, 0.f};
        int m_consecutivePasteCount{0};
        float m_pasteOffsetStep{20.f};
        float m_nodeDistance = 50.f;
        float m_scale = 0.5f;
        bool m_nodeWidthsInitialized = false;
        std::string m_newModelName{"New_Part"};
        bool m_showAddModel{false};

        // New function dialog options
        enum class FunctionType
        {
            Empty = 0,
            CopyExisting = 1,
            LevelsetTemplate = 2,
            WrapExisting = 3
        };

        FunctionType m_selectedFunctionType{FunctionType::Empty};
        int m_selectedSourceFunctionIndex{0};

        nodes::SharedAssembly m_assembly;
        nodes::SharedModel m_currentModel;

        static void noOp() {};
        PopupMenuFunction m_popupMenuFunction = noOp;
        NodeView m_nodeViewVisitor;

        bool m_modelWasModified{false};
        bool m_outlineRenaming{true};
        bool m_showCreateNodePopUp{false};
        bool m_showExtractDialog{false};
        std::string m_extractFunctionName{"ExtractedFunction"};

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

        // Confirmation dialog for removing unused resources
        bool m_showDeleteUnusedResourcesConfirmation = false;
        std::vector<Lib3MF::PResource> m_unusedResources;

        // Node filtering
        std::string m_nodeFilterText;

        // Library browser
        LibraryBrowser m_libraryBrowser;

        // Expression dialog
        ExpressionDialog m_expressionDialog;

        /// Focus management for keyboard-driven workflow
        nodes::NodeId m_nodeToFocus{0};
        bool m_shouldFocusNode{false};

        /// Group assignment dialog state
        bool m_showGroupAssignmentDialog{false};

        // Clipboard buffer for copy/paste of nodes
        nodes::UniqueModel m_clipboardModel;
    };

    std::vector<ed::NodeId> selectedNodes(ed::EditorContext * editorContext);
} // namespace gladius::ui
