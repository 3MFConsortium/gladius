#pragma once
#include <sstream>

#include "Model.h"
#include "NodeBase.h"
#include "Visitor.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    using OpenCLSnipplet = std::string;

    class ToOclVisitor : public Visitor
    {
      public:
        ToOclVisitor() = default;
        ~ToOclVisitor() override = default;

        void setAssembly(Assembly * assembly) override;
        virtual void setModel(Model * const model) override;

        void write(std::ostream & out) const;

        void visit(NodeBase &) override;

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

        void visit(Min & min) override;

        void visit(Max & max) override;

        void visit(Abs & abs) override;

        void visit(Sqrt & sqrt) override;

        void visit(Fmod & fmod) override;

        void visit(Mod & mod) override;

        void visit(Pow & pow) override;

        void visit(SignedDistanceToMesh & signedDistanceToMesh) override;

        void visit(FunctionCall & functionCall) override;

        // gladius extensions
        void visit(Mix & mix) override;

        void visit(Transformation & transformation) override;

        void visit(Length & length) override;

        void visit(DecomposeVector & decomposeVector) override;

        void visit(Resource & resource) override;

        void visit(ImageSampler & imageSampler) override;

        void visit(DecomposeMatrix & decomposeMatrix) override;

        void visit(Inverse & inverse) override;

        void visit(ArcTan2 & arcTan2) override;

        void visit(Exp & exp) override;

        void visit(Log & log) override;

        void visit(Log2 & log2) override;

        void visit(Log10 & log10) override;

        void visit(Select & select) override;

        void visit(Clamp & clamp) override;

        void visit(SinH & sinh) override;

        void visit(CosH & cosh) override;

        void visit(TanH & tanh) override;

        void visit(Round & round) override;

        void visit(Ceil & ceil) override;

        void visit(Floor & floor) override;

        void visit(Sign & sign) override;

        void visit(Fract & fract) override;

        void visit(VectorFromScalar & vectorFromScalar) override;

        void visit(UnsignedDistanceToMesh & unsignedDistanceToMesh) override;

        void visit(BoxMinMax & boxMinMax) override;

      private:
        auto isOutPutOfNodeValid(NodeBase const & node) -> bool;
        void assemblyBegin(Begin & beginning);

      private:
        std::stringstream m_definition;
        std::stringstream m_declaration;
        bool m_endReached = false;
        Assembly * m_assembly{};
        std::set<nodes::NodeId> m_visitedNodes;
    };
} // namespace gladius::nodes
