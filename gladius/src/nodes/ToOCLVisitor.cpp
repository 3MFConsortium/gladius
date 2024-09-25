#include "ToOCLVisitor.h"
#include "nodesfwd.h"
#include <fmt/format.h>
#include <iostream>

#include <src/nodes/Assembly.h>

namespace gladius::nodes
{
    void ToOclVisitor::setAssembly(Assembly * assembly)
    {
        m_assembly = assembly;
    }

    void ToOclVisitor::setModel(Model * const model)
    {
        m_currentModel = model;
        m_visitedNodes.clear();
    }

    void ToOclVisitor::write(std::ostream & out) const
    {
        out << "\n";
        out << m_declaration.str();
        out << "\n";
        out << m_definition.str();
    }

    auto ToOclVisitor::isOutPutOfNodeValid(NodeBase const & node) -> bool
    {
        if (m_endReached)
        {
            return false;
        }

        auto visitedIter =
          std::find(std::begin(m_visitedNodes), std::end(m_visitedNodes), node.getId());
        if (visitedIter != std::end(m_visitedNodes))
        {
            std::cerr << "Warning: Visiting already visited node " << node.getUniqueName() << "\n";
            return false;
        }
        m_visitedNodes.insert(node.getId());
        return true;
    }

    std::string typeIndexToOpenCl(std::type_index typeIndex)
    {
        if (typeIndex == ParameterTypeIndex::Float)
        {
            return "float";
        }
        else if (typeIndex == ParameterTypeIndex::Float3)
        {
            return "float3";
        }
        else if (typeIndex == ParameterTypeIndex::Matrix4)
        {
            return "float16";
        }
        else
        {
            throw std::runtime_error("Unknown type index");
        }
    }

    void ToOclVisitor::assemblyBegin(Begin & beginning)
    {
        m_declaration << fmt::format("float4 model(float3 {}, PAYLOAD_ARGS);\n",
                                     beginning.getOutputs()[FieldNames::Pos].getUniqueName());

        m_definition << fmt::format("float4 model(float3 {},  PAYLOAD_ARGS)\n{{\n",
                                    beginning.getOutputs()[FieldNames::Pos].getUniqueName());

        for (auto & [name, output] : beginning.getOutputs())
        {
            if (name == FieldNames::Pos)
            {
                continue;
            }
            if (beginning.parameter().find(name) == std::end(beginning.parameter()))
            {
                continue;
            }
            m_definition << fmt::format("float const {0} = {1};\n",
                                        output.getUniqueName(),
                                        beginning.parameter()[name].toString());
        }
    }

    void ToOclVisitor::visit(Begin & beginning)
    {
        m_endReached = false;

        bool isAssembly =
          m_currentModel->getResourceId() == m_assembly->assemblyModel()->getResourceId();

        auto const methodName =
          isAssembly ? std::string("model") : std::string(m_currentModel->getModelName());

        if (isAssembly)
        {
            assemblyBegin(beginning);
            return;
        }
        std::stringstream customArguments;
        bool first = true;
        for (auto & [name, input] : m_currentModel->getInputs())
        {
            // if (!input.isUsed())
            // {
            //     continue;
            // }

            if (!first)
            {
                customArguments << ", ";
            }
            customArguments << fmt::format(
              "{0} const {1}", typeIndexToOpenCl(input.getTypeIndex()), input.getUniqueName());
            first = false;
        }

        for (auto & [name, output] : m_currentModel->getOutputs())
        {
            if (!output.isConsumedByFunction())
            {
                continue;
            }

            if (!first)
            {
                customArguments << ", ";
            }
            customArguments << fmt::format(
              "{0} * {1}", typeIndexToOpenCl(output.getTypeIndex()), name);
            first = false;
        }

        if (first)
        {
            customArguments << "PAYLOAD_ARGS";
        }
        else
        {
            customArguments << ", PAYLOAD_ARGS";
        }

        m_declaration << fmt::format("void {0}({1});\n", methodName, customArguments.str());
        m_definition << fmt::format("void {0}({1})\n{{\n", methodName, customArguments.str());
    }

    void ToOclVisitor::visit(End & ending)
    {
        if (!isOutPutOfNodeValid(ending))
        {
            return;
        }
        m_endReached = true;

        bool isAssembly =
          m_currentModel->getResourceId() == m_assembly->assemblyModel()->getResourceId();

        if (isAssembly)
        {
            auto fallBackValue =
              m_assembly->getFallbackValueLevelSet(); // The fallback value is used if shape is not
                                                      // a number or infinity
            if (fallBackValue)
            {
                m_definition << fmt::format(
                  "return (float4)((float3)({0}),isnan({1})|| isinf({1}) ? {2} : {1});\n}}\n",
                  ending.parameter().at(FieldNames::Color).toString(),
                  ending.parameter().at(FieldNames::Shape).toString(),
                  *fallBackValue);
            }
            else
            {
                m_definition << fmt::format("return (float4)((float3)({0}),{1});\n}}\n",
                                            ending.parameter().at(FieldNames::Color).toString(),
                                            ending.parameter().at(FieldNames::Shape).toString());
            }
            return;
        }

        for (auto & [name, output] : m_currentModel->getOutputs())
        {
            if (!output.isConsumedByFunction())
            {
                continue;
            }

            m_definition << fmt::format("*{0} = ({1})({2});\n",
                                        name,
                                        typeIndexToOpenCl(output.getTypeIndex()),
                                        output.toString());
        }
        m_definition << "}\n";
    }

    void ToOclVisitor::visit(NodeBase &)
    {
    }

    void ToOclVisitor::visit(ConstantScalar & constantScalar)
    {
        if (!isOutPutOfNodeValid(constantScalar))
        {
            return;
        }

        m_definition << fmt::format("float const {0} = {1};\n",
                                    constantScalar.getOutputs()["value"].getUniqueName(),
                                    constantScalar.parameter()["value"].toString());
    }

    void ToOclVisitor::visit(ConstantVector & constantVector)
    {
        if (!isOutPutOfNodeValid(constantVector))
        {
            return;
        }

        m_definition << fmt::format("float3 const {0} = (float3)({1}, {2}, {3} );\n",
                                    constantVector.getOutputs()["vector"].getUniqueName(),
                                    constantVector.parameter()["x"].toString(),
                                    constantVector.parameter()["y"].toString(),
                                    constantVector.parameter()["z"].toString());
    }

    void ToOclVisitor::visit(ConstantMatrix & constantMatrix)
    {
        if (!isOutPutOfNodeValid(constantMatrix))
        {
            return;
        }

        m_definition << fmt::format("float16 const {0} = (float16)({1}, {2}, {3}, {4}, {5}, {6}, "
                                    "{7}, {8}, {9}, {10}, {11}, {12}, {13}, {14}, {15}, {16});\n",
                                    constantMatrix.getOutputs()["matrix"].getUniqueName(),
                                    constantMatrix.parameter()[FieldNames::M00].toString(),
                                    constantMatrix.parameter()[FieldNames::M01].toString(),
                                    constantMatrix.parameter()[FieldNames::M02].toString(),
                                    constantMatrix.parameter()[FieldNames::M03].toString(),
                                    constantMatrix.parameter()[FieldNames::M10].toString(),
                                    constantMatrix.parameter()[FieldNames::M11].toString(),
                                    constantMatrix.parameter()[FieldNames::M12].toString(),
                                    constantMatrix.parameter()[FieldNames::M13].toString(),
                                    constantMatrix.parameter()[FieldNames::M20].toString(),
                                    constantMatrix.parameter()[FieldNames::M21].toString(),
                                    constantMatrix.parameter()[FieldNames::M22].toString(),
                                    constantMatrix.parameter()[FieldNames::M23].toString(),
                                    constantMatrix.parameter()[FieldNames::M30].toString(),
                                    constantMatrix.parameter()[FieldNames::M31].toString(),
                                    constantMatrix.parameter()[FieldNames::M32].toString(),
                                    constantMatrix.parameter()[FieldNames::M33].toString());
    }

    void ToOclVisitor::visit(ComposeVector & composeVector)
    {
        if (!isOutPutOfNodeValid(composeVector))
        {
            return;
        }

        m_definition << fmt::format("float3 const {0} = (float3)({1}, {2}, {3} );\n",
                                    composeVector.getOutputs()[FieldNames::Result].getUniqueName(),
                                    composeVector.parameter()[FieldNames::X].toString(),
                                    composeVector.parameter()[FieldNames::Y].toString(),
                                    composeVector.parameter()[FieldNames::Z].toString());
    }

    void ToOclVisitor::visit(ComposeMatrix & composeMatrix)
    {
        if (!isOutPutOfNodeValid(composeMatrix))
        {
            return;
        }

        m_definition << fmt::format("float16 const {0} = (float16)({1}, {2}, {3}, {4}, {5}, {6}, "
                                    "{7}, {8}, {9}, {10}, {11}, {12}, {13}, {14}, {15}, {16});\n",
                                    composeMatrix.getOutputs()["matrix"].getUniqueName(),
                                    composeMatrix.parameter()[FieldNames::M00].toString(),
                                    composeMatrix.parameter()[FieldNames::M01].toString(),
                                    composeMatrix.parameter()[FieldNames::M02].toString(),
                                    composeMatrix.parameter()[FieldNames::M03].toString(),
                                    composeMatrix.parameter()[FieldNames::M10].toString(),
                                    composeMatrix.parameter()[FieldNames::M11].toString(),
                                    composeMatrix.parameter()[FieldNames::M12].toString(),
                                    composeMatrix.parameter()[FieldNames::M13].toString(),
                                    composeMatrix.parameter()[FieldNames::M20].toString(),
                                    composeMatrix.parameter()[FieldNames::M21].toString(),
                                    composeMatrix.parameter()[FieldNames::M22].toString(),
                                    composeMatrix.parameter()[FieldNames::M23].toString(),
                                    composeMatrix.parameter()[FieldNames::M30].toString(),
                                    composeMatrix.parameter()[FieldNames::M31].toString(),
                                    composeMatrix.parameter()[FieldNames::M32].toString(),
                                    composeMatrix.parameter()[FieldNames::M33].toString());
    }

    void ToOclVisitor::visit(ComposeMatrixFromColumns & composeMatrixFromColumns)
    {
        if (!isOutPutOfNodeValid(composeMatrixFromColumns))
        {
            return;
        }

        m_definition << fmt::format(
          "float16 const {0} = (float16)("
          "{1}.x, {2}.x, {3}.x, {4}.x,"
          "{1}.y, {2}.y, {3}.y, {4}.y,"
          "{1}.z, {2}.z, {3}.z, {4}.z,"
          "0.f, 0.f, 0.f, 1.f,"
          ");\n",
          composeMatrixFromColumns.getOutputs()["matrix"].getUniqueName(),
          composeMatrixFromColumns.parameter()[FieldNames::Col0].toString(),
          composeMatrixFromColumns.parameter()[FieldNames::Col1].toString(),
          composeMatrixFromColumns.parameter()[FieldNames::Col2].toString(),
          composeMatrixFromColumns.parameter()[FieldNames::Col3].toString());
    }

    void ToOclVisitor::visit(ComposeMatrixFromRows & composeMatrixFromRows)
    {
        if (!isOutPutOfNodeValid(composeMatrixFromRows))
        {
            return;
        }

        m_definition << fmt::format("float16 const {0} = (float16)("
                                    "{1}.x, {1}.y, {1}.z, 0.f,"
                                    "{2}.x, {2}.y, {2}.z, 0.f,"
                                    "{3}.x, {3}.y, {3}.z, 0.f,"
                                    "{4}.x, {4}.y, {4}.z, 1.f,"
                                    ");\n",
                                    composeMatrixFromRows.getOutputs()["matrix"].getUniqueName(),
                                    composeMatrixFromRows.parameter()[FieldNames::Row0].toString(),
                                    composeMatrixFromRows.parameter()[FieldNames::Row1].toString(),
                                    composeMatrixFromRows.parameter()[FieldNames::Row2].toString(),
                                    composeMatrixFromRows.parameter()[FieldNames::Row3].toString());
    }

    void ToOclVisitor::visit(DecomposeVector & decomposeVector)
    {
        if (!isOutPutOfNodeValid(decomposeVector))
        {
            return;
        }

        m_definition << fmt::format("float const {0} = ((float3)({1})).x;\n",
                                    decomposeVector.getOutputs()[FieldNames::X].getUniqueName(),
                                    decomposeVector.parameter()[FieldNames::A].toString());

        m_definition << fmt::format("float const {0} = ((float3)({1})).y;\n",
                                    decomposeVector.getOutputs()[FieldNames::Y].getUniqueName(),
                                    decomposeVector.parameter()[FieldNames::A].toString());

        m_definition << fmt::format("float const {0} = ((float3)({1})).z;\n",
                                    decomposeVector.getOutputs()[FieldNames::Z].getUniqueName(),
                                    decomposeVector.parameter()[FieldNames::A].toString());
    }

    void ToOclVisitor::visit(SignedDistanceToMesh & signedDistanceToMesh)
    {
        if (!isOutPutOfNodeValid(signedDistanceToMesh))
        {
            return;
        }

        m_definition << fmt::format(
          "float const {0} = payload((float3)({1}), (int)({2}), (int)({3}), PASS_PAYLOAD_ARGS);\n",
          signedDistanceToMesh.getOutputs()[FieldNames::Distance].getUniqueName(),
          signedDistanceToMesh.parameter()[FieldNames::Pos].toString(),
          signedDistanceToMesh.parameter()[FieldNames::Start].toString(),
          signedDistanceToMesh.parameter()[FieldNames::End].toString());
    }

    void ToOclVisitor::visit(FunctionCall & functionCall)
    {
        if (!isOutPutOfNodeValid(functionCall))
        {
            return;
        }

        functionCall.resolveFunctionId();
        auto const functionId = functionCall.getFunctionId();
        auto referencedModel = m_assembly->findModel(functionId);

        if (!referencedModel)
        {
            throw std::runtime_error(fmt::format(
              "Model {} referenced by {} not found", functionId, functionCall.getUniqueName()));
        }

        auto const functionName = referencedModel->getModelName();

        /*
            1. Create variables for all outputs
            2. Create function signature with all inputs and outputs passed as arguments by pointer
        */

        std::stringstream customArguments;
        bool first = true;
        for (auto & [name, parameter] : functionCall.parameter())
        {
            if (!parameter.isArgument())
            {
                continue;
            }

            if (parameter.toString().empty())
            {
                continue;
            }

            if (!first)
            {
                customArguments << ", ";
            }

            customArguments << fmt::format(
              "({0})({1})", typeIndexToOpenCl(parameter.getTypeIndex()), parameter.toString());
            first = false;
        }

        for (auto & [name, port] : functionCall.getOutputs())
        {
            if (!port.isUsed())
            {
                continue;
            }

            if (!first)
            {
                customArguments << ", ";
            }

            m_definition << fmt::format("{0} {1} = ({0})(0.f);\n",
                                        typeIndexToOpenCl(port.getTypeIndex()),
                                        port.getUniqueName());
            customArguments << fmt::format("&{0}", port.getUniqueName());
            first = false;
        }
        auto const customArgumentsStr = customArguments.str();
        if (customArgumentsStr.empty())
        {
            m_definition << fmt::format("{0}(PASS_PAYLOAD_ARGS);\n", functionName);
        }
        else
        {
            m_definition << fmt::format(
              "{0}({1}, PASS_PAYLOAD_ARGS);\n", functionName, customArgumentsStr);
        }
    }

    void ToOclVisitor::visit(Addition & addition)
    {
        if (!isOutPutOfNodeValid(addition))
        {
            return;
        }

        if (addition.getOutputs().at(FieldNames::Result).getTypeIndex() ==
            ParameterTypeIndex::Float)
        {
            m_definition << fmt::format(
              "float const {0} = {1} + {2};\n",
              addition.getOutputs().at(FieldNames::Result).getUniqueName(),
              addition.parameter().at(FieldNames::A).toString(),
              addition.parameter().at(FieldNames::B).toString());
        }
        else if (addition.getOutputs().at(FieldNames::Result).getTypeIndex() ==
                 ParameterTypeIndex::Float3)
        {
            m_definition << fmt::format(
              "float3 const {0} = (float3)({1}) + (float3)({2});\n",
              addition.getOutputs().at(FieldNames::Result).getUniqueName(),
              addition.parameter().at(FieldNames::A).toString(),
              addition.parameter().at(FieldNames::B).toString());
        }
    }

    void ToOclVisitor::visit(Subtraction & subtraction)
    {
        if (!isOutPutOfNodeValid(subtraction))
        {
            return;
        }
        if (subtraction.getOutputs().at(FieldNames::Result).getTypeIndex() ==
            ParameterTypeIndex::Float)
        {
            m_definition << fmt::format(
              "float const {0} = {1} - {2};\n",
              subtraction.getOutputs().at(FieldNames::Result).getUniqueName(),
              subtraction.parameter().at(FieldNames::A).toString(),
              subtraction.parameter().at(FieldNames::B).toString());
        }
        else if (subtraction.getOutputs().at(FieldNames::Result).getTypeIndex() ==
                 ParameterTypeIndex::Float3)
        {
            m_definition << fmt::format(
              "float3 const {0} = (float3)({1}) - (float3)({2});\n",
              subtraction.getOutputs().at(FieldNames::Result).getUniqueName(),
              subtraction.parameter().at(FieldNames::A).toString(),
              subtraction.parameter().at(FieldNames::B).toString());
        }
    }

    void ToOclVisitor::visit(Multiplication & multiplication)
    {
        if (!isOutPutOfNodeValid(multiplication))
        {
            return;
        }
        if (multiplication.getOutputs().at(FieldNames::Result).getTypeIndex() ==
            ParameterTypeIndex::Float)
        {
            m_definition << fmt::format(
              "float const {0} = {1} * {2};\n",
              multiplication.getOutputs().at(FieldNames::Result).getUniqueName(),
              multiplication.parameter().at(FieldNames::A).toString(),
              multiplication.parameter().at(FieldNames::B).toString());
        }
        else if (multiplication.getOutputs().at(FieldNames::Result).getTypeIndex() ==
                 ParameterTypeIndex::Float3)
        {
            m_definition << fmt::format(
              "float3 const {0} = (float3)({1}) * (float3)({2});\n",
              multiplication.getOutputs().at(FieldNames::Result).getUniqueName(),
              multiplication.parameter().at(FieldNames::A).toString(),
              multiplication.parameter().at(FieldNames::B).toString());
        }
    }

    void ToOclVisitor::visit(Division & division)
    {
        if (!isOutPutOfNodeValid(division))
        {
            return;
        }
        if (division.getOutputs().at(FieldNames::Result).getTypeIndex() ==
            ParameterTypeIndex::Float)
        {
            m_definition << fmt::format(
              "float const {0} = {1} / {2};\n",
              division.getOutputs().at(FieldNames::Result).getUniqueName(),
              division.parameter().at(FieldNames::A).toString(),
              division.parameter().at(FieldNames::B).toString());
        }
        else if (division.getOutputs().at(FieldNames::Result).getTypeIndex() ==
                 ParameterTypeIndex::Float3)
        {
            m_definition << fmt::format(
              "float3 const {0} = (float3)({1}) / (float3)({2});\n",
              division.getOutputs().at(FieldNames::Result).getUniqueName(),
              division.parameter().at(FieldNames::A).toString(),
              division.parameter().at(FieldNames::B).toString());
        }
    }

    void ToOclVisitor::visit(DotProduct & dotProduct)
    {
        if (!isOutPutOfNodeValid(dotProduct))
        {
            return;
        }
        m_definition << fmt::format("float const {0} = dot({1}, {2});\n",
                                    dotProduct.getOutputs()[FieldNames::Result].getUniqueName(),
                                    dotProduct.parameter()[FieldNames::A].toString(),
                                    dotProduct.parameter()[FieldNames::B].toString());
    }

    void ToOclVisitor::visit(CrossProduct & crossProduct)
    {
        if (!isOutPutOfNodeValid(crossProduct))
        {
            return;
        }
        m_definition << fmt::format("float3 const {0} = cross({1}, {2});\n",
                                    crossProduct.getOutputs()[FieldNames::Result].getUniqueName(),
                                    crossProduct.parameter()[FieldNames::A].toString(),
                                    crossProduct.parameter()[FieldNames::B].toString());
    }

    void ToOclVisitor::visit(MatrixVectorMultiplication & matrixVectorMultiplication)
    {
        if (!isOutPutOfNodeValid(matrixVectorMultiplication))
        {
            return;
        }
        m_definition << fmt::format(
          "float3 const {0} = matrixVectorMul3f((float16)({1}), {2});\n",
          matrixVectorMultiplication.getOutputs()[FieldNames::Result].getUniqueName(),
          matrixVectorMultiplication.parameter().at(FieldNames::A).toString(),
          matrixVectorMultiplication.parameter().at(FieldNames::B).toString());
    }

    void ToOclVisitor::visit(Transpose & transpose)
    {
        if (!isOutPutOfNodeValid(transpose))
        {
            return;
        }
        m_definition << fmt::format("float16 const {0} = transpose((float16)({1}));\n",
                                    transpose.getOutputs()[FieldNames::Result].getUniqueName(),
                                    transpose.parameter()[FieldNames::Matrix].toString());
    }

    void ToOclVisitor::visit(Sine & sine)
    {
        if (!isOutPutOfNodeValid(sine))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = sin({2});\n",
          typeIndexToOpenCl(sine.getOutputs().at(FieldNames::Result).getTypeIndex()),
          sine.getOutputs().at(FieldNames::Result).getUniqueName(),
          sine.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Cosine & cosine)
    {
        if (!isOutPutOfNodeValid(cosine))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = cos({2});\n",
          typeIndexToOpenCl(cosine.getOutputs().at(FieldNames::Result).getTypeIndex()),
          cosine.getOutputs().at(FieldNames::Result).getUniqueName(),
          cosine.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Tangent & tangent)
    {
        if (!isOutPutOfNodeValid(tangent))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = tan({2});\n",
          typeIndexToOpenCl(tangent.getOutputs().at(FieldNames::Result).getTypeIndex()),
          tangent.getOutputs().at(FieldNames::Result).getUniqueName(),
          tangent.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(ArcSin & arcSin)
    {
        if (!isOutPutOfNodeValid(arcSin))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = asin({2});\n",
          typeIndexToOpenCl(arcSin.getOutputs().at(FieldNames::Result).getTypeIndex()),
          arcSin.getOutputs().at(FieldNames::Result).getUniqueName(),
          arcSin.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(ArcCos & arcCos)
    {
        if (!isOutPutOfNodeValid(arcCos))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = acos({2});\n",
          typeIndexToOpenCl(arcCos.getOutputs().at(FieldNames::Result).getTypeIndex()),
          arcCos.getOutputs().at(FieldNames::Result).getUniqueName(),
          arcCos.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(ArcTan & arcTan)
    {
        if (!isOutPutOfNodeValid(arcTan))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = atan({2});\n",
          typeIndexToOpenCl(arcTan.getOutputs().at(FieldNames::Result).getTypeIndex()),
          arcTan.getOutputs().at(FieldNames::Result).getUniqueName(),
          arcTan.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Pow & power)
    {
        if (!isOutPutOfNodeValid(power))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = pow({0})({2}), {0})({3}));\n",
          typeIndexToOpenCl(power.getOutputs().at(FieldNames::Result).getTypeIndex()),
          power.getOutputs().at(FieldNames::Result).getUniqueName(),
          power.parameter().at(FieldNames::A).toString(),
          power.parameter().at(FieldNames::B).toString());
    }

    void ToOclVisitor::visit(Sqrt & sqrtNode)
    {
        if (!isOutPutOfNodeValid(sqrtNode))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = sqrt({0})({2}));\n",
          typeIndexToOpenCl(sqrtNode.getOutputs().at(FieldNames::Result).getTypeIndex()),
          sqrtNode.getOutputs().at(FieldNames::Result).getUniqueName(),
          sqrtNode.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Fmod & modulus)
    {
        if (!isOutPutOfNodeValid(modulus))
        {
            return;
        }

        auto const numCopmonents = modulus.parameter().at(FieldNames::A).getSize();

        m_definition << fmt::format(
          "{0} const {1} = fmod(({0})({2}), ({0})({3}));\n",
          typeIndexToOpenCl(modulus.getOutputs().at(FieldNames::Result).getTypeIndex()),
          modulus.getOutputs().at(FieldNames::Result).getUniqueName(),
          modulus.parameter().at(FieldNames::A).toString(),
          modulus.parameter().at(FieldNames::B).toString(),
          numCopmonents);
    }

    void ToOclVisitor::visit(Mod & modulus)
    {
        if (!isOutPutOfNodeValid(modulus))
        {
            return;
        }

        auto const numCopmonents = modulus.parameter().at(FieldNames::A).getSize();

        m_definition << fmt::format(
          "{0} const {1} = glsl_mod{4}f(({0})({2}), ({0})({3}));\n",
          typeIndexToOpenCl(modulus.getOutputs().at(FieldNames::Result).getTypeIndex()),
          modulus.getOutputs().at(FieldNames::Result).getUniqueName(),
          modulus.parameter().at(FieldNames::A).toString(),
          modulus.parameter().at(FieldNames::B).toString(),
          numCopmonents);
    }

    void ToOclVisitor::visit(Max & maxNode)
    {
        if (!isOutPutOfNodeValid(maxNode))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = max(({0})({2}), ({0})({3}));\n",
          typeIndexToOpenCl(maxNode.getOutputs().at(FieldNames::Result).getTypeIndex()),
          maxNode.getOutputs().at(FieldNames::Result).getUniqueName(),
          maxNode.parameter().at(FieldNames::A).toString(),
          maxNode.parameter().at(FieldNames::B).toString());
    }

    void ToOclVisitor::visit(Min & minNode)
    {
        if (!isOutPutOfNodeValid(minNode))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = min(({0})({2}), ({0})({3}));\n",
          typeIndexToOpenCl(minNode.getOutputs().at(FieldNames::Result).getTypeIndex()),
          minNode.getOutputs().at(FieldNames::Result).getUniqueName(),
          minNode.parameter().at(FieldNames::A).toString(),
          minNode.parameter().at(FieldNames::B).toString());
    }

    void ToOclVisitor::visit(Abs & absNode)
    {
        if (!isOutPutOfNodeValid(absNode))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = fabs(({0})({2}));\n",
          typeIndexToOpenCl(absNode.getOutputs().at(FieldNames::Result).getTypeIndex()),
          absNode.getOutputs().at(FieldNames::Result).getUniqueName(),
          absNode.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Length & lengthNode)
    {
        if (!isOutPutOfNodeValid(lengthNode))
        {
            return;
        }
        m_definition << fmt::format("float const {0} = length((float3)({1}));\n",
                                    lengthNode.getOutputs().at(FieldNames::Result).getUniqueName(),
                                    lengthNode.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Mix & mixNode)
    {
        if (!isOutPutOfNodeValid(mixNode))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = mix(({0})({2}), ({0})({3}), ({0})({4}));\n",
          typeIndexToOpenCl(mixNode.getOutputs().at(FieldNames::Result).getTypeIndex()),
          mixNode.getOutputs().at(FieldNames::Result).getUniqueName(),
          mixNode.parameter().at(FieldNames::A).toString(),
          mixNode.parameter().at(FieldNames::B).toString(),
          mixNode.parameter().at(FieldNames::Ratio).toString());
    }

    void ToOclVisitor::visit(Transformation & transformation)
    {

        if (!isOutPutOfNodeValid(transformation))
        {
            return;
        }

        m_definition << fmt::format(
          "float3 const {0} = matrixVectorMul3f((float16)({1}), {2});\n",
          transformation.getOutputs()[FieldNames::Pos].getUniqueName(),
          transformation.parameter()[FieldNames::Transformation].toString(),
          transformation.parameter()[FieldNames::Pos].toString());
    }

    // Resource
    void ToOclVisitor::visit(Resource & resource)
    {
        return; // The content of resource is handled by the consuming node
    }

    std::string wrapMethodFromTileStyle(TextureTileStyle style)
    {
        switch (style)
        {
        case TTS_REPEAT:
            return "wrap";
        case TTS_MIRROR:
            return "mirrorRepeated";
        case TTS_CLAMP:
            return "clamp01";
        default:
            throw std::runtime_error("Unknown tile style");
        }
    }

    void ToOclVisitor::visit(ImageSampler & imageSampler)
    {
        if (!isOutPutOfNodeValid(imageSampler))
        {
            return;
        }

        std::string samplerName =
          (imageSampler.getFilter() == SF_NEAREST ? "sampleImageNearest4f" : "sampleImageLinear4f");

        if (imageSampler.isVdbGrid())
        {
            samplerName = "sampleImageLinear4fvdb";
        }

        m_definition << fmt::format("int3 const {0}_tileStyle = (int3)({1}, {2}, {3});\n",
                                    imageSampler.getUniqueName(),
                                    static_cast<int>(imageSampler.getTileStyleU()),
                                    static_cast<int>(imageSampler.getTileStyleV()),
                                    static_cast<int>(imageSampler.getTileStyleW()));

        m_definition << fmt::format("float4 const {0}_rgba = {1}((float3)({2}), (float3)({3}), "
                                    "{4}, {5}_tileStyle, PASS_PAYLOAD_ARGS);\n",
                                    imageSampler.getOutputs().at(FieldNames::Color).getUniqueName(),
                                    samplerName,
                                    imageSampler.parameter().at(FieldNames::UVW).toString(),
                                    imageSampler.parameter().at(FieldNames::Dimensions).toString(),
                                    imageSampler.parameter().at(FieldNames::Start).toString(),
                                    imageSampler.getUniqueName());

        m_definition << fmt::format(
          "float3 const {0} = {1}_rgba.xyz;\n",
          imageSampler.getOutputs().at(FieldNames::Color).getUniqueName(),
          imageSampler.getOutputs().at(FieldNames::Color).getUniqueName());

        m_definition << fmt::format(
          "float const {0} = {1}_rgba.w;\n",
          imageSampler.getOutputs().at(FieldNames::Alpha).getUniqueName(),
          imageSampler.getOutputs().at(FieldNames::Color).getUniqueName());
    }

    void ToOclVisitor::visit(DecomposeMatrix & decomposeMatrixNode)
    {
        if (!isOutPutOfNodeValid(decomposeMatrixNode))
        {
            return;
        }

        m_definition << fmt::format(
          "float const {0} = {1}.s0;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M00].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s1;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M01].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s2;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M02].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s3;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M03].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s4;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M10].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s5;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M11].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s6;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M12].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s7;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M13].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s8;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M20].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.s9;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M21].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.sa;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M22].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.sb;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M23].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.sc;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M30].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.sd;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M31].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.se;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M32].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());

        m_definition << fmt::format(
          "float const {0} = {1}.sf;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M33].getUniqueName(),
          decomposeMatrixNode.parameter()[FieldNames::Matrix].toString());
    }

    void ToOclVisitor::visit(Inverse & inverse)
    {
        if (!isOutPutOfNodeValid(inverse))
        {
            return;
        }

        m_definition << fmt::format("float16 const {0} = inverse((float16)({1}));\n",
                                    inverse.getOutputs()[FieldNames::Result].getUniqueName(),
                                    inverse.parameter()[FieldNames::Matrix].toString());
    }

    void ToOclVisitor::visit(ArcTan2 & arcTan2)
    {
        if (!isOutPutOfNodeValid(arcTan2))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = atan2({2}, {3});\n",
          typeIndexToOpenCl(arcTan2.getOutputs().at(FieldNames::Result).getTypeIndex()),
          arcTan2.getOutputs().at(FieldNames::Result).getUniqueName(),
          arcTan2.parameter().at(FieldNames::A).toString(),
          arcTan2.parameter().at(FieldNames::B).toString());
    }

    void ToOclVisitor::visit(Exp & exp)
    {
        if (!isOutPutOfNodeValid(exp))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = exp({2});\n",
          typeIndexToOpenCl(exp.getOutputs().at(FieldNames::Result).getTypeIndex()),
          exp.getOutputs().at(FieldNames::Result).getUniqueName(),
          exp.parameter().at(FieldNames::A).toString());
    }
    void ToOclVisitor::visit(Log & log)
    {
        if (!isOutPutOfNodeValid(log))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = log({2});\n",
          typeIndexToOpenCl(log.getOutputs().at(FieldNames::Result).getTypeIndex()),
          log.getOutputs().at(FieldNames::Result).getUniqueName(),
          log.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Log2 & log2)
    {
        if (!isOutPutOfNodeValid(log2))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = log2({2});\n",
          typeIndexToOpenCl(log2.getOutputs().at(FieldNames::Result).getTypeIndex()),
          log2.getOutputs().at(FieldNames::Result).getUniqueName(),
          log2.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Log10 & log10)
    {
        if (!isOutPutOfNodeValid(log10))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = log10({2});\n",
          typeIndexToOpenCl(log10.getOutputs().at(FieldNames::Result).getTypeIndex()),
          log10.getOutputs().at(FieldNames::Result).getUniqueName(),
          log10.parameter().at(FieldNames::A).toString());
    }

    // result = A < B ? C : D
    void ToOclVisitor::visit(Select & select)
    {
        if (!isOutPutOfNodeValid(select))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = {2} < {3} ? {4} : {5};\n",
          typeIndexToOpenCl(select.getOutputs().at(FieldNames::Result).getTypeIndex()),
          select.getOutputs().at(FieldNames::Result).getUniqueName(),
          select.parameter().at(FieldNames::A).toString(),
          select.parameter().at(FieldNames::B).toString(),
          select.parameter().at(FieldNames::C).toString(),
          select.parameter().at(FieldNames::D).toString());
    }

    void ToOclVisitor::visit(Clamp & clamp)
    {
        if (!isOutPutOfNodeValid(clamp))
        {
            return;
        }

        m_definition << fmt::format(
          "{0} const {1} = clamp({2}, {3}, {4});\n",
          typeIndexToOpenCl(clamp.getOutputs().at(FieldNames::Result).getTypeIndex()),
          clamp.getOutputs().at(FieldNames::Result).getUniqueName(),
          clamp.parameter().at(FieldNames::A).toString(),
          clamp.parameter().at(FieldNames::Min).toString(),
          clamp.parameter().at(FieldNames::Max).toString());
    }

    void ToOclVisitor::visit(SinH & sinh)
    {
        if (!isOutPutOfNodeValid(sinh))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = sinh({2});\n",
          typeIndexToOpenCl(sinh.getOutputs().at(FieldNames::Result).getTypeIndex()),
          sinh.getOutputs().at(FieldNames::Result).getUniqueName(),
          sinh.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(CosH & cosh)
    {
        if (!isOutPutOfNodeValid(cosh))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = cosh({2});\n",
          typeIndexToOpenCl(cosh.getOutputs().at(FieldNames::Result).getTypeIndex()),
          cosh.getOutputs().at(FieldNames::Result).getUniqueName(),
          cosh.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(TanH & tanh)
    {
        if (!isOutPutOfNodeValid(tanh))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = tanh({2});\n",
          typeIndexToOpenCl(tanh.getOutputs().at(FieldNames::Result).getTypeIndex()),
          tanh.getOutputs().at(FieldNames::Result).getUniqueName(),
          tanh.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Round & round)
    {
        if (!isOutPutOfNodeValid(round))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = round({2});\n",
          typeIndexToOpenCl(round.getOutputs().at(FieldNames::Result).getTypeIndex()),
          round.getOutputs().at(FieldNames::Result).getUniqueName(),
          round.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Ceil & ceil)
    {
        if (!isOutPutOfNodeValid(ceil))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = ceil({2});\n",
          typeIndexToOpenCl(ceil.getOutputs().at(FieldNames::Result).getTypeIndex()),
          ceil.getOutputs().at(FieldNames::Result).getUniqueName(),
          ceil.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Floor & floor)
    {
        if (!isOutPutOfNodeValid(floor))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = floor({2});\n",
          typeIndexToOpenCl(floor.getOutputs().at(FieldNames::Result).getTypeIndex()),
          floor.getOutputs().at(FieldNames::Result).getUniqueName(),
          floor.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Sign & sign)
    {
        if (!isOutPutOfNodeValid(sign))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = sign({2});\n",
          typeIndexToOpenCl(sign.getOutputs().at(FieldNames::Result).getTypeIndex()),
          sign.getOutputs().at(FieldNames::Result).getUniqueName(),
          sign.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(Fract & fract)
    {
        if (!isOutPutOfNodeValid(fract))
        {
            return;
        }
        m_definition << fmt::format(
          "{0} const {1} = fract({2});\n",
          typeIndexToOpenCl(fract.getOutputs().at(FieldNames::Result).getTypeIndex()),
          fract.getOutputs().at(FieldNames::Result).getUniqueName(),
          fract.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(VectorFromScalar & vectorFromScalar)
    {
        if (!isOutPutOfNodeValid(vectorFromScalar))
        {
            return;
        }
        m_definition << fmt::format(
          "float3 const {0} = (float3)({1},{1},{1});\n",
          vectorFromScalar.getOutputs().at(FieldNames::Result).getUniqueName(),
          vectorFromScalar.parameter().at(FieldNames::A).toString());
    }

    void ToOclVisitor::visit(UnsignedDistanceToMesh & unsignedDistanceToMesh)
    {
        if (!isOutPutOfNodeValid(unsignedDistanceToMesh))
        {
            return;
        }

        m_definition << fmt::format(
          "float const {0} = fabs(payload((float3)({1}), (int)({2}), (int)({3}), "
          "PASS_PAYLOAD_ARGS));\n", // Todo: Use optimized method for unsigned distance
          unsignedDistanceToMesh.getOutputs()[FieldNames::Distance].getUniqueName(),
          unsignedDistanceToMesh.parameter()[FieldNames::Pos].toString(),
          unsignedDistanceToMesh.parameter()[FieldNames::Start].toString(),
          unsignedDistanceToMesh.parameter()[FieldNames::End].toString());
    }

    void ToOclVisitor::visit(BoxMinMax & boxMinMax)
    {
        if (!isOutPutOfNodeValid(boxMinMax))
        {
            return;
        }

        m_definition << fmt::format("float const {0} = bbBox({1}, {2}, {3});\n",
                                    boxMinMax.getOutputs().at(FieldNames::Shape).getUniqueName(),
                                    boxMinMax.parameter().at(FieldNames::Pos).toString(),
                                    boxMinMax.parameter().at(FieldNames::Min).toString(),
                                    boxMinMax.parameter().at(FieldNames::Max).toString());
    }
} // namespace gladius::nodes
