#pragma once
#include <sstream>

#include "Model.h"
#include "NodeBase.h"
#include "OutputPortReferenceAnalyzer.h"
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

        void visit(SignedDistanceToBeamLattice & signedDistanceToBeamLattice) override;

        void visit(FunctionCall & functionCall) override;

        void visit(FunctionGradient & functionGradient) override;

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

        /// @brief Check if an output should be inlined (used only once)
        bool shouldInlineOutput(NodeBase const & node, std::string const & portName) const;

        /// @brief Get the OpenCL expression for a parameter, checking inline map first
        /// @param param The parameter to resolve
        /// @return The OpenCL expression string (either inlined or from toString())
        std::string resolveParameter(IParameter const & param) const;

        /// @brief Helper for unary operations (sin, cos, sqrt, etc.)
        void emitUnaryOperation(NodeBase & node,
                                std::string const & operation,
                                std::string const & outputPortName);

        /// @brief Helper for binary operations (min, max, pow, etc.)
        void emitBinaryOperation(NodeBase & node,
                                 std::string const & operation,
                                 std::string const & outputPortName,
                                 std::string const & param1Name = FieldNames::A,
                                 std::string const & param2Name = FieldNames::B);

        /// @brief Helper for ternary operations (mix, clamp, etc.)
        void emitTernaryOperation(NodeBase & node,
                                  std::string const & operation,
                                  std::string const & outputPortName,
                                  std::string const & param1Name,
                                  std::string const & param2Name,
                                  std::string const & param3Name);

      private:
        std::stringstream m_definition;
        std::stringstream m_declaration;
        bool m_endReached = false;
        Assembly * m_assembly{};
        std::set<nodes::NodeId> m_visitedNodes;
        Model * m_currentModel = nullptr;

        // Reference analyzer for optimization decisions
        mutable OutputPortReferenceAnalyzer m_referenceAnalyzer;
        mutable bool m_referenceAnalysisPerformed = false;

        // Map from (NodeId, PortName) to inline expression
        // Used for single-use outputs that should be inlined
        std::map<std::pair<NodeId, std::string>, std::string> m_inlineExpressions;
    };
} // namespace gladius::nodes
