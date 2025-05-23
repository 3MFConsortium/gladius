#pragma once

#include "../nodes/Model.h"
#include "Style.h"

#include "imgui.h"
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
        nodes::NodeBase* findNodeById(nodes::NodeId nodeId);

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
         * @brief Checks if the provided tag is already assigned to a group
         * @param tag The tag to check
         * @return Whether the tag exists in the current groups
         */
        [[nodiscard]] bool hasGroup(const std::string& tag) const;
        
        /**
         * @brief Calculates the bounds of a node group
         * @param group The group to calculate bounds for
         */
        void calculateGroupBounds(NodeGroup& group);
        
        /**
         * @brief Generates a color for a group based on its tag
         * @param tag The group's tag
         * @return A color for the group
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

        ColumnWidths & getOrCreateColumnWidths(nodes::NodeId nodeId);

        // New member variable to track node groups
        std::vector<NodeGroup> m_nodeGroups;
        bool m_nodeGroupsNeedUpdate{true};
        
        // Custom colors for groups, persisted by tag
        std::unordered_map<std::string, ImVec4> m_customGroupColors;
    };
} // namespace gladius::ui
