#pragma once
#include <array>
#include <cstddef>
#include <sstream>
#include <unordered_map>

#include "Assembly.h"
#include "Commands.h"
#include "Model.h"
#include "NodeBase.h"
#include "Parameter.h"
#include "Port.h"
#include "ResourceContext.h"
#include "Visitor.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    using OpenCLSnipplet = std::string;

    using SourceIdToLookupIndex =
      std::unordered_map<ResourceId, std::unordered_map<nodes::NodeId, NodeId>>;
    using ArgumentIndices = std::array<int, 16>;
    using OutputIndices = std::array<int, 16>;

    using ArgumentOutputIndices = std::unordered_map<ParameterName, int>;

    class ToCommandStreamVisitor : public Visitor
    {
      public:
        ToCommandStreamVisitor(CommandBuffer * target, Assembly * assembly);

        ~ToCommandStreamVisitor() override = default;
        void setModel(Model * const model) override;

        void setAssembly(Assembly * assembly) override;

        void write(std::ostream & out) const;
        void visit(Begin & beginning) override;

        void visit(End & ending) override;

        void visit(ConstantScalar & constantScalar) override;

        void visit(ConstantVector & constantVector) override;

        void visit(ConstantMatrix & constantMatrix) override;

        void visit(ComposeVector & composeVector) override;

        void visit(ComposeMatrix & composeMatrix) override;

        void visit(ComposeMatrixFromColumns & composeMatrixFromColumns) override;

        void visit(ComposeMatrixFromRows & composeMatrixFromRows) override;

        void visit(Addition & addition) override;

        void visit(Multiplication & multiplication) override;

        void visit(Subtraction & subtraction) override;

        void visit(Division & division) override;

        void visit(DotProduct & dotProduct) override;

        void visit(CrossProduct & crossProduct) override;

        void visit(MatrixVectorMultiplication & matrixVectorMultiplication) override;

        void visit(Transpose & transpose) override;

        void visit(Sine & sine) override;

        void visit(Cosine & cosine) override;

        void visit(Tangent & tangent) override;

        void visit(ArcSin & arcSin) override;

        void visit(ArcCos & arcCos) override;

        void visit(ArcTan & arcTan) override;

        void visit(ArcTan2 & arcTan2) override;

        void visit(Min & min) override;

        void visit(Max & max) override;

        void visit(Abs & abs) override;

        void visit(Sqrt & sqrt) override;

        void visit(Fmod & fmod) override;

        void visit(Mod & fmod) override;

        void visit(Pow & pow) override;

        void visit(Log & log) override;

        void visit(Log2 & log2) override;

        void visit(Log10 & log10) override;

        void visit(Exp & exp) override;

        void visit(CosH & cosH) override;

        void visit(SinH & sinH) override;

        void visit(TanH & tanH) override;

        void visit(Clamp & clamp) override;

        void visit(Select & select) override;

        void visit(Round & round) override;

        void visit(Ceil & ceil) override;

        void visit(Floor & floor) override;

        void visit(Sign & sign) override;

        void visit(Fract & fract) override;

        void visit(VectorFromScalar & vectorFromScalar) override;

        void visit(SignedDistanceToMesh & signedDistanceToMesh) override;

        void visit(UnsignedDistanceToMesh & unsignedDistanceToMesh) override;

        void visit(DecomposeVector & decomposeVector) override;

        void visit(Inverse & inverse) override;

        void visit(Length & length) override;
        void visit(FunctionCall & functionCall) override;

        // gladius extensions
        void visit(Mix & mix) override;

        void visit(Transformation & transformation) override;

        void visit(Resource & resource) override;

        void visit(ImageSampler & imageSampler) override;

        void visit(NodeBase & baseNode) override;

        void visit(BoxMinMax & boxMinMax) override;

      private:
        auto isOutPutOfNodeValid(NodeBase const & node) -> bool;
        Begin & getAssemblyBegin() const;

        ArgumentIndices getLookUpIndex(IParameter & parameter, ResourceId modelId) const;
        ArgumentIndices getLookUpIndex(IParameter & parameter);

        int acquireOutputIndex(Port & port, size_t numComponents);
        template <typename NodeType>
        void visitImplMathFunction(NodeType & node,
                                   enum CommandType cmdTypeScalar,
                                   enum CommandType cmdTypeVector,
                                   enum CommandType cmdTypeMatrix);

        bool isLookUpIndexValid(int & lookUpIndex) const;

        std::stringstream m_signature;
        std::stringstream m_resultStatement;
        bool m_endReached = false;
        ResourceId m_modelId{};
        Assembly * m_assembly{};
        std::set<nodes::NodeId> m_visitedNodes;
        CommandBuffer * m_cmds{nullptr};

        int m_currentOutPutIndex{1};

        SourceIdToLookupIndex m_portIdToOutputIndex;

        std::optional<NodeBase> m_partNode;
        std::optional<Begin> m_partBegin;
        std::optional<End> m_partEnding;
    };

    template <typename NodeType>
    void ToCommandStreamVisitor::visitImplMathFunction(NodeType & node,
                                                       enum CommandType cmdTypeScalar,
                                                       enum CommandType cmdTypeVector,
                                                       enum CommandType cmdTypeMatrix)
    {
        if (!isOutPutOfNodeValid(node))
        {
            return;
        }

        auto & inputA = node.parameter().at(FieldNames::A);

        auto const dimension = inputA.getSize();
        Command cmd;

        if (dimension == 1)
        {
            cmd.type = cmdTypeScalar;
        }
        else if (dimension == 3)
        {
            cmd.type = cmdTypeVector;
        }
        else if (dimension == 16)
        {
            cmd.type = cmdTypeMatrix;
        }
        else
        {
            throw std::runtime_error("invalid dimension");
        }

        cmd.id = node.getId();

        cmd.output[0] = acquireOutputIndex(node.getResultOutputPort(), dimension);
        for (int i = 1; i < dimension; ++i)
        {
            cmd.output[i] = cmd.output[0] + i;
        }

        cmd.args[0] = getLookUpIndex(node.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(node.parameter().at(FieldNames::A))[1];
        cmd.args[2] = getLookUpIndex(node.parameter().at(FieldNames::A))[2];

        m_cmds->getData().push_back(cmd);
    }
} // namespace gladius::nodes
