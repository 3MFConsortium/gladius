#pragma once

#include "../nodes/Model.h"
#include "Style.h"

#include <imgui.h>
#include <map>
#include <optional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <string>
#include <vector>

namespace gladius::ui
{
    class ModelEditor;

    std::string typeToString(std::type_index typeIndex);
    using ColumnWidths = std::array<float, 8>;

    /**
     * @brief Structure that holds information about a group of nodes
     */
    struct NodeGroup 
    {
        std::string tag;                   ///< The tag that all nodes in this group share
        std::vector<nodes::NodeId> nodes;  ///< The nodes that belong to this group
        ImVec2 minBound;                   ///< The minimum bound of the group (top-left)
        ImVec2 maxBound;                   ///< The maximum bound of the group (bottom-right)
        ImVec4 color;                      ///< The color for this group
    };

    class NodeView : public nodes::Visitor
    {
      public:
        NodeView();
        ~NodeView() override = default;
        void visit(nodes::NodeBase & baseNode) override;
        void visit(nodes::Begin & beginNode) override;
        void visit(nodes::End & endNode) override;
        void visit(nodes::ConstantScalar & constantScalarNode) override;
        void visit(nodes::ConstantVector & constantVectorNode) override;
        void visit(nodes::ConstantMatrix & constantMatrixNode) override;
        void visit(nodes::Transformation & transformationNode) override;

        void visit(nodes::Resource & resourceNode) override;

        void setModelEditor(ModelEditor * editor);

        [[nodiscard]] auto haveParameterChanged() const -> bool;
        [[nodiscard]] auto hasModelChanged() const -> bool;

        void setAssembly(nodes::SharedAssembly assembly);
        void reset();

        void setCurrentModel(nodes::SharedModel model);

        void setResourceNodesVisible(bool visible);
        [[nodiscard]] bool areResourceNodesVisible() const;

        void setUiScale(float scale);
        [[nodiscard]] float getUiScale() const;

        bool columnWidthsAreInitialized() const;
        
        /**
         * @brief Updates the node group mapping based on current model
         */
        void updateNodeGroups();
        
        /**
         * @brief Renders all node groups
         */
        void renderNodeGroups();

        /**
         * @brief Find a node by ID in the current model
         * @param nodeId The ID of the node to find
         * @return Pointer to the found node or nullptr if not found
         */
        nodes::NodeBase* findNodeById(nodes::NodeId nodeId) const;
        
        /**
         * @brief Replace the tag of all nodes in a group
         * @param oldTag The current tag to replace
         * @param newTag The new tag to assign to all nodes in the group
         * @return true if the tag was successfully replaced, false otherwise
         */
        bool replaceGroupTag(const std::string& oldTag, const std::string& newTag);
        
        /**
         * @brief Get access to the node groups map
         * @return const reference to the node groups map
         */
        const std::unordered_map<std::string, NodeGroup>& getNodeGroups() const;
        
        /**
         * @brief Checks if the provided tag is already assigned to a group
         * @param tag The tag to check
         * @return Whether the tag exists in the current groups
         */
        [[nodiscard]] bool hasGroup(const std::string& tag) const;
        
        /**
         * @brief Gets all nodes that belong to the same group as the specified node
         * @param nodeId The ID of the node to find group members for
         * @return Vector of node IDs in the same group, or empty vector if not in a group
         */
        std::vector<nodes::NodeId> getNodesInSameGroup(nodes::NodeId nodeId) const;
        
        /**
         * @brief Handles group movement when a group node is moved
         * Called during the editor update loop to detect group node movement and synchronize all nodes in the group
         */
        void handleGroupMovement();
        
        /**
         * @brief Handles group dragging via header/border areas
         * Should be called before rendering groups to handle mouse input
         */
        void handleGroupDragging();
        
        /**
         * @brief Checks if selection rectangle should be suppressed due to group dragging
         * @return true if selection rectangle should be suppressed
         */
        bool shouldSuppressSelectionRect() const;
        
        /**
         * @brief Alternative: Handle group dragging with modifier key (Ctrl+drag anywhere in group)
         * @param requireModifier If true, requires Ctrl key to be held for group dragging
         */
        void handleGroupDraggingWithModifier(bool requireModifier = true);
        
        /**
         * @brief Alternative: Handle group dragging with right mouse button
         * Right-click and drag anywhere in group to move entire group
         */
        void handleGroupDraggingRightClick();
        
        /**
         * @brief Handles double-click on group rectangles to select all nodes in the group
         * @param groupTag The tag of the group that was double-clicked
         */
        void handleGroupClick(const std::string& groupTag);
        
        /**
         * @brief Checks for double-clicks on group rectangles and returns the group tag if found
         * @return The tag of the group that was double-clicked, or empty string if none
         */
        std::string checkForGroupClick() const;
        
        /**
         * @brief Checks if the mouse is over a group header or border area (for dragging)
         * @param mousePos The mouse position in screen coordinates
         * @return The tag of the group under the mouse header/border, or empty string if none
         */
        std::string getGroupUnderMouseHeader(const ImVec2& mousePos) const;
        
        /**
         * @brief Checks if the mouse is over the interior (content area) of any group
         * @param mousePos The mouse position in screen coordinates  
         * @return true if mouse is over group interior (should allow selection rectangle)
         */
        bool isMouseOverGroupInterior(const ImVec2& mousePos) const;

      private:
        void show(nodes::NodeBase & node);
        void header(nodes::NodeBase & node);
        void content(nodes::NodeBase & node);
        void footer(nodes::NodeBase & node);
        void inputControls(nodes::NodeBase & node, nodes::ParameterMap::reference parameter);
        void showLinkAssignmentMenu(nodes::ParameterMap::reference parameter);
        void showInputAndOutputs(nodes::NodeBase & node);
        void inputPins(nodes::NodeBase & node);
        void outputPins(nodes::NodeBase & node);
        void viewInputNode(nodes::NodeBase & node);

        ImVec4 typeToColor(std::type_index tyepIndex);

        bool viewString(nodes::NodeBase const & node,
                        nodes::ParameterMap::reference parameter,
                        nodes::VariantType & val);

        void viewFloat(nodes::NodeBase const & node,
                       nodes::ParameterMap::reference parameter,
                       nodes::VariantType & val);

        void viewFloat3(nodes::NodeBase const & node,
                        nodes::ParameterMap::reference parameter,
                        nodes::VariantType & val);

        void viewMatrix(nodes::NodeBase const & node,
                        nodes::ParameterMap::reference parameter,
                        nodes::VariantType & val);

        void viewResource(nodes::NodeBase & node,
                          nodes::ParameterMap::reference parameter,
                          nodes::VariantType & val);

        void viewInt(nodes::NodeBase const & node,
                     nodes::ParameterMap::reference parameter,
                     nodes::VariantType & val);

        bool typeControl(std::string const & label, std::type_index & typeIndex);
        
        /**
         * @brief Calculates the bounds of a node group
         * @param group The group to calculate bounds for
         */
        void calculateGroupBounds(NodeGroup& group);
        
        /**
         * @brief Calculate group rectangle with padding and header
         * @param group The group to calculate bounds for
         * @param minOut Output minimum bounds (top-left)
         * @param maxOut Output maximum bounds (bottom-right)
         * @return true if the group has valid nodes and bounds were calculated
         */
        bool calculateGroupRect(const NodeGroup& group, ImVec2& minOut, ImVec2& maxOut) const;
        
        /**
         * @brief Generate a consistent color for a group based on its tag
         * @param tag The group tag to generate color for
         * @return RGBA color for the group
         */
        ImVec4 generateGroupColor(const std::string& tag) const;

        int m_currentLinkId = 0;
        bool m_parameterChanged{false};
        bool m_modelChanged{false};

        nodes::SharedAssembly m_assembly;
        nodes::SharedModel m_currentModel;

        ModelEditor * m_modelEditor{nullptr};
        bool m_showContextMenu{false};

        bool m_showLinkAssignmentMenu{false};
        bool m_popStyle{false};

        bool m_resoureIdNodesVisible{false};

        struct NewChannelProperties
        {
            nodes::ParameterName name = {"new"};
            std::type_index typeIndex = typeid(float);
        };

        std::unordered_map<nodes::NodeId, NewChannelProperties>
          m_newChannelProperties; // used for persistence of the new parameter field in the begin
                                  // node

        std::unordered_map<nodes::NodeId, NewChannelProperties> m_newOutputChannelProperties;

        NodeTypeToColor m_nodeTypeToColor;

        float m_uiScale = 1.0f;

        std::unordered_map<nodes::NodeId, ColumnWidths> m_columnWidths;
        
        /// Storage for node groups organized by tag
        std::unordered_map<std::string, NodeGroup> m_nodeGroups;
        
        /// Tag editing state
        std::string m_editingTag;
        std::string m_editingTagBuffer;
        bool m_isEditingTag = false;
        
        /// Group node position tracking for group movement (stores positions of group nodes, not individual model nodes)
        std::unordered_map<nodes::NodeId, ImVec2> m_previousNodePositions;
        bool m_skipGroupMovement = false; // Flag to prevent infinite loops during programmatic movement
        
        /// Double-click group selection state
        std::string m_pendingGroupSelection;
        bool m_hasPendingGroupSelection = false;
        int m_groupSelectionFrame = 0; // Frame counter for deferred selection
        
        /// Group dragging state
        std::string m_draggingGroup;
        bool m_isDraggingGroup = false;
        ImVec2 m_groupDragStartPos;

        ColumnWidths & getOrCreateColumnWidths(nodes::NodeId nodeId);
    };
} // namespace gladius::ui
