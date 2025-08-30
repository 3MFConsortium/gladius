#include "NodeFactory.h"
#include "DerivedNodes.h"

namespace gladius::nodes
{
    std::unique_ptr<NodeBase> NodeFactory::createNode(const std::string & nodeType)
    {
        if (nodeType == "Addition")
            return std::make_unique<Addition>();
        if (nodeType == "Subtraction")
            return std::make_unique<Subtraction>();
        if (nodeType == "Multiplication")
            return std::make_unique<Multiplication>();
        if (nodeType == "Division")
            return std::make_unique<Division>();
        if (nodeType == "Sine")
            return std::make_unique<Sine>();
        if (nodeType == "Cosine")
            return std::make_unique<Cosine>();
        if (nodeType == "Tangent")
            return std::make_unique<Tangent>();
        if (nodeType == "ArcSin")
            return std::make_unique<ArcSin>();
        if (nodeType == "ArcCos")
            return std::make_unique<ArcCos>();
        if (nodeType == "ArcTan")
            return std::make_unique<ArcTan>();
        if (nodeType == "ArcTan2")
            return std::make_unique<ArcTan2>();
        if (nodeType == "Pow")
            return std::make_unique<Pow>();
        if (nodeType == "Exp")
            return std::make_unique<Exp>();
        if (nodeType == "Log")
            return std::make_unique<Log>();
        if (nodeType == "Log2")
            return std::make_unique<Log2>();
        if (nodeType == "Log10")
            return std::make_unique<Log10>();
        if (nodeType == "Sqrt")
            return std::make_unique<Sqrt>();
        if (nodeType == "Abs")
            return std::make_unique<Abs>();
        if (nodeType == "Min")
            return std::make_unique<Min>();
        if (nodeType == "Max")
            return std::make_unique<Max>();
        if (nodeType == "Fmod")
            return std::make_unique<Fmod>();
        if (nodeType == "Mod")
            return std::make_unique<Mod>();
        if (nodeType == "Select")
            return std::make_unique<Select>();
        if (nodeType == "Clamp")
            return std::make_unique<Clamp>();
        if (nodeType == "Length")
            return std::make_unique<Length>();
        if (nodeType == "DotProduct")
            return std::make_unique<DotProduct>();
        if (nodeType == "CrossProduct")
            return std::make_unique<CrossProduct>();
        if (nodeType == "MatrixVectorMultiplication")
            return std::make_unique<MatrixVectorMultiplication>();
        if (nodeType == "Transpose")
            return std::make_unique<Transpose>();
        if (nodeType == "Inverse")
            return std::make_unique<Inverse>();
        if (nodeType == "ConstantScalar")
            return std::make_unique<ConstantScalar>();
        if (nodeType == "ConstantVector")
            return std::make_unique<ConstantVector>();
        if (nodeType == "ConstantMatrix")
            return std::make_unique<ConstantMatrix>();
        if (nodeType == "DecomposeVector")
            return std::make_unique<DecomposeVector>();
        if (nodeType == "ComposeVector")
            return std::make_unique<ComposeVector>();
        if (nodeType == "DecomposeMatrix")
            return std::make_unique<DecomposeMatrix>();
        if (nodeType == "ComposeMatrix")
            return std::make_unique<ComposeMatrix>();
        if (nodeType == "ComposeMatrixFromColumns")
            return std::make_unique<ComposeMatrixFromColumns>();
        if (nodeType == "ComposeMatrixFromRows")
            return std::make_unique<ComposeMatrixFromRows>();
        if (nodeType == "FunctionCall")
            return std::make_unique<FunctionCall>();
        if (nodeType == "Resource")
            return std::make_unique<Resource>();
        if (nodeType == "Round")
            return std::make_unique<Round>();
        if (nodeType == "Ceil")
            return std::make_unique<Ceil>();
        if (nodeType == "Floor")
            return std::make_unique<Floor>();
        if (nodeType == "Fract")
            return std::make_unique<Fract>();
        if (nodeType == "Sign")
            return std::make_unique<Sign>();
        if (nodeType == "VectorFromScalar")
            return std::make_unique<VectorFromScalar>();

        return nullptr;
    }
}
