#pragma once

#include "../nodes/Model.h"
#include "Style.h"

#include "imgui.h"
#include <map>

namespace gladius::ui
{
    class ModelEditor;

    std::string typeToString(std::type_index typeIndex);

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

        bool typeControl(std::string const & label, std::type_index & typeIndex);

        int m_currentLinkId = 0;
        bool m_parameterChanged{false};
        bool m_modelChanged{false};

        nodes::SharedAssembly m_assembly;
        nodes::SharedModel m_currentModel;

        ModelEditor * m_modelEditor{nullptr};
        bool m_showContextMenu{false};

        bool m_showLinkAssignmentMenu{false};
        bool m_popStyle{false};

        bool m_resoureIdNodesVisible{true};

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
    };
} // namespace gladius::ui
