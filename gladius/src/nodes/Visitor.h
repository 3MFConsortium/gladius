#pragma once

#include "nodesfwd.h"
#include <functional>

namespace gladius::nodes
{
    class Visitor
    {
      public:
        Visitor();
        virtual ~Visitor();

        virtual void setAssembly(Assembly * assembly);
        virtual void setModel(Model * const model);

        virtual void visit(Begin & beginning);
        virtual void visit(End & ending);

        virtual void visit(ConstantScalar & constantScalar);
        virtual void visit(ConstantVector & constantVector);
        virtual void visit(ConstantMatrix & constantMatrix);

        virtual void visit(ComposeVector & composeVector);
        virtual void visit(ComposeMatrix & composeMatrix);
        virtual void visit(ComposeMatrixFromColumns & composeMatrixFromColumns);
        virtual void visit(ComposeMatrixFromRows & composeMatrixFromRows);

        virtual void visit(Addition & addition);
        virtual void visit(Multiplication & multiplication);
        virtual void visit(Subtraction & subtraction);
        virtual void visit(Division & division);

        virtual void visit(DotProduct & dotProduct);
        virtual void visit(CrossProduct & crossProduct);
        virtual void visit(MatrixVectorMultiplication & matrixVectorMultiplication);
        virtual void visit(Transpose & transpose);
        virtual void visit(Sine & sine);
        virtual void visit(Cosine & cosine);
        virtual void visit(Tangent & tangent);
        virtual void visit(ArcSin & arcSin);
        virtual void visit(ArcCos & arcCos);
        virtual void visit(ArcTan & arcTan);
        virtual void visit(Min & min);
        virtual void visit(Max & max);
        virtual void visit(Abs & abs);
        virtual void visit(Sqrt & sqrt);
        virtual void visit(Fmod & fmod);
        virtual void visit(Mod & mod);
        virtual void visit(Pow & pow);
        virtual void visit(SignedDistanceToMesh & signedDistanceToMesh);
        virtual void visit(SignedDistanceToBeamLattice & signedDistanceToBeamLattice);
        virtual void visit(FunctionCall & functionCall);
        virtual void visit(FunctionGradient & functionGradient);
        virtual void visit(Length & length);
        virtual void visit(DecomposeVector & decomposeVector);
        virtual void visit(Resource & resource);
        virtual void visit(ImageSampler & imageSampler);

        // gladius extensions
        virtual void visit(Mix & mix);
        virtual void visit(Transformation & transformation);
        virtual void visit(BoxMinMax & boxMinMax);

        virtual void visit(DecomposeMatrix & decomposeMatrix);
        virtual void visit(Inverse & inverse);
        virtual void visit(ArcTan2 & arcTan2);
        virtual void visit(Exp & exp);
        virtual void visit(Log & log);
        virtual void visit(Log2 & log2);
        virtual void visit(Log10 & log10);
        virtual void visit(Select & select);
        virtual void visit(Clamp & clamp);
        virtual void visit(SinH & sinh);
        virtual void visit(CosH & cosh);
        virtual void visit(TanH & tanh);
        virtual void visit(Round & round);
        virtual void visit(Ceil & ceil);
        virtual void visit(Floor & floor);
        virtual void visit(Sign & sign);
        virtual void visit(Fract & fract);
        virtual void visit(VectorFromScalar & vectorFromScalar);
        virtual void visit(UnsignedDistanceToMesh & unsignedDistanceToMesh);

        virtual void visit(NodeBase & baseNode);

      protected:
        Model * m_currentModel{nullptr};
    };

    template <typename T>
    class OnTypeVisitor : public nodes::Visitor
    {
        using NodeAction = std::function<void(T & node)>;

      public:
        explicit OnTypeVisitor(NodeAction action)
            : m_action(std::move(action))
        {
        }

      private:
        void visit(NodeBase &) override
        {
        }

        void visit(T & node) override
        {
            m_action(node);
        }
        NodeAction m_action;
    };

} // namespace gladius::nodes
