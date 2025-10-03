#include "ToOCLVisitor.h"
#include "nodesfwd.h"
#include <fmt/format.h>
#include <iostream>

#include <nodes/Assembly.h>

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
        m_referenceAnalysisPerformed = false;
        m_inlineExpressions.clear();
    }

    void ToOclVisitor::write(std::ostream & out) const
    {
        out << "\n";
        out << m_declaration.str();
        out << "\n";
        out << m_definition.str();
    }

    bool ToOclVisitor::shouldInlineOutput(NodeBase const & node, std::string const & portName) const
    {
        // Perform reference analysis if not yet done
        if (!m_referenceAnalysisPerformed && m_currentModel)
        {
            m_referenceAnalyzer.setModel(m_currentModel);
            m_referenceAnalyzer.analyze();
            m_referenceAnalysisPerformed = true;
        }

        // Check if this output should be inlined
        return m_referenceAnalyzer.shouldInline(node.getId(), portName);
    }

    std::string ToOclVisitor::resolveParameter(IParameter const & param) const
    {
        // Check if the parameter has a source (i.e., connected to an output port)
        auto const & source = param.getConstSource();
        if (source.has_value())
        {
            // Check if this source has an inline expression
            auto const key = std::make_pair(source->nodeId, std::string(source->shortName));
            auto const it = m_inlineExpressions.find(key);
            if (it != m_inlineExpressions.end())
            {
                return it->second;
            }
        }
        
        // Fall back to the default toString() behavior
        return const_cast<IParameter&>(param).toString();
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
            // Already visited, skip without noisy console output
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

    void ToOclVisitor::emitUnaryOperation(NodeBase & node, std::string const & operation, 
                                         std::string const & outputPortName)
    {
        if (!isOutPutOfNodeValid(node))
        {
            return;
        }

        auto const & outputPort = node.getOutputs().at(outputPortName);
        std::string const typeName = typeIndexToOpenCl(outputPort.getTypeIndex());
        std::string const inputExpr = resolveParameter(node.parameter().at(FieldNames::A));
        
        // Build the expression
        std::string const expression = fmt::format("{}(({})({}))", operation, typeName, inputExpr);
        
        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(node, outputPortName);
        
        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(node.getId(), outputPortName);
            m_inlineExpressions[key] = expression;
            
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       outputPort.getUniqueName(), expression);
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                       typeName, outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::emitBinaryOperation(NodeBase & node, std::string const & operation, 
                                          std::string const & outputPortName,
                                          std::string const & param1Name,
                                          std::string const & param2Name)
    {
        if (!isOutPutOfNodeValid(node))
        {
            return;
        }

        auto const & outputPort = node.getOutputs().at(outputPortName);
        std::string const typeName = typeIndexToOpenCl(outputPort.getTypeIndex());
        std::string const param1Expr = resolveParameter(node.parameter().at(param1Name));
        std::string const param2Expr = resolveParameter(node.parameter().at(param2Name));
        
        // Build the expression
        std::string const expression = fmt::format("{}(({})({}) , ({})({}))", 
                                                   operation, typeName, param1Expr, typeName, param2Expr);
        
        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(node, outputPortName);
        
        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(node.getId(), outputPortName);
            m_inlineExpressions[key] = expression;
            
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       outputPort.getUniqueName(), expression);
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                       typeName, outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::emitTernaryOperation(NodeBase & node, std::string const & operation,
                                           std::string const & outputPortName,
                                           std::string const & param1Name,
                                           std::string const & param2Name,
                                           std::string const & param3Name)
    {
        if (!isOutPutOfNodeValid(node))
        {
            return;
        }

        auto const & outputPort = node.getOutputs().at(outputPortName);
        std::string const typeName = typeIndexToOpenCl(outputPort.getTypeIndex());
        std::string const param1Expr = resolveParameter(node.parameter().at(param1Name));
        std::string const param2Expr = resolveParameter(node.parameter().at(param2Name));
        std::string const param3Expr = resolveParameter(node.parameter().at(param3Name));
        
        // Build the expression
        std::string const expression = fmt::format("{}({}, {}, {})", 
                                                   operation, param1Expr, param2Expr, param3Expr);
        
        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(node, outputPortName);
        
        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(node.getId(), outputPortName);
            m_inlineExpressions[key] = expression;
            
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       outputPort.getUniqueName(), expression);
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                       typeName, outputPort.getUniqueName(), expression);
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
                  resolveParameter(ending.parameter().at(FieldNames::Color)),
                  resolveParameter(ending.parameter().at(FieldNames::Shape)),
                  *fallBackValue);
            }
            else
            {
                m_definition << fmt::format("return (float4)((float3)({0}),{1});\n}}\n",
                                            resolveParameter(ending.parameter().at(FieldNames::Color)),
                                            resolveParameter(ending.parameter().at(FieldNames::Shape)));
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
                                    constantScalar.getValueOutputPort().getUniqueName(),
                                    constantScalar.parameter()["value"].toString());
    }

    void ToOclVisitor::visit(ConstantVector & constantVector)
    {
        if (!isOutPutOfNodeValid(constantVector))
        {
            return;
        }

        m_definition << fmt::format("float3 const {0} = (float3)({1}, {2}, {3} );\n",
                                    constantVector.getVectorOutputPort().getUniqueName(),
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
                                    constantMatrix.getMatrixOutputPort().getUniqueName(),
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

        m_definition << fmt::format(fmt::runtime("float3 const {0} = (float3)({1}, {2}, {3});\n"),
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

        m_definition << fmt::format(
          fmt::runtime("float16 const {0} = (float16)({1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, "
                       "{10}, {11}, {12}, {13}, {14}, {15}, {16});\n"),
          composeMatrix.getOutputs()[FieldNames::Matrix].getUniqueName(),
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
          fmt::runtime("float16 const {0} = (float16)({1}.x, {2}.x, {3}.x, {4}.x, {1}.y, {2}.y, "
                       "{3}.y, {4}.y, {1}.z, {2}.z, {3}.z, {4}.z, 0.f, 0.f, 0.f, 1.f);\n"),
          composeMatrixFromColumns.getOutputs()[FieldNames::Matrix].getUniqueName(),
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

        m_definition << fmt::format(
          fmt::runtime("float16 const {0} = (float16)({1}.x, {1}.y, {1}.z, 0.f, {2}.x, {2}.y, "
                       "{2}.z, 0.f, {3}.x, {3}.y, {3}.z, 0.f, {4}.x, {4}.y, {4}.z, 1.f);\n"),
          composeMatrixFromRows.getOutputs()[FieldNames::Matrix].getUniqueName(),
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

    void ToOclVisitor::visit(SignedDistanceToBeamLattice & signedDistanceToBeamLattice)
    {
        if (!isOutPutOfNodeValid(signedDistanceToBeamLattice))
        {
            return;
        }

        m_definition << fmt::format(
          "float const {0} = payload((float3)({1}), (int)({2}), (int)({3}), PASS_PAYLOAD_ARGS);\n",
          signedDistanceToBeamLattice.getOutputs()[FieldNames::Distance].getUniqueName(),
          signedDistanceToBeamLattice.parameter()[FieldNames::Pos].toString(),
          signedDistanceToBeamLattice.parameter()[FieldNames::Start].toString(),
          signedDistanceToBeamLattice.parameter()[FieldNames::End].toString());
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

        // Resolve input parameters (may use inlined expressions)
        std::string const aExpr = resolveParameter(addition.parameter().at(FieldNames::A));
        std::string const bExpr = resolveParameter(addition.parameter().at(FieldNames::B));
        
        // Build the addition expression
        std::string expression;
        std::string typeName;
        
        if (addition.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float)
        {
            typeName = "float";
            expression = fmt::format("({} + {})", aExpr, bExpr);
        }
        else if (addition.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float3)
        {
            typeName = "float3";
            expression = fmt::format("({} + {})", aExpr, bExpr);
        }
        else
        {
            // Unknown type, skip
            return;
        }
        
        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(addition, FieldNames::Result);
        
        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(addition.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
            
            // Add a comment for debugging
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       addition.getResultOutputPort().getUniqueName(),
                                       expression);
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                       typeName,
                                       addition.getResultOutputPort().getUniqueName(),
                                       expression);
        }
    }    void ToOclVisitor::visit(Subtraction & subtraction)
    {
        if (!isOutPutOfNodeValid(subtraction))
        {
            return;
        }
        
        // Resolve input parameters (may use inlined expressions)
        std::string const aExpr = resolveParameter(subtraction.parameter().at(FieldNames::A));
        std::string const bExpr = resolveParameter(subtraction.parameter().at(FieldNames::B));
        
        // Build the subtraction expression
        std::string expression;
        std::string typeName;
        
        if (subtraction.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float)
        {
            typeName = "float";
            expression = fmt::format("({} - {})", aExpr, bExpr);
        }
        else if (subtraction.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float3)
        {
            typeName = "float3";
            expression = fmt::format("((float3)({}) - (float3)({}))", aExpr, bExpr);
        }
        else
        {
            return;
        }
        
        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(subtraction, FieldNames::Result);
        
        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(subtraction.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
            
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       subtraction.getResultOutputPort().getUniqueName(),
                                       expression);
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                       typeName,
                                       subtraction.getResultOutputPort().getUniqueName(),
                                       expression);
        }
    }

    void ToOclVisitor::visit(Multiplication & multiplication)
    {
        if (!isOutPutOfNodeValid(multiplication))
        {
            return;
        }
        
        // Resolve input parameters (may use inlined expressions)
        std::string const aExpr = resolveParameter(multiplication.parameter().at(FieldNames::A));
        std::string const bExpr = resolveParameter(multiplication.parameter().at(FieldNames::B));
        
        // Build the multiplication expression
        std::string expression;
        std::string typeName;
        
        if (multiplication.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float)
        {
            typeName = "float";
            expression = fmt::format("({} * {})", aExpr, bExpr);
        }
        else if (multiplication.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float3)
        {
            typeName = "float3";
            expression = fmt::format("((float3)({}) * (float3)({}))", aExpr, bExpr);
        }
        else
        {
            return;
        }
        
        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(multiplication, FieldNames::Result);
        
        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(multiplication.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
            
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       multiplication.getResultOutputPort().getUniqueName(),
                                       expression);
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                       typeName,
                                       multiplication.getResultOutputPort().getUniqueName(),
                                       expression);
        }
    }

    void ToOclVisitor::visit(Division & division)
    {
        if (!isOutPutOfNodeValid(division))
        {
            return;
        }
        
        // Resolve input parameters (may use inlined expressions)
        std::string const aExpr = resolveParameter(division.parameter().at(FieldNames::A));
        std::string const bExpr = resolveParameter(division.parameter().at(FieldNames::B));
        
        // Build the division expression
        std::string expression;
        std::string typeName;
        
        if (division.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float)
        {
            typeName = "float";
            expression = fmt::format("({} / {})", aExpr, bExpr);
        }
        else if (division.getResultOutputPort().getTypeIndex() == ParameterTypeIndex::Float3)
        {
            typeName = "float3";
            expression = fmt::format("((float3)({}) / (float3)({}))", aExpr, bExpr);
        }
        else
        {
            return;
        }
        
        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(division, FieldNames::Result);
        
        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(division.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
            
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       division.getResultOutputPort().getUniqueName(),
                                       expression);
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                       typeName,
                                       division.getResultOutputPort().getUniqueName(),
                                       expression);
        }
    }

    void ToOclVisitor::visit(DotProduct & dotProduct)
    {
        if (!isOutPutOfNodeValid(dotProduct))
        {
            return;
        }
        
        std::string const aExpr = resolveParameter(dotProduct.parameter()[FieldNames::A]);
        std::string const bExpr = resolveParameter(dotProduct.parameter()[FieldNames::B]);
        std::string const expression = fmt::format("dot({}, {})", aExpr, bExpr);
        
        bool const canInline = shouldInlineOutput(dotProduct, FieldNames::Result);
        
        if (canInline)
        {
            auto const key = std::make_pair(dotProduct.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       dotProduct.getResultOutputPort().getUniqueName(), expression);
        }
        else
        {
            m_definition << fmt::format("float const {} = {};\n",
                                        dotProduct.getResultOutputPort().getUniqueName(), expression);
        }
    }

    void ToOclVisitor::visit(CrossProduct & crossProduct)
    {
        if (!isOutPutOfNodeValid(crossProduct))
        {
            return;
        }
        
        std::string const aExpr = resolveParameter(crossProduct.parameter()[FieldNames::A]);
        std::string const bExpr = resolveParameter(crossProduct.parameter()[FieldNames::B]);
        std::string const expression = fmt::format("cross({}, {})", aExpr, bExpr);
        
        bool const canInline = shouldInlineOutput(crossProduct, FieldNames::Result);
        
        if (canInline)
        {
            auto const key = std::make_pair(crossProduct.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       crossProduct.getResultOutputPort().getUniqueName(), expression);
        }
        else
        {
            m_definition << fmt::format("float3 const {} = {};\n",
                                        crossProduct.getResultOutputPort().getUniqueName(), expression);
        }
    }

    void ToOclVisitor::visit(MatrixVectorMultiplication & matrixVectorMultiplication)
    {
        if (!isOutPutOfNodeValid(matrixVectorMultiplication))
        {
            return;
        }
        m_definition << fmt::format(
          "float3 const {0} = matrixVectorMul3f((float16)({1}), {2});\n",
          matrixVectorMultiplication.getResultOutputPort().getUniqueName(),
          matrixVectorMultiplication.parameter().at(FieldNames::A).toString(),
          matrixVectorMultiplication.parameter().at(FieldNames::B).toString());
    }

    void ToOclVisitor::visit(Transpose & transpose)
    {
        if (!isOutPutOfNodeValid(transpose))
        {
            return;
        }
        m_definition << fmt::format(
          fmt::runtime("float16 const {0} = transpose((float16)({1}));\n"),
          transpose.getOutputs()[FieldNames::Matrix].getUniqueName(),
          transpose.parameter()[FieldNames::Matrix].toString());
    }

    void ToOclVisitor::visit(Sine & sine)
    {
        emitUnaryOperation(sine, "sin", FieldNames::Result);
    }

    void ToOclVisitor::visit(Cosine & cosine)
    {
        emitUnaryOperation(cosine, "cos", FieldNames::Result);
    }

    void ToOclVisitor::visit(Tangent & tangent)
    {
        emitUnaryOperation(tangent, "tan", FieldNames::Result);
    }

    void ToOclVisitor::visit(ArcSin & arcSin)
    {
        emitUnaryOperation(arcSin, "asin", FieldNames::Result);
    }

    void ToOclVisitor::visit(ArcCos & arcCos)
    {
        emitUnaryOperation(arcCos, "acos", FieldNames::Result);
    }

    void ToOclVisitor::visit(ArcTan & arcTan)
    {
        emitUnaryOperation(arcTan, "atan", FieldNames::Result);
    }

    void ToOclVisitor::visit(Pow & power)
    {
        emitBinaryOperation(power, "pow", FieldNames::Value, FieldNames::Base, FieldNames::Exponent);
    }

    void ToOclVisitor::visit(Sqrt & sqrtNode)
    {
        emitUnaryOperation(sqrtNode, "sqrt", FieldNames::Result);
    }

    void ToOclVisitor::visit(Fmod & modulus)
    {
        emitBinaryOperation(modulus, "fmod", FieldNames::Result);
    }

    void ToOclVisitor::visit(Mod & modulus)
    {
        if (!isOutPutOfNodeValid(modulus))
        {
            return;
        }

        auto const numCopmonents = modulus.parameter().at(FieldNames::A).getSize();

        m_definition << fmt::format("{0} const {1} = glsl_mod{4}f(({0})({2}), ({0})({3}));\n",
                                    typeIndexToOpenCl(modulus.getResultOutputPort().getTypeIndex()),
                                    modulus.getResultOutputPort().getUniqueName(),
                                    modulus.parameter().at(FieldNames::A).toString(),
                                    modulus.parameter().at(FieldNames::B).toString(),
                                    numCopmonents);
    }

    void ToOclVisitor::visit(Max & maxNode)
    {
        emitBinaryOperation(maxNode, "max", FieldNames::Result);
    }

    void ToOclVisitor::visit(Min & minNode)
    {
        emitBinaryOperation(minNode, "min", FieldNames::Result);
    }

    void ToOclVisitor::visit(Abs & absNode)
    {
        emitUnaryOperation(absNode, "fabs", FieldNames::Result);
    }

    void ToOclVisitor::visit(Length & lengthNode)
    {
        if (!isOutPutOfNodeValid(lengthNode))
        {
            return;
        }
        
        std::string const inputExpr = resolveParameter(lengthNode.parameter().at(FieldNames::A));
        std::string const expression = fmt::format("length((float3)({}))", inputExpr);
        
        bool const canInline = shouldInlineOutput(lengthNode, FieldNames::Result);
        
        if (canInline)
        {
            auto const key = std::make_pair(lengthNode.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
            m_definition << fmt::format("// Inlined: {} {}\n", 
                                       lengthNode.getResultOutputPort().getUniqueName(), expression);
        }
        else
        {
            m_definition << fmt::format("float const {} = {};\n",
                                        lengthNode.getResultOutputPort().getUniqueName(), expression);
        }
    }

    void ToOclVisitor::visit(Mix & mixNode)
    {
        emitTernaryOperation(mixNode, "mix", FieldNames::Result, FieldNames::A, FieldNames::B, FieldNames::Ratio);
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
        emitUnaryOperation(exp, "exp", FieldNames::Result);
    }
    
    void ToOclVisitor::visit(Log & log)
    {
        emitUnaryOperation(log, "log", FieldNames::Result);
    }

    void ToOclVisitor::visit(Log2 & log2)
    {
        emitUnaryOperation(log2, "log2", FieldNames::Result);
    }

    void ToOclVisitor::visit(Log10 & log10)
    {
        emitUnaryOperation(log10, "log10", FieldNames::Result);
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
        emitTernaryOperation(clamp, "clamp", FieldNames::Result, FieldNames::A, FieldNames::Min, FieldNames::Max);
    }

    void ToOclVisitor::visit(SinH & sinh)
    {
        emitUnaryOperation(sinh, "sinh", FieldNames::Result);
    }

    void ToOclVisitor::visit(CosH & cosh)
    {
        emitUnaryOperation(cosh, "cosh", FieldNames::Result);
    }

    void ToOclVisitor::visit(TanH & tanh)
    {
        emitUnaryOperation(tanh, "tanh", FieldNames::Result);
    }

    void ToOclVisitor::visit(Round & round)
    {
        emitUnaryOperation(round, "round", FieldNames::Result);
    }

    void ToOclVisitor::visit(Ceil & ceil)
    {
        emitUnaryOperation(ceil, "ceil", FieldNames::Result);
    }

    void ToOclVisitor::visit(Floor & floor)
    {
        emitUnaryOperation(floor, "floor", FieldNames::Result);
    }

    void ToOclVisitor::visit(Sign & sign)
    {
        emitUnaryOperation(sign, "sign", FieldNames::Result);
    }

    void ToOclVisitor::visit(Fract & fract)
    {
        emitUnaryOperation(fract, "fract", FieldNames::Result);
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
