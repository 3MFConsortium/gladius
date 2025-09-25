#include "Visitor.h"
#include <stdexcept>

#include "Model.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    Visitor::Visitor() = default;

    Visitor::~Visitor() = default;

    void Visitor::setAssembly(Assembly * assembly)
    {
        (void) assembly;
    }

    void Visitor::setModel(Model * const model)
    {
        m_currentModel = model;
    }

    void Visitor::visit(Begin & beginning)
    {
        visit(static_cast<NodeBase &>(beginning));
    }

    void Visitor::visit(End & ending)
    {
        visit(static_cast<NodeBase &>(ending));
    }

    void Visitor::visit(ConstantScalar & constantScalar)
    {
        visit(static_cast<NodeBase &>(constantScalar));
    }

    void Visitor::visit(ConstantVector & constantVector)
    {
        visit(static_cast<NodeBase &>(constantVector));
    }

    void Visitor::visit(ConstantMatrix & constantMatrix)
    {
        visit(static_cast<NodeBase &>(constantMatrix));
    }

    void Visitor::visit(ComposeVector & composeVector)
    {
        visit(static_cast<NodeBase &>(composeVector));
    }

    void Visitor::visit(ComposeMatrix & composeMatrix)
    {
        visit(static_cast<NodeBase &>(composeMatrix));
    }

    void Visitor::visit(ComposeMatrixFromColumns & composeMatrixFromColumns)
    {
        visit(static_cast<NodeBase &>(composeMatrixFromColumns));
    }

    void Visitor::visit(ComposeMatrixFromRows & composeMatrixFromRows)
    {
        visit(static_cast<NodeBase &>(composeMatrixFromRows));
    }

    void Visitor::visit(Addition & addition)
    {
        visit(static_cast<NodeBase &>(addition));
    }

    void Visitor::visit(Multiplication & multiplication)
    {
        visit(static_cast<NodeBase &>(multiplication));
    }

    void Visitor::visit(Subtraction & subtraction)
    {
        visit(static_cast<NodeBase &>(subtraction));
    }

    void Visitor::visit(Division & division)
    {
        visit(static_cast<NodeBase &>(division));
    }

    void Visitor::visit(DotProduct & dotProduct)
    {
        visit(static_cast<NodeBase &>(dotProduct));
    }

    void Visitor::visit(CrossProduct & crossProduct)
    {
        visit(static_cast<NodeBase &>(crossProduct));
    }

    void Visitor::visit(MatrixVectorMultiplication & matrixVectorMultiplication)
    {
        visit(static_cast<NodeBase &>(matrixVectorMultiplication));
    }

    void Visitor::visit(Transpose & transpose)
    {
        visit(static_cast<NodeBase &>(transpose));
    }

    void Visitor::visit(Sine & sine)
    {
        visit(static_cast<NodeBase &>(sine));
    }

    void Visitor::visit(Cosine & cosine)
    {
        visit(static_cast<NodeBase &>(cosine));
    }

    void Visitor::visit(Tangent & tangent)
    {
        visit(static_cast<NodeBase &>(tangent));
    }

    void Visitor::visit(ArcSin & arcSin)
    {
        visit(static_cast<NodeBase &>(arcSin));
    }

    void Visitor::visit(ArcCos & arcCos)
    {
        visit(static_cast<NodeBase &>(arcCos));
    }

    void Visitor::visit(ArcTan & arcTan)
    {
        visit(static_cast<NodeBase &>(arcTan));
    }

    void Visitor::visit(Min & min)
    {
        visit(static_cast<NodeBase &>(min));
    }

    void Visitor::visit(Max & max)
    {
        visit(static_cast<NodeBase &>(max));
    }

    void Visitor::visit(Abs & abs)
    {
        visit(static_cast<NodeBase &>(abs));
    }

    void Visitor::visit(Sqrt & sqrt)
    {
        visit(static_cast<NodeBase &>(sqrt));
    }

    void Visitor::visit(Fmod & fmod)
    {
        visit(static_cast<NodeBase &>(fmod));
    }

    void Visitor::visit(Mod & mod)
    {
        visit(static_cast<NodeBase &>(mod));
    }

    void Visitor::visit(Pow & pow)
    {
        visit(static_cast<NodeBase &>(pow));
    }

    void Visitor::visit(SignedDistanceToMesh & signedDistanceToMesh)
    {
        visit(static_cast<NodeBase &>(signedDistanceToMesh));
    }

    void Visitor::visit(SignedDistanceToBeamLattice & signedDistanceToBeamLattice)
    {
        visit(static_cast<NodeBase &>(signedDistanceToBeamLattice));
    }

    void Visitor::visit(FunctionCall & functionCall)
    {
        visit(static_cast<NodeBase &>(functionCall));
    }

    void Visitor::visit(Mix & mix)
    {
        visit(static_cast<NodeBase &>(mix));
    }

    void Visitor::visit(Transformation & transformation)
    {
        visit(static_cast<NodeBase &>(transformation));
    }

    void Visitor::visit(BoxMinMax & boxMinMax)
    {
        visit(static_cast<NodeBase &>(boxMinMax));
    }

    void Visitor::visit(Length & length)
    {
        visit(static_cast<NodeBase &>(length));
    }

    void Visitor::visit(DecomposeVector & decomposeVector)
    {
        visit(static_cast<NodeBase &>(decomposeVector));
    }

    void Visitor::visit(Resource & resource)
    {
        visit(static_cast<NodeBase &>(resource));
    }

    void Visitor::visit(ImageSampler & imageSampler)
    {
        visit(static_cast<NodeBase &>(imageSampler));
    }

    void Visitor::visit(DecomposeMatrix & decomposeMatrix)
    {
        visit(static_cast<NodeBase &>(decomposeMatrix));
    }

    void Visitor::visit(Inverse & inverse)
    {
        visit(static_cast<NodeBase &>(inverse));
    }

    void Visitor::visit(ArcTan2 & arcTan2)
    {
        visit(static_cast<NodeBase &>(arcTan2));
    }

    void Visitor::visit(Exp & exp)
    {
        visit(static_cast<NodeBase &>(exp));
    }

    void Visitor::visit(Log & log)
    {
        visit(static_cast<NodeBase &>(log));
    }

    void Visitor::visit(Log2 & log2)
    {
        visit(static_cast<NodeBase &>(log2));
    }

    void Visitor::visit(Log10 & log10)
    {
        visit(static_cast<NodeBase &>(log10));
    }

    void Visitor::visit(Select & select)
    {
        visit(static_cast<NodeBase &>(select));
    }

    void Visitor::visit(Clamp & clamp)
    {
        visit(static_cast<NodeBase &>(clamp));
    }

    void Visitor::visit(SinH & sinh)
    {
        visit(static_cast<NodeBase &>(sinh));
    }

    void Visitor::visit(CosH & cosh)
    {
        visit(static_cast<NodeBase &>(cosh));
    }

    void Visitor::visit(TanH & tanh)
    {
        visit(static_cast<NodeBase &>(tanh));
    }

    void Visitor::visit(Round & round)
    {
        visit(static_cast<NodeBase &>(round));
    }

    void Visitor::visit(Ceil & ceil)
    {
        visit(static_cast<NodeBase &>(ceil));
    }

    void Visitor::visit(Floor & floor)
    {
        visit(static_cast<NodeBase &>(floor));
    }

    void Visitor::visit(Sign & sign)
    {
        visit(static_cast<NodeBase &>(sign));
    }

    void Visitor::visit(Fract & fract)
    {
        visit(static_cast<NodeBase &>(fract));
    }

    void Visitor::visit(VectorFromScalar & vectorFromScalar)
    {
        visit(static_cast<NodeBase &>(vectorFromScalar));
    }

    void Visitor::visit(UnsignedDistanceToMesh & unsignedDistanceToMesh)
    {
        visit(static_cast<NodeBase &>(unsignedDistanceToMesh));
    }

    void Visitor::visit(NodeBase & baseNode)
    {
        (void) baseNode;
        throw std::runtime_error(
          fmt::format("Node for {} not implemented", baseNode.getDisplayName()));
    }

} // namespace gladius::nodes
