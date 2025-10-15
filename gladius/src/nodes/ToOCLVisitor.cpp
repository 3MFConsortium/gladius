#include "ToOCLVisitor.h"
#include "nodesfwd.h"
#include <array>
#include <fmt/format.h>
#include <iostream>
#include <string>
#include <unordered_map>

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
        return const_cast<IParameter &>(param).toString();
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

    void ToOclVisitor::emitUnaryOperation(NodeBase & node,
                                          std::string const & operation,
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
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format(
              "{} const {} = {};\n", typeName, outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::emitBinaryOperation(NodeBase & node,
                                           std::string const & operation,
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
        std::string const expression = fmt::format(
          "{}(({})({}) , ({})({}))", operation, typeName, param1Expr, typeName, param2Expr);

        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(node, outputPortName);

        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(node.getId(), outputPortName);
            m_inlineExpressions[key] = expression;
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format(
              "{} const {} = {};\n", typeName, outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::emitTernaryOperation(NodeBase & node,
                                            std::string const & operation,
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
        std::string const expression =
          fmt::format("{}({}, {}, {})", operation, param1Expr, param2Expr, param3Expr);

        // Check if this result should be inlined
        bool const canInline = shouldInlineOutput(node, outputPortName);

        if (canInline)
        {
            // Store the expression for inlining
            auto const key = std::make_pair(node.getId(), outputPortName);
            m_inlineExpressions[key] = expression;
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format(
              "{} const {} = {};\n", typeName, outputPort.getUniqueName(), expression);
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
                m_definition << fmt::format(
                  "return (float4)((float3)({0}),{1});\n}}\n",
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

        auto const & outputPort = composeVector.getOutputs().at(FieldNames::Result);
        std::string const xExpr = resolveParameter(composeVector.parameter().at(FieldNames::X));
        std::string const yExpr = resolveParameter(composeVector.parameter().at(FieldNames::Y));
        std::string const zExpr = resolveParameter(composeVector.parameter().at(FieldNames::Z));

        std::string const expression = fmt::format("(float3)({}, {}, {})", xExpr, yExpr, zExpr);

        bool const canInline = shouldInlineOutput(composeVector, FieldNames::Result);
        if (canInline)
        {
            auto const key = std::make_pair(composeVector.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
        }
        else
        {
            m_definition << fmt::format(
              "float3 const {} = {};\n", outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::visit(ComposeMatrix & composeMatrix)
    {
        if (!isOutPutOfNodeValid(composeMatrix))
        {
            return;
        }

        auto const & outputPort = composeMatrix.getOutputs().at(FieldNames::Result);
        auto const m00 = resolveParameter(composeMatrix.parameter().at(FieldNames::M00));
        auto const m01 = resolveParameter(composeMatrix.parameter().at(FieldNames::M01));
        auto const m02 = resolveParameter(composeMatrix.parameter().at(FieldNames::M02));
        auto const m03 = resolveParameter(composeMatrix.parameter().at(FieldNames::M03));
        auto const m10 = resolveParameter(composeMatrix.parameter().at(FieldNames::M10));
        auto const m11 = resolveParameter(composeMatrix.parameter().at(FieldNames::M11));
        auto const m12 = resolveParameter(composeMatrix.parameter().at(FieldNames::M12));
        auto const m13 = resolveParameter(composeMatrix.parameter().at(FieldNames::M13));
        auto const m20 = resolveParameter(composeMatrix.parameter().at(FieldNames::M20));
        auto const m21 = resolveParameter(composeMatrix.parameter().at(FieldNames::M21));
        auto const m22 = resolveParameter(composeMatrix.parameter().at(FieldNames::M22));
        auto const m23 = resolveParameter(composeMatrix.parameter().at(FieldNames::M23));
        auto const m30 = resolveParameter(composeMatrix.parameter().at(FieldNames::M30));
        auto const m31 = resolveParameter(composeMatrix.parameter().at(FieldNames::M31));
        auto const m32 = resolveParameter(composeMatrix.parameter().at(FieldNames::M32));
        auto const m33 = resolveParameter(composeMatrix.parameter().at(FieldNames::M33));

        std::string const expression =
          fmt::format("(float16)({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
                      m00,
                      m01,
                      m02,
                      m03,
                      m10,
                      m11,
                      m12,
                      m13,
                      m20,
                      m21,
                      m22,
                      m23,
                      m30,
                      m31,
                      m32,
                      m33);

        bool const canInline = shouldInlineOutput(composeMatrix, FieldNames::Result);
        if (canInline)
        {
            auto const key = std::make_pair(composeMatrix.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
        }
        else
        {
            m_definition << fmt::format(
              "float16 const {} = {};\n", outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::visit(ComposeMatrixFromColumns & composeMatrixFromColumns)
    {
        if (!isOutPutOfNodeValid(composeMatrixFromColumns))
        {
            return;
        }

        auto const & outputPort = composeMatrixFromColumns.getOutputs().at(FieldNames::Result);
        auto const col0 =
          resolveParameter(composeMatrixFromColumns.parameter().at(FieldNames::Col0));
        auto const col1 =
          resolveParameter(composeMatrixFromColumns.parameter().at(FieldNames::Col1));
        auto const col2 =
          resolveParameter(composeMatrixFromColumns.parameter().at(FieldNames::Col2));
        auto const col3 =
          resolveParameter(composeMatrixFromColumns.parameter().at(FieldNames::Col3));

        std::string const expression =
          fmt::format("(float16)(({0}).x, ({1}).x, ({2}).x, ({3}).x, ({0}).y, ({1}).y, ({2}).y, "
                      "({3}).y, ({0}).z, ({1}).z, ({2}).z, ({3}).z, 0.f, 0.f, 0.f, 1.f)",
                      col0,
                      col1,
                      col2,
                      col3);

        bool const canInline = shouldInlineOutput(composeMatrixFromColumns, FieldNames::Result);
        if (canInline)
        {
            auto const key =
              std::make_pair(composeMatrixFromColumns.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
        }
        else
        {
            m_definition << fmt::format(
              "float16 const {} = {};\n", outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::visit(ComposeMatrixFromRows & composeMatrixFromRows)
    {
        if (!isOutPutOfNodeValid(composeMatrixFromRows))
        {
            return;
        }

        auto const & outputPort = composeMatrixFromRows.getOutputs().at(FieldNames::Result);
        auto const row0 = resolveParameter(composeMatrixFromRows.parameter().at(FieldNames::Row0));
        auto const row1 = resolveParameter(composeMatrixFromRows.parameter().at(FieldNames::Row1));
        auto const row2 = resolveParameter(composeMatrixFromRows.parameter().at(FieldNames::Row2));
        auto const row3 = resolveParameter(composeMatrixFromRows.parameter().at(FieldNames::Row3));

        std::string const expression =
          fmt::format("(float16)(({0}).x, ({0}).y, ({0}).z, 0.f, ({1}).x, ({1}).y, ({1}).z, 0.f, "
                      "({2}).x, ({2}).y, ({2}).z, 0.f, ({3}).x, ({3}).y, ({3}).z, 1.f)",
                      row0,
                      row1,
                      row2,
                      row3);

        bool const canInline = shouldInlineOutput(composeMatrixFromRows, FieldNames::Result);
        if (canInline)
        {
            auto const key =
              std::make_pair(composeMatrixFromRows.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
        }
        else
        {
            m_definition << fmt::format(
              "float16 const {} = {};\n", outputPort.getUniqueName(), expression);
        }
    }

    void ToOclVisitor::visit(DecomposeVector & decomposeVector)
    {
        if (!isOutPutOfNodeValid(decomposeVector))
        {
            return;
        }

        auto const inputVec = resolveParameter(decomposeVector.parameter()[FieldNames::A]);
        m_definition << fmt::format("float const {0} = ((float3)({1})).x;\n",
                                    decomposeVector.getOutputs()[FieldNames::X].getUniqueName(),
                                    inputVec);

        m_definition << fmt::format("float const {0} = ((float3)({1})).y;\n",
                                    decomposeVector.getOutputs()[FieldNames::Y].getUniqueName(),
                                    inputVec);

        m_definition << fmt::format("float const {0} = ((float3)({1})).z;\n",
                                    decomposeVector.getOutputs()[FieldNames::Z].getUniqueName(),
                                    inputVec);
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
          resolveParameter(signedDistanceToMesh.parameter()[FieldNames::Pos]),
          resolveParameter(signedDistanceToMesh.parameter()[FieldNames::Start]),
          resolveParameter(signedDistanceToMesh.parameter()[FieldNames::End]));
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
          resolveParameter(signedDistanceToBeamLattice.parameter()[FieldNames::Pos]),
          resolveParameter(signedDistanceToBeamLattice.parameter()[FieldNames::Start]),
          resolveParameter(signedDistanceToBeamLattice.parameter()[FieldNames::End]));
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

    void ToOclVisitor::visit(FunctionGradient & functionGradient)
    {
        if (!isOutPutOfNodeValid(functionGradient))
        {
            return;
        }

        auto gradientOutputIter = functionGradient.getOutputs().find(FieldNames::Vector);
        if (gradientOutputIter == functionGradient.getOutputs().end())
        {
            return;
        }

        auto & gradientOutput = gradientOutputIter->second;
        if (!gradientOutput.isUsed())
        {
            return;
        }

        auto emitFallback = [&](std::string reason)
        {
            std::string const fallbackExpr = "(float3)(0.0f)";

            bool const canInline = shouldInlineOutput(functionGradient, FieldNames::Vector);
            auto const key =
              std::make_pair(functionGradient.getId(), std::string(FieldNames::Vector));

            if (canInline)
            {
                m_inlineExpressions[key] = fallbackExpr;
            }
            else
            {
                m_definition << fmt::format(
                  "float3 const {0} = {1};\n", gradientOutput.getUniqueName(), fallbackExpr);
            }
        };

        if (!functionGradient.hasValidConfiguration())
        {
            emitFallback("node not fully configured");
            return;
        }

        if (m_assembly == nullptr)
        {
            emitFallback("assembly is null");
            return;
        }

        functionGradient.resolveFunctionId();
        auto const functionId = functionGradient.getFunctionId();
        auto referencedModel = m_assembly->findModel(functionId);

        if (!referencedModel)
        {
            emitFallback(fmt::format("referenced model {} not found", functionId));
            return;
        }

        auto const & selectedOutputName = functionGradient.getSelectedScalarOutput();
        auto const & selectedVectorParamName = functionGradient.getSelectedVectorInput();

        auto & referencedOutputs = referencedModel->getOutputs();
        auto referencedOutputIter = referencedOutputs.find(selectedOutputName);
        if (referencedOutputIter == referencedOutputs.end())
        {
            emitFallback(fmt::format("missing output '{}'", selectedOutputName));
            return;
        }

        if (referencedOutputIter->second.getTypeIndex() != ParameterTypeIndex::Float)
        {
            emitFallback(fmt::format("output '{}' is not float", selectedOutputName));
            return;
        }

        if (!referencedOutputIter->second.isConsumedByFunction())
        {
            emitFallback(fmt::format("output '{}' is not marked as consumed", selectedOutputName));
            return;
        }

        auto * vectorParameter = functionGradient.getSelectedVectorParameter();
        if (vectorParameter == nullptr)
        {
            emitFallback(fmt::format("vector input '{}' not selected", selectedVectorParamName));
            return;
        }

        if (vectorParameter->getTypeIndex() != ParameterTypeIndex::Float3)
        {
            emitFallback(fmt::format("vector input '{}' is not float3", selectedVectorParamName));
            return;
        }

        auto stepIter = functionGradient.parameter().find(FieldNames::StepSize);
        if (stepIter == functionGradient.parameter().end())
        {
            emitFallback("step size parameter missing");
            return;
        }

        auto const functionName = referencedModel->getModelName();
        std::string const nodeIdStr = std::to_string(functionGradient.getId());

        std::string const stepVarName = fmt::format("FG_step_{}", nodeIdStr);
        std::string const rawStepExpr = resolveParameter(stepIter->second);
        m_definition << fmt::format(
          "float const {0} = fmax(fabs({1}), 1e-8f);\n", stepVarName, rawStepExpr);

        std::string const baseVectorVar = fmt::format("FG_input_{}", nodeIdStr);
        std::string const baseVectorExpr = resolveParameter(*vectorParameter);
        m_definition << fmt::format(
          "float3 const {0} = (float3)({1});\n", baseVectorVar, baseVectorExpr);

        auto & parameters = functionGradient.parameter();

        auto emitEvaluation = [&](std::string const & callTag,
                                  std::string const & vectorExpression) -> std::string
        {
            std::stringstream args;
            bool first = true;

            for (auto const & [paramName, parameter] : parameters)
            {
                if (!parameter.isArgument())
                {
                    continue;
                }

                std::string argumentExpr;
                if (paramName == selectedVectorParamName)
                {
                    argumentExpr = vectorExpression;
                }
                else
                {
                    argumentExpr = resolveParameter(parameter);
                }

                if (argumentExpr.empty())
                {
                    continue;
                }

                if (!first)
                {
                    args << ", ";
                }

                args << fmt::format(
                  "({0})({1})", typeIndexToOpenCl(parameter.getTypeIndex()), argumentExpr);
                first = false;
            }

            std::unordered_map<std::string, std::string> localOutputs;

            for (auto & [outputName, outputPort] : referencedOutputs)
            {
                if (!outputPort.isConsumedByFunction())
                {
                    continue;
                }

                std::string const localVarName =
                  fmt::format("{0}_{1}_{2}", callTag, outputName, nodeIdStr);
                m_definition << fmt::format("{0} {1} = ({0})(0.f);\n",
                                            typeIndexToOpenCl(outputPort.getTypeIndex()),
                                            localVarName);

                if (!first)
                {
                    args << ", ";
                }

                args << fmt::format("&{0}", localVarName);
                first = false;

                localOutputs.emplace(outputName, localVarName);
            }

            auto selectedOutputVar = localOutputs.find(selectedOutputName);
            if (selectedOutputVar == localOutputs.end())
            {
                throw std::runtime_error(fmt::format(
                  "FunctionGradient node {0}: referenced output '{1}' is not marked as consumed",
                  functionGradient.getUniqueName(),
                  selectedOutputName));
            }

            if (first)
            {
                m_definition << fmt::format("{0}(PASS_PAYLOAD_ARGS);\n", functionName);
            }
            else
            {
                m_definition << fmt::format(
                  "{0}({1}, PASS_PAYLOAD_ARGS);\n", functionName, args.str());
            }

            return selectedOutputVar->second;
        };

        std::array<char, 3> const axisComponents{'x', 'y', 'z'};
        std::unordered_map<std::string, std::string> positiveSamples;
        std::unordered_map<std::string, std::string> negativeSamples;

        for (char component : axisComponents)
        {
            std::string const componentStr(1, component);

            std::string const posVectorVar =
              fmt::format("{0}_pos_{1}", baseVectorVar, componentStr);
            m_definition << fmt::format("float3 {0} = {1};\n", posVectorVar, baseVectorVar);
            m_definition << fmt::format("{0}.{1} += {2};\n", posVectorVar, component, stepVarName);

            std::string const posCallTag = fmt::format("FG_pos_{0}_{1}", nodeIdStr, componentStr);
            std::string const posSample = emitEvaluation(posCallTag, posVectorVar);
            positiveSamples.emplace(componentStr, posSample);

            std::string const negVectorVar =
              fmt::format("{0}_neg_{1}", baseVectorVar, componentStr);
            m_definition << fmt::format("float3 {0} = {1};\n", negVectorVar, baseVectorVar);
            m_definition << fmt::format("{0}.{1} -= {2};\n", negVectorVar, component, stepVarName);

            std::string const negCallTag = fmt::format("FG_neg_{0}_{1}", nodeIdStr, componentStr);
            std::string const negSample = emitEvaluation(negCallTag, negVectorVar);
            negativeSamples.emplace(componentStr, negSample);
        }

        auto const gradientVarName = fmt::format("FG_gradient_{}", nodeIdStr);
        m_definition << fmt::format(
          "float3 const {0} = (float3)(({1} - {2}) / (2.0f * {3}), ({4} - {5}) / (2.0f * {3}),"
          " ({6} - {7}) / (2.0f * {3}));\n",
          gradientVarName,
          positiveSamples.at("x"),
          negativeSamples.at("x"),
          stepVarName,
          positiveSamples.at("y"),
          negativeSamples.at("y"),
          positiveSamples.at("z"),
          negativeSamples.at("z"));

        auto const gradientLenVarName = fmt::format("FG_gradient_len_{}", nodeIdStr);
        m_definition << fmt::format(
          "float const {0} = length({1});\n", gradientLenVarName, gradientVarName);

        auto const normalizedVarName = fmt::format("FG_gradient_norm_{}", nodeIdStr);
        m_definition << fmt::format(
          "float3 const {0} = ({1} > 1e-8f) ? ({2} / {1}) : (float3)(0.0f);\n",
          normalizedVarName,
          gradientLenVarName,
          gradientVarName);

        bool const canInline = shouldInlineOutput(functionGradient, FieldNames::Vector);

        if (canInline)
        {
            auto const key =
              std::make_pair(functionGradient.getId(), std::string(FieldNames::Vector));
            m_inlineExpressions[key] = normalizedVarName;
        }
        else
        {
            m_definition << fmt::format(
              "float3 const {0} = {1};\n", gradientOutput.getUniqueName(), normalizedVarName);
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
        }
        else
        {
            // Emit a variable declaration
            m_definition << fmt::format("{} const {} = {};\n",
                                        typeName,
                                        addition.getResultOutputPort().getUniqueName(),
                                        expression);
        }
    }
    void ToOclVisitor::visit(Subtraction & subtraction)
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
            auto const key =
              std::make_pair(multiplication.getId(), std::string(FieldNames::Result));
            m_inlineExpressions[key] = expression;
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
        }
        else
        {
            m_definition << fmt::format("float const {} = {};\n",
                                        dotProduct.getResultOutputPort().getUniqueName(),
                                        expression);
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
        }
        else
        {
            m_definition << fmt::format("float3 const {} = {};\n",
                                        crossProduct.getResultOutputPort().getUniqueName(),
                                        expression);
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
          resolveParameter(matrixVectorMultiplication.parameter().at(FieldNames::A)),
          resolveParameter(matrixVectorMultiplication.parameter().at(FieldNames::B)));
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
          resolveParameter(transpose.parameter()[FieldNames::Matrix]));
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
        emitBinaryOperation(
          power, "pow", FieldNames::Value, FieldNames::Base, FieldNames::Exponent);
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
                                    resolveParameter(modulus.parameter().at(FieldNames::A)),
                                    resolveParameter(modulus.parameter().at(FieldNames::B)),
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
        }
        else
        {
            m_definition << fmt::format("float const {} = {};\n",
                                        lengthNode.getResultOutputPort().getUniqueName(),
                                        expression);
        }
    }

    void ToOclVisitor::visit(Mix & mixNode)
    {
        emitTernaryOperation(
          mixNode, "mix", FieldNames::Result, FieldNames::A, FieldNames::B, FieldNames::Ratio);
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
          resolveParameter(transformation.parameter()[FieldNames::Transformation]),
          resolveParameter(transformation.parameter()[FieldNames::Pos]));
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

        m_definition << fmt::format(
          "float4 const {0}_rgba = {1}((float3)({2}), (float3)({3}), "
          "{4}, {5}_tileStyle, PASS_PAYLOAD_ARGS);\n",
          imageSampler.getOutputs().at(FieldNames::Color).getUniqueName(),
          samplerName,
          resolveParameter(imageSampler.parameter().at(FieldNames::UVW)),
          resolveParameter(imageSampler.parameter().at(FieldNames::Dimensions)),
          resolveParameter(imageSampler.parameter().at(FieldNames::Start)),
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

        auto const matrixParam =
          resolveParameter(decomposeMatrixNode.parameter()[FieldNames::Matrix]);
        m_definition << fmt::format(
          "float const {0} = {1}.s0;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M00].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s1;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M01].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s2;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M02].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s3;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M03].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s4;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M10].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s5;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M11].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s6;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M12].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s7;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M13].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s8;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M20].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.s9;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M21].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.sa;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M22].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.sb;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M23].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.sc;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M30].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.sd;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M31].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.se;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M32].getUniqueName(),
          matrixParam);

        m_definition << fmt::format(
          "float const {0} = {1}.sf;\n",
          decomposeMatrixNode.getOutputs()[FieldNames::M33].getUniqueName(),
          matrixParam);
    }

    void ToOclVisitor::visit(Inverse & inverse)
    {
        if (!isOutPutOfNodeValid(inverse))
        {
            return;
        }

        m_definition << fmt::format("float16 const {0} = inverse((float16)({1}));\n",
                                    inverse.getOutputs()[FieldNames::Result].getUniqueName(),
                                    resolveParameter(inverse.parameter()[FieldNames::Matrix]));
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
          resolveParameter(arcTan2.parameter().at(FieldNames::A)),
          resolveParameter(arcTan2.parameter().at(FieldNames::B)));
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
          resolveParameter(select.parameter().at(FieldNames::A)),
          resolveParameter(select.parameter().at(FieldNames::B)),
          resolveParameter(select.parameter().at(FieldNames::C)),
          resolveParameter(select.parameter().at(FieldNames::D)));
    }

    void ToOclVisitor::visit(Clamp & clamp)
    {
        emitTernaryOperation(
          clamp, "clamp", FieldNames::Result, FieldNames::A, FieldNames::Min, FieldNames::Max);
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
          resolveParameter(vectorFromScalar.parameter().at(FieldNames::A)));
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
          resolveParameter(unsignedDistanceToMesh.parameter()[FieldNames::Pos]),
          resolveParameter(unsignedDistanceToMesh.parameter()[FieldNames::Start]),
          resolveParameter(unsignedDistanceToMesh.parameter()[FieldNames::End]));
    }

    void ToOclVisitor::visit(BoxMinMax & boxMinMax)
    {
        if (!isOutPutOfNodeValid(boxMinMax))
        {
            return;
        }

        m_definition << fmt::format("float const {0} = bbBox({1}, {2}, {3});\n",
                                    boxMinMax.getOutputs().at(FieldNames::Shape).getUniqueName(),
                                    resolveParameter(boxMinMax.parameter().at(FieldNames::Pos)),
                                    resolveParameter(boxMinMax.parameter().at(FieldNames::Min)),
                                    resolveParameter(boxMinMax.parameter().at(FieldNames::Max)));
    }
} // namespace gladius::nodes
