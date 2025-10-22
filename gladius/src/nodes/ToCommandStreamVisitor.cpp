#include "ToCommandStreamVisitor.h"

#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "Assembly.h"
#include "Commands.h"
#include "Model.h"
#include "Parameter.h"
#include "kernel/types.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    ToCommandStreamVisitor::ToCommandStreamVisitor(CommandBuffer * target, Assembly * assembly)
        : m_assembly(assembly)
        , m_cmds(target)
    {
    }

    void ToCommandStreamVisitor::setModel(Model * const model)
    {
        m_visitedNodes.clear();
        m_currentModel = model;
        m_modelId = model->getResourceId();
    }

    void ToCommandStreamVisitor::setAssembly(Assembly * assembly)
    {
        m_assembly = assembly;
    }

    int roundUp(int value, int stepSize)
    {
        const auto remainder = value % stepSize;
        if (remainder == 0)
        {
            return value;
        }
        return value + stepSize - remainder;
    }

    std::string cmdIdToString(CommandType cmdType)
    {
        switch (cmdType)
        {
        case CT_CONSTANT_SCALAR:
            return "CT_CONSTANT_SCALAR";
        case CT_CONSTANT_VECTOR:
            return "CT_CONSTANT_VECTOR";
        case CT_CONSTANT_MATRIX:
            return "CT_CONSTANT_MATRIX";
        case CT_SIGNED_DISTANCE_TO_MESH:
            return "CT_SIGNED_DISTANCE_TO_MESH";
        case CT_ADDITION_SCALAR:
            return "CT_ADDITION_SCALAR";
        case CT_ADDITION_VECTOR:
            return "CT_ADDITION_VECTOR";
        case CT_ADDITION_MATRIX:
            return "CT_ADDITION_MATRIX";
        case CT_SUBTRACTION_SCALAR:
            return "CT_SUBTRACTION_SCALAR";
        case CT_SUBTRACTION_VECTOR:
            return "CT_SUBTRACTION_VECTOR";
        case CT_SUBTRACTION_MATRIX:
            return "CT_SUBTRACTION_MATRIX";
        case CT_MULTIPLICATION_SCALAR:
            return "CT_MULTIPLICATION_SCALAR";
        case CT_MULTIPLICATION_VECTOR:
            return "CT_MULTIPLICATION_VECTOR";
        case CT_MULTIPLICATION_MATRIX:
            return "CT_MULTIPLICATION_MATRIX";
        case CT_TRANSFORMATION:
            return "CT_TRANSFORMATION";
        case CT_SINE_SCALAR:
            return "CT_SINE_SCALAR";
        case CT_SINE_VECTOR:
            return "CT_SINE_VECTOR";
        case CT_SINE_MATRIX:
            return "CT_SINE_MATRIX";
        case CT_COSINE_SCALAR:
            return "CT_COSINE_SCALAR";
        case CT_COSINE_VECTOR:
            return "CT_COSINE_VECTOR";
        case CT_COSINE_MATRIX:
            return "CT_COSINE_MATRIX";
        case CT_TANGENT_SCALAR:
            return "CT_TANGENT_SCALAR";
        case CT_TANGENT_VECTOR:
            return "CT_TANGENT_VECTOR";
        case CT_TANGENT_MATRIX:
            return "CT_TANGENT_MATRIX";
        case CT_ARC_SIN_SCALAR:
            return "CT_ARC_SIN_SCALAR";
        case CT_ARC_SIN_VECTOR:
            return "CT_ARC_SIN_VECTOR";
        case CT_ARC_SIN_MATRIX:
            return "CT_ARC_SIN_MATRIX";
        case CT_ARC_COS_SCALAR:
            return "CT_ARC_COS_SCALAR";
        case CT_ARC_COS_VECTOR:
            return "CT_ARC_COS_VECTOR";
        case CT_ARC_COS_MATRIX:
            return "CT_ARC_COS_MATRIX";
        case CT_ARC_TAN_SCALAR:
            return "CT_ARC_TAN_SCALAR";
        case CT_ARC_TAN_VECTOR:
            return "CT_ARC_TAN_VECTOR";
        case CT_ARC_TAN_MATRIX:
            return "CT_ARC_TAN_MATRIX";
        case CT_ARC_TAN2_SCALAR:
            return "CT_ARC_TAN2_SCALAR";
        case CT_ARC_TAN2_VECTOR:
            return "CT_ARC_TAN2_VECTOR";
        case CT_ARC_TAN2_MATRIX:
            return "CT_ARC_TAN2_MATRIX";
        case CT_POW_SCALAR:
            return "CT_POW_SCALAR";
        case CT_POW_VECTOR:
            return "CT_POW_VECTOR";
        case CT_POW_MATRIX:
            return "CT_POW_MATRIX";
        case CT_SQRT_SCALAR:
            return "CT_SQRT_SCALAR";
        case CT_SQRT_VECTOR:
            return "CT_SQRT_VECTOR";
        case CT_SQRT_MATRIX:
            return "CT_SQRT_MATRIX";
        case CT_FMOD_SCALAR:
            return "CT_FMOD_SCALAR";
        case CT_FMOD_VECTOR:
            return "CT_FMOD_VECTOR";
        case CT_FMOD_MATRIX:
            return "CT_MOD_MATRIX";
        case CT_MOD_SCALAR:
            return "CT_MOD_SCALAR";
        case CT_MOD_VECTOR:
            return "CT_MOD_VECTOR";
        case CT_MOD_MATRIX:
            return "CT_MOD_MATRIX";
        case CT_MAX_SCALAR:
            return "CT_MAX_SCALAR";
        case CT_MAX_VECTOR:
            return "CT_MAX_VECTOR";
        case CT_MAX_MATRIX:
            return "CT_MAX_MATRIX";
        case CT_MIN_SCALAR:
            return "CT_MIN_SCALAR";
        case CT_MIN_VECTOR:
            return "CT_MIN_VECTOR";
        case CT_MIN_MATRIX:
            return "CT_MIN_MATRIX";
        case CT_ABS_SCALAR:
            return "CT_ABS_SCALAR";
        case CT_ABS_VECTOR:
            return "CT_ABS_VECTOR";
        case CT_ABS_MATRIX:
            return "CT_ABS_MATRIX";
        case CT_DOT_PRODUCT:
            return "CT_DOT_PRODUCT";
        case CT_LENGTH:
            return "CT_LENGTH";
        case CT_RESOURCE:
            return "CT_RESOURCE";
        case CT_DECOMPOSE_VECTOR:
            return "CT_DECOMPOSE_VECTOR";
        case CT_COMPOSE_VECTOR:
            return "CT_COMPOSE_VECTOR";
        case CT_END:
            return "CT_END";
        default:
            return "Unknown command type";
        }
    }

    void ToCommandStreamVisitor::write(std::ostream & out) const
    {
        if (m_assembly->assemblyModel()->getBeginNode() == nullptr)
        {
            return;
        }

        out << "\n#define GETPARAM(index, offset) \
    (cmds[i].args[index] < 0) ? out[-cmds[i].args[index]+offset] : parameter[cmds[i].args[index]+offset]\n";

        out << "#define GETPARAM2(arg) \
    (arg < 0) ? out[-arg] : parameter[arg]\n";

        out << m_signature.str();
        out << fmt::format("float out[{}];\n", roundUp(m_currentOutPutIndex + 1, 1024));
        out << fmt::format("out[1]={}.x;\n",
                           getAssemblyBegin().getOutputs().at(FieldNames::Pos).getUniqueName());
        out << fmt::format("out[2]={}.y;\n",
                           getAssemblyBegin().getOutputs().at(FieldNames::Pos).getUniqueName());
        out << fmt::format("out[3]={}.z;\n",
                           getAssemblyBegin().getOutputs().at(FieldNames::Pos).getUniqueName());

        out << "for (int i = 0; i < sizeOfCmds; ++i)\n";
        out << "{\n";

        out << "if (cmds[i].type == CT_CONSTANT_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CONSTANT_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[0]+1] = GETPARAM(1,0);\n";
        out << " out[cmds[i].output[0]+2] = GETPARAM(2,0);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CONSTANT_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[0]+1] = GETPARAM(0,1);\n";
        out << " out[cmds[i].output[0]+2] = GETPARAM(0,2);\n";
        out << " out[cmds[i].output[0]+3] = GETPARAM(0,3);\n";
        out << " out[cmds[i].output[0]+4] = GETPARAM(0,4);\n";
        out << " out[cmds[i].output[0]+5] = GETPARAM(0,5);\n";
        out << " out[cmds[i].output[0]+6] = GETPARAM(0,6);\n";
        out << " out[cmds[i].output[0]+7] = GETPARAM(0,7);\n";
        out << " out[cmds[i].output[0]+8] = GETPARAM(0,8);\n";
        out << " out[cmds[i].output[0]+9] = GETPARAM(0,9);\n";
        out << " out[cmds[i].output[0]+10] = GETPARAM(0,10);\n";
        out << " out[cmds[i].output[0]+11] = GETPARAM(0,11);\n";
        out << " out[cmds[i].output[0]+12] = GETPARAM(0,12);\n";
        out << " out[cmds[i].output[0]+13] = GETPARAM(0,13);\n";
        out << " out[cmds[i].output[0]+14] = GETPARAM(0,14);\n";
        out << " out[cmds[i].output[0]+15] = GETPARAM(0,15);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SIGNED_DISTANCE_TO_MESH)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = "
               "payload((float3)(GETPARAM(0,0),GETPARAM(0,1),GETPARAM(0,2)), "
               "(int)(GETPARAM(1,0)),(int)(GETPARAM(2,0)),"
               "PASS_PAYLOAD_ARGS);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_UNSIGNED_DISTANCE_TO_MESH)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = "
               "fabs(payload((float3)(GETPARAM(0,0),GETPARAM(0,1),GETPARAM(0,2)), "
               "(int)(GETPARAM(1,0)),(int)(GETPARAM(2,0)),"
               "PASS_PAYLOAD_ARGS));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ADDITION_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))+(GETPARAM(1,0)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ADDITION_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))+(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))+(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))+(GETPARAM(1,2)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ADDITION_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))+(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))+(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))+(GETPARAM(1,2)));\n";
        out << " out[cmds[i].output[0]+3] = ((GETPARAM(0,3))+(GETPARAM(1,3)));\n";
        out << " out[cmds[i].output[0]+4] = ((GETPARAM(0,4))+(GETPARAM(1,4)));\n";
        out << " out[cmds[i].output[0]+5] = ((GETPARAM(0,5))+(GETPARAM(1,5)));\n";
        out << " out[cmds[i].output[0]+6] = ((GETPARAM(0,6))+(GETPARAM(1,6)));\n";
        out << " out[cmds[i].output[0]+7] = ((GETPARAM(0,7))+(GETPARAM(1,7)));\n";
        out << " out[cmds[i].output[0]+8] = ((GETPARAM(0,8))+(GETPARAM(1,8)));\n";
        out << " out[cmds[i].output[0]+9] = ((GETPARAM(0,9))+(GETPARAM(1,9)));\n";
        out << " out[cmds[i].output[0]+10] = ((GETPARAM(0,10))+(GETPARAM(1,10)));\n";
        out << " out[cmds[i].output[0]+11] = ((GETPARAM(0,11))+(GETPARAM(1,11)));\n";
        out << " out[cmds[i].output[0]+12] = ((GETPARAM(0,12))+(GETPARAM(1,12)));\n";
        out << " out[cmds[i].output[0]+13] = ((GETPARAM(0,13))+(GETPARAM(1,13)));\n";
        out << " out[cmds[i].output[0]+14] = ((GETPARAM(0,14))+(GETPARAM(1,14)));\n";
        out << " out[cmds[i].output[0]+15] = ((GETPARAM(0,15))+(GETPARAM(1,15)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SUBTRACTION_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))-(GETPARAM(1,0)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SUBTRACTION_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))-(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))-(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))-(GETPARAM(1,2)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SUBTRACTION_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))-(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))-(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))-(GETPARAM(1,2)));\n";
        out << " out[cmds[i].output[0]+3] = ((GETPARAM(0,3))-(GETPARAM(1,3)));\n";
        out << " out[cmds[i].output[0]+4] = ((GETPARAM(0,4))-(GETPARAM(1,4)));\n";
        out << " out[cmds[i].output[0]+5] = ((GETPARAM(0,5))-(GETPARAM(1,5)));\n";
        out << " out[cmds[i].output[0]+6] = ((GETPARAM(0,6))-(GETPARAM(1,6)));\n";
        out << " out[cmds[i].output[0]+7] = ((GETPARAM(0,7))-(GETPARAM(1,7)));\n";
        out << " out[cmds[i].output[0]+8] = ((GETPARAM(0,8))-(GETPARAM(1,8)));\n";
        out << " out[cmds[i].output[0]+9] = ((GETPARAM(0,9))-(GETPARAM(1,9)));\n";
        out << " out[cmds[i].output[0]+10] = ((GETPARAM(0,10))-(GETPARAM(1,10)));\n";
        out << " out[cmds[i].output[0]+11] = ((GETPARAM(0,11))-(GETPARAM(1,11)));\n";
        out << " out[cmds[i].output[0]+12] = ((GETPARAM(0,12))-(GETPARAM(1,12)));\n";
        out << " out[cmds[i].output[0]+13] = ((GETPARAM(0,13))-(GETPARAM(1,13)));\n";
        out << " out[cmds[i].output[0]+14] = ((GETPARAM(0,14))-(GETPARAM(1,14)));\n";
        out << " out[cmds[i].output[0]+15] = ((GETPARAM(0,15))-(GETPARAM(1,15)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MULTIPLICATION_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))*(GETPARAM(1,0)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MULTIPLICATION_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))*(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))*(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))*(GETPARAM(1,2)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MULTIPLICATION_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))*(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))*(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))*(GETPARAM(1,2)));\n";
        out << " out[cmds[i].output[0]+3] = ((GETPARAM(0,3))*(GETPARAM(1,3)));\n";
        out << " out[cmds[i].output[0]+4] = ((GETPARAM(0,4))*(GETPARAM(1,4)));\n";
        out << " out[cmds[i].output[0]+5] = ((GETPARAM(0,5))*(GETPARAM(1,5)));\n";
        out << " out[cmds[i].output[0]+6] = ((GETPARAM(0,6))*(GETPARAM(1,6)));\n";
        out << " out[cmds[i].output[0]+7] = ((GETPARAM(0,7))*(GETPARAM(1,7)));\n";
        out << " out[cmds[i].output[0]+8] = ((GETPARAM(0,8))*(GETPARAM(1,8)));\n";
        out << " out[cmds[i].output[0]+9] = ((GETPARAM(0,9))*(GETPARAM(1,9)));\n";
        out << " out[cmds[i].output[0]+10] = ((GETPARAM(0,10))*(GETPARAM(1,10)));\n";
        out << " out[cmds[i].output[0]+11] = ((GETPARAM(0,11))*(GETPARAM(1,11)));\n";
        out << " out[cmds[i].output[0]+12] = ((GETPARAM(0,12))*(GETPARAM(1,12)));\n";
        out << " out[cmds[i].output[0]+13] = ((GETPARAM(0,13))*(GETPARAM(1,13)));\n";
        out << " out[cmds[i].output[0]+14] = ((GETPARAM(0,14))*(GETPARAM(1,14)));\n";
        out << " out[cmds[i].output[0]+15] = ((GETPARAM(0,15))*(GETPARAM(1,15)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_DIVISION_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))/(GETPARAM(1,0)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_DIVISION_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))/(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))/(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))/(GETPARAM(1,2)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_DIVISION_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ((GETPARAM(0,0))/(GETPARAM(1,0)));\n";
        out << " out[cmds[i].output[0]+1] = ((GETPARAM(0,1))/(GETPARAM(1,1)));\n";
        out << " out[cmds[i].output[0]+2] = ((GETPARAM(0,2))/(GETPARAM(1,2)));\n";
        out << " out[cmds[i].output[0]+3] = ((GETPARAM(0,3))/(GETPARAM(1,3)));\n";
        out << " out[cmds[i].output[0]+4] = ((GETPARAM(0,4))/(GETPARAM(1,4)));\n";
        out << " out[cmds[i].output[0]+5] = ((GETPARAM(0,5))/(GETPARAM(1,5)));\n";
        out << " out[cmds[i].output[0]+6] = ((GETPARAM(0,6))/(GETPARAM(1,6)));\n";
        out << " out[cmds[i].output[0]+7] = ((GETPARAM(0,7))/(GETPARAM(1,7)));\n";
        out << " out[cmds[i].output[0]+8] = ((GETPARAM(0,8))/(GETPARAM(1,8)));\n";
        out << " out[cmds[i].output[0]+9] = ((GETPARAM(0,9))/(GETPARAM(1,9)));\n";
        out << " out[cmds[i].output[0]+10] = ((GETPARAM(0,10))/(GETPARAM(1,10)));\n";
        out << " out[cmds[i].output[0]+11] = ((GETPARAM(0,11))/(GETPARAM(1,11)));\n";
        out << " out[cmds[i].output[0]+12] = ((GETPARAM(0,12))/(GETPARAM(1,12)));\n";
        out << " out[cmds[i].output[0]+13] = ((GETPARAM(0,13))/(GETPARAM(1,13)));\n";
        out << " out[cmds[i].output[0]+14] = ((GETPARAM(0,14))/(GETPARAM(1,14)));\n";
        out << " out[cmds[i].output[0]+15] = ((GETPARAM(0,15))/(GETPARAM(1,15)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TRANSFORMATION)\n";
        out << "{\n";
        out << " float3 const newPos = matrixVectorMul3f("
               "(float16)(";
        for (int i = 0; i < 15; ++i)
        {
            out << fmt::format("GETPARAM(1,{}),", i);
        }
        out << fmt::format("GETPARAM(1,15)");
        out << "),(float3) (GETPARAM(0,0), GETPARAM(0,1),GETPARAM(0,2)));\n";
        out << " out[cmds[i].output[0]] = newPos.x;\n";
        out << " out[cmds[i].output[0]+1] = newPos.y;\n";
        out << " out[cmds[i].output[0]+2] = newPos.z;\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SINE_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sin(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SINE_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sin(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sin(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sin(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SINE_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sin(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sin(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sin(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = sin(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = sin(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = sin(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = sin(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = sin(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = sin(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = sin(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = sin(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = sin(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = sin(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = sin(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = sin(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = sin(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COSINE_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = cos(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COSINE_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = cos(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = cos(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = cos(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COSINE_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = cos(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = cos(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = cos(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = cos(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = cos(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = cos(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = cos(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = cos(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = cos(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = cos(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = cos(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = cos(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = cos(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = cos(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = cos(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = cos(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TANGENT_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = tan(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TANGENT_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = tan(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = tan(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = tan(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TANGENT_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = tan(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = tan(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = tan(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = tan(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = tan(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = tan(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = tan(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = tan(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = tan(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = tan(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = tan(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = tan(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = tan(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = tan(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = tan(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = tan(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_SIN_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = asin(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_SIN_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = asin(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = asin(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = asin(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_SIN_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = asin(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = asin(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = asin(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = asin(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = asin(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = asin(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = asin(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = asin(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = asin(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = asin(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = asin(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = asin(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = asin(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = asin(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = asin(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = asin(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_COS_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = acos(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_COS_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = acos(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = acos(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = acos(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_COS_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = acos(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = acos(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = acos(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = acos(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = acos(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = acos(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = acos(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = acos(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = acos(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = acos(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = acos(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = acos(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = acos(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = acos(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = acos(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = acos(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_TAN_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = atan(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_TAN_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = atan(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = atan(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = atan(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_TAN_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = atan(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = atan(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = atan(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = atan(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = atan(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = atan(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = atan(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = atan(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = atan(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = atan(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = atan(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = atan(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = atan(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = atan(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = atan(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = atan(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_TAN2_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = atan2(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_TAN2_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = atan2(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = atan2(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = atan2(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ARC_TAN2_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = atan2(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = atan2(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = atan2(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << " out[cmds[i].output[0]+3] = atan2(GETPARAM(0,3), GETPARAM(1,3));\n";
        out << " out[cmds[i].output[0]+4] = atan2(GETPARAM(0,4), GETPARAM(1,4));\n";
        out << " out[cmds[i].output[0]+5] = atan2(GETPARAM(0,5), GETPARAM(1,5));\n";
        out << " out[cmds[i].output[0]+6] = atan2(GETPARAM(0,6), GETPARAM(1,6));\n";
        out << " out[cmds[i].output[0]+7] = atan2(GETPARAM(0,7), GETPARAM(1,7));\n";
        out << " out[cmds[i].output[0]+8] = atan2(GETPARAM(0,8), GETPARAM(1,8));\n";
        out << " out[cmds[i].output[0]+9] = atan2(GETPARAM(0,9), GETPARAM(1,9));\n";
        out << " out[cmds[i].output[0]+10] = atan2(GETPARAM(0,10), GETPARAM(1,10));\n";
        out << " out[cmds[i].output[0]+11] = atan2(GETPARAM(0,11), GETPARAM(1,11));\n";
        out << " out[cmds[i].output[0]+12] = atan2(GETPARAM(0,12), GETPARAM(1,12));\n";
        out << " out[cmds[i].output[0]+13] = atan2(GETPARAM(0,13), GETPARAM(1,13));\n";
        out << " out[cmds[i].output[0]+14] = atan2(GETPARAM(0,14), GETPARAM(1,14));\n";
        out << " out[cmds[i].output[0]+15] = atan2(GETPARAM(0,15), GETPARAM(1,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_POW_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = pow(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_POW_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = pow(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = pow(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = pow(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_POW_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = pow(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = pow(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = pow(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << " out[cmds[i].output[0]+3] = pow(GETPARAM(0,3), GETPARAM(1,3));\n";
        out << " out[cmds[i].output[0]+4] = pow(GETPARAM(0,4), GETPARAM(1,4));\n";
        out << " out[cmds[i].output[0]+5] = pow(GETPARAM(0,5), GETPARAM(1,5));\n";
        out << " out[cmds[i].output[0]+6] = pow(GETPARAM(0,6), GETPARAM(1,6));\n";
        out << " out[cmds[i].output[0]+7] = pow(GETPARAM(0,7), GETPARAM(1,7));\n";
        out << " out[cmds[i].output[0]+8] = pow(GETPARAM(0,8), GETPARAM(1,8));\n";
        out << " out[cmds[i].output[0]+9] = pow(GETPARAM(0,9), GETPARAM(1,9));\n";
        out << " out[cmds[i].output[0]+10] = pow(GETPARAM(0,10), GETPARAM(1,10));\n";
        out << " out[cmds[i].output[0]+11] = pow(GETPARAM(0,11), GETPARAM(1,11));\n";
        out << " out[cmds[i].output[0]+12] = pow(GETPARAM(0,12), GETPARAM(1,12));\n";
        out << " out[cmds[i].output[0]+13] = pow(GETPARAM(0,13), GETPARAM(1,13));\n";
        out << " out[cmds[i].output[0]+14] = pow(GETPARAM(0,14), GETPARAM(1,14));\n";
        out << " out[cmds[i].output[0]+15] = pow(GETPARAM(0,15), GETPARAM(1,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_EXP_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = exp(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_EXP_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = exp(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = exp(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = exp(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_EXP_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = exp(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = exp(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = exp(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = exp(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = exp(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = exp(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = exp(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = exp(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = exp(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = exp(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = exp(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = exp(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = exp(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = exp(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = exp(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = exp(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = log(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = log(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = log(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = log(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = log(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = log(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = log(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = log(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = log(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = log(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = log(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = log(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = log(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = log(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = log(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = log(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = log(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG2_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log2(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG2_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log2(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = log2(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = log2(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG2_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log2(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = log2(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = log2(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = log2(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = log2(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = log2(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = log2(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = log2(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = log2(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = log2(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = log2(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = log2(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = log2(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = log2(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = log2(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = log2(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG10_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log10(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG10_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log10(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = log10(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = log10(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LOG10_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = log10(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = log10(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = log10(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = log10(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = log10(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = log10(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = log10(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = log10(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = log10(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = log10(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = log10(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = log10(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = log10(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = log10(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = log10(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = log10(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_EXP_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = exp(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_EXP_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = exp(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = exp(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = exp(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_EXP_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = exp(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = exp(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = exp(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = exp(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = exp(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = exp(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = exp(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = exp(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = exp(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = exp(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = exp(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = exp(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = exp(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = exp(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = exp(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = exp(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SQRT_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sqrt(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SQRT_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sqrt(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sqrt(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sqrt(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SQRT_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sqrt(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sqrt(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sqrt(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = sqrt(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = sqrt(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = sqrt(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = sqrt(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = sqrt(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = sqrt(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = sqrt(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = sqrt(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = sqrt(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = sqrt(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = sqrt(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = sqrt(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = sqrt(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FMOD_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = fmod(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FMOD_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = fmod(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = fmod(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = fmod(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FMOD_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = fmod(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = fmod(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = fmod(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << " out[cmds[i].output[0]+3] = fmod(GETPARAM(0,3), GETPARAM(1,3));\n";
        out << " out[cmds[i].output[0]+4] = fmod(GETPARAM(0,4), GETPARAM(1,4));\n";
        out << " out[cmds[i].output[0]+5] = fmod(GETPARAM(0,5), GETPARAM(1,5));\n";
        out << " out[cmds[i].output[0]+6] = fmod(GETPARAM(0,6), GETPARAM(1,6));\n";
        out << " out[cmds[i].output[0]+7] = fmod(GETPARAM(0,7), GETPARAM(1,7));\n";
        out << " out[cmds[i].output[0]+8] = fmod(GETPARAM(0,8), GETPARAM(1,8));\n";
        out << " out[cmds[i].output[0]+9] = fmod(GETPARAM(0,9), GETPARAM(1,9));\n";
        out << " out[cmds[i].output[0]+10] = fmod(GETPARAM(0,10), GETPARAM(1,10));\n";
        out << " out[cmds[i].output[0]+11] = fmod(GETPARAM(0,11), GETPARAM(1,11));\n";
        out << " out[cmds[i].output[0]+12] = fmod(GETPARAM(0,12), GETPARAM(1,12));\n";
        out << " out[cmds[i].output[0]+13] = fmod(GETPARAM(0,13), GETPARAM(1,13));\n";
        out << " out[cmds[i].output[0]+14] = fmod(GETPARAM(0,14), GETPARAM(1,14));\n";
        out << " out[cmds[i].output[0]+15] = fmod(GETPARAM(0,15), GETPARAM(1,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MOD_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = glsl_mod1f(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MOD_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = glsl_mod3f(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = glsl_mod3f(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = glsl_mod3f(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MOD_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = glsl_mod16f(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = glsl_mod16f(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = glsl_mod16f(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << " out[cmds[i].output[0]+3] = glsl_mod16f(GETPARAM(0,3), GETPARAM(1,3));\n";
        out << " out[cmds[i].output[0]+4] = glsl_mod16f(GETPARAM(0,4), GETPARAM(1,4));\n";
        out << " out[cmds[i].output[0]+5] = glsl_mod16f(GETPARAM(0,5), GETPARAM(1,5));\n";
        out << " out[cmds[i].output[0]+6] = glsl_mod16f(GETPARAM(0,6), GETPARAM(1,6));\n";
        out << " out[cmds[i].output[0]+7] = glsl_mod16f(GETPARAM(0,7), GETPARAM(1,7));\n";
        out << " out[cmds[i].output[0]+8] = glsl_mod16f(GETPARAM(0,8), GETPARAM(1,8));\n";
        out << " out[cmds[i].output[0]+9] = glsl_mod16f(GETPARAM(0,9), GETPARAM(1,9));\n";
        out << " out[cmds[i].output[0]+10] = glsl_mod16f(GETPARAM(0,10), GETPARAM(1,10));\n";
        out << " out[cmds[i].output[0]+11] = glsl_mod16f(GETPARAM(0,11), GETPARAM(1,11));\n";
        out << " out[cmds[i].output[0]+12] = glsl_mod16f(GETPARAM(0,12), GETPARAM(1,12));\n";
        out << " out[cmds[i].output[0]+13] = glsl_mod16f(GETPARAM(0,13), GETPARAM(1,13));\n";
        out << " out[cmds[i].output[0]+14] = glsl_mod16f(GETPARAM(0,14), GETPARAM(1,14));\n";
        out << " out[cmds[i].output[0]+15] = glsl_mod16f(GETPARAM(0,15), GETPARAM(1,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MAX_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = max(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MAX_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = max(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = max(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = max(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MAX_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = max(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = max(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = max(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << " out[cmds[i].output[0]+3] = max(GETPARAM(0,3), GETPARAM(1,3));\n";
        out << " out[cmds[i].output[0]+4] = max(GETPARAM(0,4), GETPARAM(1,4));\n";
        out << " out[cmds[i].output[0]+5] = max(GETPARAM(0,5), GETPARAM(1,5));\n";
        out << " out[cmds[i].output[0]+6] = max(GETPARAM(0,6), GETPARAM(1,6));\n";
        out << " out[cmds[i].output[0]+7] = max(GETPARAM(0,7), GETPARAM(1,7));\n";
        out << " out[cmds[i].output[0]+8] = max(GETPARAM(0,8), GETPARAM(1,8));\n";
        out << " out[cmds[i].output[0]+9] = max(GETPARAM(0,9), GETPARAM(1,9));\n";
        out << " out[cmds[i].output[0]+10] = max(GETPARAM(0,10), GETPARAM(1,10));\n";
        out << " out[cmds[i].output[0]+11] = max(GETPARAM(0,11), GETPARAM(1,11));\n";
        out << " out[cmds[i].output[0]+12] = max(GETPARAM(0,12), GETPARAM(1,12));\n";
        out << " out[cmds[i].output[0]+13] = max(GETPARAM(0,13), GETPARAM(1,13));\n";
        out << " out[cmds[i].output[0]+14] = max(GETPARAM(0,14), GETPARAM(1,14));\n";
        out << " out[cmds[i].output[0]+15] = max(GETPARAM(0,15), GETPARAM(1,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MIN_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = min(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MIN_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = min(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = min(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = min(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MIN_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = min(GETPARAM(0,0), GETPARAM(1,0));\n";
        out << " out[cmds[i].output[0]+1] = min(GETPARAM(0,1), GETPARAM(1,1));\n";
        out << " out[cmds[i].output[0]+2] = min(GETPARAM(0,2), GETPARAM(1,2));\n";
        out << " out[cmds[i].output[0]+3] = min(GETPARAM(0,3), GETPARAM(1,3));\n";
        out << " out[cmds[i].output[0]+4] = min(GETPARAM(0,4), GETPARAM(1,4));\n";
        out << " out[cmds[i].output[0]+5] = min(GETPARAM(0,5), GETPARAM(1,5));\n";
        out << " out[cmds[i].output[0]+6] = min(GETPARAM(0,6), GETPARAM(1,6));\n";
        out << " out[cmds[i].output[0]+7] = min(GETPARAM(0,7), GETPARAM(1,7));\n";
        out << " out[cmds[i].output[0]+8] = min(GETPARAM(0,8), GETPARAM(1,8));\n";
        out << " out[cmds[i].output[0]+9] = min(GETPARAM(0,9), GETPARAM(1,9));\n";
        out << " out[cmds[i].output[0]+10] = min(GETPARAM(0,10), GETPARAM(1,10));\n";
        out << " out[cmds[i].output[0]+11] = min(GETPARAM(0,11), GETPARAM(1,11));\n";
        out << " out[cmds[i].output[0]+12] = min(GETPARAM(0,12), GETPARAM(1,12));\n";
        out << " out[cmds[i].output[0]+13] = min(GETPARAM(0,13), GETPARAM(1,13));\n";
        out << " out[cmds[i].output[0]+14] = min(GETPARAM(0,14), GETPARAM(1,14));\n";
        out << " out[cmds[i].output[0]+15] = min(GETPARAM(0,15), GETPARAM(1,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ABS_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = fabs(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ABS_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = fabs(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = fabs(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = fabs(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ABS_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = fabs(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = fabs(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = fabs(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = fabs(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = fabs(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = fabs(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = fabs(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = fabs(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = fabs(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = fabs(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = fabs(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = fabs(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = fabs(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = fabs(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = fabs(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = fabs(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_DOT_PRODUCT)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = dot((float3)(GETPARAM(0,0), GETPARAM(0,1), "
               "GETPARAM(0,2)),(float3)(GETPARAM(1,0), GETPARAM(1,1), GETPARAM(1,2)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CROSS_PRODUCT)\n";
        out << "{\n";
        out << " float3 const crossProduct = cross((float3)(GETPARAM(0,0), GETPARAM(0,1), "
               "GETPARAM(0,2)),(float3)(GETPARAM(1,0), GETPARAM(1,1), GETPARAM(1,2)));\n";
        out << " out[cmds[i].output[0]] = crossProduct.x;\n";
        out << " out[cmds[i].output[0]+1] = crossProduct.y;\n";
        out << " out[cmds[i].output[0]+2] = crossProduct.z;\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_LENGTH)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = length((float3)(GETPARAM(0,0), "
               "GETPARAM(0,1),GETPARAM(0,2)));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_RESOURCE)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_DECOMPOSE_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[1]] = GETPARAM(0,1);\n";
        out << " out[cmds[i].output[2]] = GETPARAM(0,2);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COMPOSE_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[0]+1] = GETPARAM(1,0);\n";
        out << " out[cmds[i].output[0]+2] = GETPARAM(2,0);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_DECOMPOSE_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[1]] = GETPARAM(0,1);\n";
        out << " out[cmds[i].output[2]] = GETPARAM(0,2);\n";
        out << " out[cmds[i].output[3]] = GETPARAM(0,3);\n";
        out << " out[cmds[i].output[4]] = GETPARAM(0,4);\n";
        out << " out[cmds[i].output[5]] = GETPARAM(0,5);\n";
        out << " out[cmds[i].output[6]] = GETPARAM(0,6);\n";
        out << " out[cmds[i].output[7]] = GETPARAM(0,7);\n";
        out << " out[cmds[i].output[8]] = GETPARAM(0,8);\n";
        out << " out[cmds[i].output[9]] = GETPARAM(0,9);\n";
        out << " out[cmds[i].output[10]] = GETPARAM(0,10);\n";
        out << " out[cmds[i].output[11]] = GETPARAM(0,11);\n";
        out << " out[cmds[i].output[12]] = GETPARAM(0,12);\n";
        out << " out[cmds[i].output[13]] = GETPARAM(0,13);\n";
        out << " out[cmds[i].output[14]] = GETPARAM(0,14);\n";
        out << " out[cmds[i].output[15]] = GETPARAM(0,15);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COMPOSE_MATRIX_FROM_COLUMNS)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[0]+1] = GETPARAM(1,0);\n";
        out << " out[cmds[i].output[0]+2] = GETPARAM(2,0);\n";
        out << " out[cmds[i].output[0]+3] = GETPARAM(3,0);\n";
        out << " out[cmds[i].output[0]+4] = GETPARAM(0,1);\n";
        out << " out[cmds[i].output[0]+5] = GETPARAM(1,1);\n";
        out << " out[cmds[i].output[0]+6] = GETPARAM(2,1);\n";
        out << " out[cmds[i].output[0]+7] = GETPARAM(3,1);\n";
        out << " out[cmds[i].output[0]+8] = GETPARAM(0,2);\n";
        out << " out[cmds[i].output[0]+9] = GETPARAM(1,2);\n";
        out << " out[cmds[i].output[0]+10] = GETPARAM(2,2);\n";
        out << " out[cmds[i].output[0]+11] = GETPARAM(3,2);\n";
        out << " out[cmds[i].output[0]+12] = GETPARAM(0,3);\n";
        out << " out[cmds[i].output[0]+13] = GETPARAM(1,3);\n";
        out << " out[cmds[i].output[0]+14] = GETPARAM(2,3);\n";
        out << " out[cmds[i].output[0]+15] = GETPARAM(3,3);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COMPOSE_MATRIX_FROM_ROWS)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[0]+1] = GETPARAM(0,1);\n";
        out << " out[cmds[i].output[0]+2] = GETPARAM(0,2);\n";
        out << " out[cmds[i].output[0]+3] = GETPARAM(0,3);\n";
        out << " out[cmds[i].output[0]+4] = GETPARAM(1,0);\n";
        out << " out[cmds[i].output[0]+5] = GETPARAM(1,1);\n";
        out << " out[cmds[i].output[0]+6] = GETPARAM(1,2);\n";
        out << " out[cmds[i].output[0]+7] = GETPARAM(1,3);\n";
        out << " out[cmds[i].output[0]+8] = GETPARAM(2,0);\n";
        out << " out[cmds[i].output[0]+9] = GETPARAM(2,1);\n";
        out << " out[cmds[i].output[0]+10] = GETPARAM(2,2);\n";
        out << " out[cmds[i].output[0]+11] = GETPARAM(2,3);\n";
        out << " out[cmds[i].output[0]+12] = GETPARAM(3,0);\n";
        out << " out[cmds[i].output[0]+13] = GETPARAM(3,1);\n";
        out << " out[cmds[i].output[0]+14] = GETPARAM(3,2);\n";
        out << " out[cmds[i].output[0]+15] = GETPARAM(3,3);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_VECTOR_FROM_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[0]+1] = GETPARAM(0,0);\n";
        out << " out[cmds[i].output[0]+2] = GETPARAM(0,0);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_MATRIX_VECTOR_MULTIPLICATION)\n";
        out << "{\n";
        out << " float3 const product = matrixVectorMul3f((float16) (GETPARAM(0,0), "
               "GETPARAM(0,1), GETPARAM(0,2), GETPARAM(0,3), GETPARAM(0,4), GETPARAM(0,5), "
               "GETPARAM(0,6), GETPARAM(0,7), GETPARAM(0,8), GETPARAM(0,9), GETPARAM(0,10), "
               "GETPARAM(0,11), GETPARAM(0,12), GETPARAM(0,13), GETPARAM(0,14), "
               "GETPARAM(0,15)),(float3)(GETPARAM(1,0), GETPARAM(1,1), GETPARAM(1,2)));\n";
        out << " out[cmds[i].output[0]] = product.x;\n";
        out << " out[cmds[i].output[0]+1] = product.y;\n";
        out << " out[cmds[i].output[0]+2] = product.z;\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TRANSPOSE)\n";
        out << "{\n";
        out << " float16 const transposed = transpose((float16) (GETPARAM(0,0), GETPARAM(0,1), "
               "GETPARAM(0,2), GETPARAM(0,3), GETPARAM(0,4), GETPARAM(0,5), GETPARAM(0,6), "
               "GETPARAM(0,7), GETPARAM(0,8), GETPARAM(0,9), GETPARAM(0,10), GETPARAM(0,11), "
               "GETPARAM(0,12), GETPARAM(0,13), GETPARAM(0,14), GETPARAM(0,15)));\n";
        out << " out[cmds[i].output[0]] = transposed.s0;\n";
        out << " out[cmds[i].output[0]+1] = transposed.s1;\n";
        out << " out[cmds[i].output[0]+2] = transposed.s2;\n";
        out << " out[cmds[i].output[0]+3] = transposed.s3;\n";
        out << " out[cmds[i].output[0]+4] = transposed.s4;\n";
        out << " out[cmds[i].output[0]+5] = transposed.s5;\n";
        out << " out[cmds[i].output[0]+6] = transposed.s6;\n";
        out << " out[cmds[i].output[0]+7] = transposed.s7;\n";
        out << " out[cmds[i].output[0]+8] = transposed.s8;\n";
        out << " out[cmds[i].output[0]+9] = transposed.s9;\n";
        out << " out[cmds[i].output[0]+10] = transposed.sa;\n";
        out << " out[cmds[i].output[0]+11] = transposed.sb;\n";
        out << " out[cmds[i].output[0]+12] = transposed.sc;\n";
        out << " out[cmds[i].output[0]+13] = transposed.sd;\n";
        out << " out[cmds[i].output[0]+14] = transposed.se;\n";
        out << " out[cmds[i].output[0]+15] = transposed.sf;\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_INVERSE)\n";
        out << "{\n";
        out << " float16 const inversedMat = inverse((float16) (GETPARAM(0,0), GETPARAM(0,1), "
               "GETPARAM(0,2), GETPARAM(0,3), GETPARAM(0,4), GETPARAM(0,5), GETPARAM(0,6), "
               "GETPARAM(0,7), GETPARAM(0,8), GETPARAM(0,9), GETPARAM(0,10), GETPARAM(0,11), "
               "GETPARAM(0,12), GETPARAM(0,13), GETPARAM(0,14), GETPARAM(0,15)));\n";
        out << " out[cmds[i].output[0]] = inversedMat.s0;\n";
        out << " out[cmds[i].output[0]+1] = inversedMat.s1;\n";
        out << " out[cmds[i].output[0]+2] = inversedMat.s2;\n";
        out << " out[cmds[i].output[0]+3] = inversedMat.s3;\n";
        out << " out[cmds[i].output[0]+4] = inversedMat.s4;\n";
        out << " out[cmds[i].output[0]+5] = inversedMat.s5;\n";
        out << " out[cmds[i].output[0]+6] = inversedMat.s6;\n";
        out << " out[cmds[i].output[0]+7] = inversedMat.s7;\n";
        out << " out[cmds[i].output[0]+8] = inversedMat.s8;\n";
        out << " out[cmds[i].output[0]+9] = inversedMat.s9;\n";
        out << " out[cmds[i].output[0]+10] = inversedMat.sa;\n";
        out << " out[cmds[i].output[0]+11] = inversedMat.sb;\n";
        out << " out[cmds[i].output[0]+12] = inversedMat.sc;\n";
        out << " out[cmds[i].output[0]+13] = inversedMat.sd;\n";
        out << " out[cmds[i].output[0]+14] = inversedMat.se;\n";
        out << " out[cmds[i].output[0]+15] = inversedMat.sf;\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COSH_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = cosh(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COSH_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = cosh(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = cosh(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = cosh(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_COSH_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = cosh(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = cosh(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = cosh(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = cosh(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = cosh(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = cosh(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = cosh(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = cosh(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = cosh(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = cosh(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = cosh(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = cosh(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = cosh(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = cosh(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = cosh(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = cosh(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SINH_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sinh(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SINH_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sinh(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sinh(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sinh(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SINH_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sinh(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sinh(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sinh(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = sinh(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = sinh(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = sinh(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = sinh(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = sinh(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = sinh(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = sinh(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = sinh(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = sinh(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = sinh(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = sinh(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = sinh(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = sinh(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TANH_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = tanh(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TANH_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = tanh(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = tanh(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = tanh(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_TANH_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = tanh(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = tanh(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = tanh(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = tanh(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = tanh(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = tanh(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = tanh(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = tanh(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = tanh(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = tanh(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = tanh(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = tanh(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = tanh(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = tanh(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = tanh(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = tanh(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CLAMP_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = clamp(GETPARAM(0,0), GETPARAM(1,0), GETPARAM(2,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CLAMP_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = clamp(GETPARAM(0,0), GETPARAM(1,0), GETPARAM(2,0));\n";
        out << " out[cmds[i].output[0]+1] = clamp(GETPARAM(0,1), GETPARAM(1,1), GETPARAM(2,1));\n";
        out << " out[cmds[i].output[0]+2] = clamp(GETPARAM(0,2), GETPARAM(1,2), GETPARAM(2,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CLAMP_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = clamp(GETPARAM(0,0), GETPARAM(1,0), GETPARAM(2,0));\n";
        out << " out[cmds[i].output[0]+1] = clamp(GETPARAM(0,1), GETPARAM(1,1), GETPARAM(2,1));\n";
        out << " out[cmds[i].output[0]+2] = clamp(GETPARAM(0,2), GETPARAM(1,2), GETPARAM(2,2));\n";
        out << " out[cmds[i].output[0]+3] = clamp(GETPARAM(0,3), GETPARAM(1,3), GETPARAM(2,3));\n";
        out << " out[cmds[i].output[0]+4] = clamp(GETPARAM(0,4), GETPARAM(1,4), GETPARAM(2,4));\n";
        out << " out[cmds[i].output[0]+5] = clamp(GETPARAM(0,5), GETPARAM(1,5), GETPARAM(2,5));\n";
        out << " out[cmds[i].output[0]+6] = clamp(GETPARAM(0,6), GETPARAM(1,6), GETPARAM(2,6));\n";
        out << " out[cmds[i].output[0]+7] = clamp(GETPARAM(0,7), GETPARAM(1,7), GETPARAM(2,7));\n";
        out << " out[cmds[i].output[0]+8] = clamp(GETPARAM(0,8), GETPARAM(1,8), GETPARAM(2,8));\n";
        out << " out[cmds[i].output[0]+9] = clamp(GETPARAM(0,9), GETPARAM(1,9), GETPARAM(2,9));\n";
        out << " out[cmds[i].output[0]+10] = clamp(GETPARAM(0,10), GETPARAM(1,10), "
               "GETPARAM(2,10));\n";
        out << " out[cmds[i].output[0]+11] = clamp(GETPARAM(0,11), GETPARAM(1,11), "
               "GETPARAM(2,11));\n";
        out << " out[cmds[i].output[0]+12] = clamp(GETPARAM(0,12), GETPARAM(1,12), "
               "GETPARAM(2,12));\n";
        out << " out[cmds[i].output[0]+13] = clamp(GETPARAM(0,13), GETPARAM(1,13), "
               "GETPARAM(2,13));\n";
        out << " out[cmds[i].output[0]+14] = clamp(GETPARAM(0,14), GETPARAM(1,14), "
               "GETPARAM(2,14));\n";
        out << " out[cmds[i].output[0]+15] = clamp(GETPARAM(0,15), GETPARAM(1,15), "
               "GETPARAM(2,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SELECT_SCALAR)\n";
        out << "{\n";
        out << " bool const b0 = GETPARAM(0,0) > GETPARAM(1,0);\n";
        out << " out[cmds[i].output[0]] = b0 ? GETPARAM(2,0) : GETPARAM(3,0);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SELECT_VECTOR)\n";
        out << "{\n";
        out << " bool const b0 = GETPARAM(0,0) > GETPARAM(1,0);\n";
        out << " bool const b1 = GETPARAM(0,1) > GETPARAM(1,1);\n";
        out << " bool const b2 = GETPARAM(0,2) > GETPARAM(1,2);\n";
        out << " out[cmds[i].output[0]] = b0 ? GETPARAM(2,0) : GETPARAM(3,0);\n";
        out << " out[cmds[i].output[0]+1] = b1 ? GETPARAM(2,1) : GETPARAM(3,1);\n";
        out << " out[cmds[i].output[0]+2] = b2 ? GETPARAM(2,2) : GETPARAM(3,2);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SELECT_MATRIX)\n";
        out << "{\n";
        out << " bool const b0 = GETPARAM(0,0) > GETPARAM(1,0);\n";
        out << " bool const b1 = GETPARAM(0,1) > GETPARAM(1,1);\n";
        out << " bool const b2 = GETPARAM(0,2) > GETPARAM(1,2);\n";
        out << " bool const b3 = GETPARAM(0,3) > GETPARAM(1,3);\n";
        out << " bool const b4 = GETPARAM(0,4) > GETPARAM(1,4);\n";
        out << " bool const b5 = GETPARAM(0,5) > GETPARAM(1,5);\n";
        out << " bool const b6 = GETPARAM(0,6) > GETPARAM(1,6);\n";
        out << " bool const b7 = GETPARAM(0,7) > GETPARAM(1,7);\n";
        out << " bool const b8 = GETPARAM(0,8) > GETPARAM(1,8);\n";
        out << " bool const b9 = GETPARAM(0,9) > GETPARAM(1,9);\n";
        out << " bool const b10 = GETPARAM(0,10) > GETPARAM(1,10);\n";
        out << " bool const b11 = GETPARAM(0,11) > GETPARAM(1,11);\n";
        out << " bool const b12 = GETPARAM(0,12) > GETPARAM(1,12);\n";
        out << " bool const b13 = GETPARAM(0,13) > GETPARAM(1,13);\n";
        out << " bool const b14 = GETPARAM(0,14) > GETPARAM(1,14);\n";
        out << " bool const b15 = GETPARAM(0,15) > GETPARAM(1,15);\n";
        out << " out[cmds[i].output[0]] = b0 ? GETPARAM(2,0) : GETPARAM(3,0);\n";
        out << " out[cmds[i].output[0]+1] = b1 ? GETPARAM(2,1) : GETPARAM(3,1);\n";
        out << " out[cmds[i].output[0]+2] = b2 ? GETPARAM(2,2) : GETPARAM(3,2);\n";
        out << " out[cmds[i].output[0]+3] = b3 ? GETPARAM(2,3) : GETPARAM(3,3);\n";
        out << " out[cmds[i].output[0]+4] = b4 ? GETPARAM(2,4) : GETPARAM(3,4);\n";
        out << " out[cmds[i].output[0]+5] = b5 ? GETPARAM(2,5) : GETPARAM(3,5);\n";
        out << " out[cmds[i].output[0]+6] = b6 ? GETPARAM(2,6) : GETPARAM(3,6);\n";
        out << " out[cmds[i].output[0]+7] = b7 ? GETPARAM(2,7) : GETPARAM(3,7);\n";
        out << " out[cmds[i].output[0]+8] = b8 ? GETPARAM(2,8) : GETPARAM(3,8);\n";
        out << " out[cmds[i].output[0]+9] = b9 ? GETPARAM(2,9) : GETPARAM(3,9);\n";
        out << " out[cmds[i].output[0]+10] = b10 ? GETPARAM(2,10) : GETPARAM(3,10);\n";
        out << " out[cmds[i].output[0]+11] = b11 ? GETPARAM(2,11) : GETPARAM(3,11);\n";
        out << " out[cmds[i].output[0]+12] = b12 ? GETPARAM(2,12) : GETPARAM(3,12);\n";
        out << " out[cmds[i].output[0]+13] = b13 ? GETPARAM(2,13) : GETPARAM(3,13);\n";
        out << " out[cmds[i].output[0]+14] = b14 ? GETPARAM(2,14) : GETPARAM(3,14);\n";
        out << " out[cmds[i].output[0]+15] = b15 ? GETPARAM(2,15) : GETPARAM(3,15);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ROUND_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = round(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ROUND_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = round(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = round(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = round(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_ROUND_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = round(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = round(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = round(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = round(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = round(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = round(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = round(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = round(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = round(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = round(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = round(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = round(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = round(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = round(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = round(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = round(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CEIL_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ceil(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CEIL_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ceil(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = ceil(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = ceil(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_CEIL_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = ceil(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = ceil(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = ceil(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = ceil(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = ceil(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = ceil(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = ceil(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = ceil(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = ceil(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = ceil(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = ceil(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = ceil(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = ceil(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = ceil(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = ceil(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = ceil(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FLOOR_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = floor(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FLOOR_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = floor(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = floor(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = floor(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FLOOR_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = floor(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = floor(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = floor(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = floor(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = floor(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = floor(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = floor(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = floor(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = floor(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = floor(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = floor(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = floor(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = floor(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = floor(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = floor(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = floor(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SIGN_SCALAR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sign(GETPARAM(0,0));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SIGN_VECTOR)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sign(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sign(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sign(GETPARAM(0,2));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_SIGN_MATRIX)\n";
        out << "{\n";
        out << " out[cmds[i].output[0]] = sign(GETPARAM(0,0));\n";
        out << " out[cmds[i].output[0]+1] = sign(GETPARAM(0,1));\n";
        out << " out[cmds[i].output[0]+2] = sign(GETPARAM(0,2));\n";
        out << " out[cmds[i].output[0]+3] = sign(GETPARAM(0,3));\n";
        out << " out[cmds[i].output[0]+4] = sign(GETPARAM(0,4));\n";
        out << " out[cmds[i].output[0]+5] = sign(GETPARAM(0,5));\n";
        out << " out[cmds[i].output[0]+6] = sign(GETPARAM(0,6));\n";
        out << " out[cmds[i].output[0]+7] = sign(GETPARAM(0,7));\n";
        out << " out[cmds[i].output[0]+8] = sign(GETPARAM(0,8));\n";
        out << " out[cmds[i].output[0]+9] = sign(GETPARAM(0,9));\n";
        out << " out[cmds[i].output[0]+10] = sign(GETPARAM(0,10));\n";
        out << " out[cmds[i].output[0]+11] = sign(GETPARAM(0,11));\n";
        out << " out[cmds[i].output[0]+12] = sign(GETPARAM(0,12));\n";
        out << " out[cmds[i].output[0]+13] = sign(GETPARAM(0,13));\n";
        out << " out[cmds[i].output[0]+14] = sign(GETPARAM(0,14));\n";
        out << " out[cmds[i].output[0]+15] = sign(GETPARAM(0,15));\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FRACT_SCALAR)\n";
        out << "{\n";
        out << " float ipart;\n";
        out << " out[cmds[i].output[0]] = fract(GETPARAM(0,0), &ipart);\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FRACT_VECTOR)\n";
        out << "{\n";
        out << " float3 ipart;\n";
        out << " float3 const fpart = fract(GETPARAM(0,0), &ipart);\n";
        out << " out[cmds[i].output[0]] = fpart.x;\n";
        out << " out[cmds[i].output[0]+1] = fpart.y;\n";
        out << " out[cmds[i].output[0]+2] = fpart.z;\n";
        out << "}\n";

        out << "if (cmds[i].type == CT_FRACT_MATRIX)\n";
        out << "{\n";
        out << " float16 ipart;\n";
        out << " float16 const fpart = fract(GETPARAM(0,0), &ipart);\n";
        out << " out[cmds[i].output[0]] = fpart.s0;\n";
        out << " out[cmds[i].output[0]+1] = fpart.s1;\n";
        out << " out[cmds[i].output[0]+2] = fpart.s2;\n";
        out << " out[cmds[i].output[0]+3] = fpart.s3;\n";
        out << " out[cmds[i].output[0]+4] = fpart.s4;\n";
        out << " out[cmds[i].output[0]+5] = fpart.s5;\n";
        out << " out[cmds[i].output[0]+6] = fpart.s6;\n";
        out << " out[cmds[i].output[0]+7] = fpart.s7;\n";
        out << " out[cmds[i].output[0]+8] = fpart.s8;\n";
        out << " out[cmds[i].output[0]+9] = fpart.s9;\n";
        out << " out[cmds[i].output[0]+10] = fpart.sa;\n";
        out << " out[cmds[i].output[0]+11] = fpart.sb;\n";
        out << " out[cmds[i].output[0]+12] = fpart.sc;\n";
        out << " out[cmds[i].output[0]+13] = fpart.sd;\n";
        out << " out[cmds[i].output[0]+14] = fpart.se;\n";
        out << " out[cmds[i].output[0]+15] = fpart.sf;\n";
        out << "}\n";

        // sampleImageNearest4f(float3 uvw, float3 dimensions, int start, PAYLOAD_ARGS)
        out << "if (cmds[i].type == CT_IMAGE_SAMPLER)\n";
        out << "{\n";
        out << " float4 color = (float4)(0.0f, 0.0f, 0.0f, 1.0f);\n";
        out << " float3 const uvw = (float3)(GETPARAM(0,0), GETPARAM(0,1), GETPARAM(0,2));\n";
        out
          << " float3 const dimensions = (float3)(GETPARAM(1,0), GETPARAM(1,1), GETPARAM(1,2));\n";
        out << " int const start =  convert_int(GETPARAM(2,0));\n";
        out << " int3 const tileStyle = (int3)(convert_int(GETPARAM(3,0)), "
               "convert_int(GETPARAM(4,0)), "
               "convert_int(GETPARAM(5,0)));\n";
        out << " int filter = convert_int(GETPARAM(6,0));\n";
        out << " if (filter == 0)\n";
        out << " color = sampleImageNearest4f(uvw, dimensions, start, tileStyle, "
               "PASS_PAYLOAD_ARGS);\n";
        out << " else\n";
        out << " color = sampleImageLinear4f(uvw, dimensions, start, tileStyle, "
               "PASS_PAYLOAD_ARGS);\n";
        out << " out[cmds[i].output[0]] = color.x;\n";
        out << " out[cmds[i].output[0]+1] = color.y;\n";
        out << " out[cmds[i].output[0]+2] = color.z;\n";
        out << " out[cmds[i].output[1]] = color.w;\n";
        out << "}\n";

        // bbox(float3 pos, float3 min, float3 max)
        out << "if (cmds[i].type == CT_BOX_MIN_MAX)\n";
        out << "{\n";
        out << " float3 const pos = (float3)(GETPARAM(0,0), GETPARAM(0,1), GETPARAM(0,2));\n";
        out << " float3 const min = (float3)(GETPARAM(1,0), GETPARAM(1,1), GETPARAM(1,2));\n";
        out << " float3 const max = (float3)(GETPARAM(2,0), GETPARAM(2,1), GETPARAM(2,2));\n";
        out << " out[cmds[i].output[0]] = bbox(pos, min, max);\n";
        out << "}\n";

        out << "}\n"; // for loop

        out << m_resultStatement.str();
        out << "}\n";
    }

    bool ToCommandStreamVisitor::isOutPutOfNodeValid(const NodeBase & node)
    {
        if (m_endReached)
        {
            // this node is not used for the output
            return false;
        }

        const auto visitedIter =
          std::find(std::begin(m_visitedNodes), std::end(m_visitedNodes), node.getId());
        if (visitedIter != std::end(m_visitedNodes))
        {
            throw std::runtime_error("duplication of node");
        }
        m_visitedNodes.insert(node.getId());
        return true;
    }

    Begin & ToCommandStreamVisitor::getAssemblyBegin() const
    {
        if (!m_assembly)
        {
            throw std::runtime_error("assembly not set");
        }
        auto * beginNode = m_assembly->assemblyModel()->getBeginNode();
        if (beginNode == nullptr)
        {
            throw std::runtime_error("begin node of assembly is null");
        }
        return *beginNode;
    }

    ArgumentIndices ToCommandStreamVisitor::getLookUpIndex(IParameter & parameter)
    {
        return getLookUpIndex(parameter, m_modelId);
    }

    ArgumentIndices ToCommandStreamVisitor::getLookUpIndex(IParameter & parameter,
                                                           ResourceId modelID) const
    {
        ArgumentIndices lookUpIndices{};

        if (parameter.getSource().has_value())
        {
            auto portId = parameter.getSource().value().portId;
            auto const sourceModelId = modelID;
            if (m_partNode && m_partBegin)
            {
                if (parameter.getSource()->nodeId == m_partBegin->getId())
                {
                    // Check arguments of part node
                    const auto extendedName =
                      std::string{"part_"} + parameter.getSource().value().shortName;
                    const auto iterArgumentOutput = m_partNode->outputs().find(extendedName);
                    if (iterArgumentOutput != std::end(m_partNode->outputs()))
                    {
                        portId = iterArgumentOutput->second.getId();
                    }

                    else
                    {
                        const auto iterToSourceOutput =
                          m_partNode->outputs().find(parameter.getSource().value().shortName);
                        if (iterToSourceOutput != std::end(m_partNode->outputs()))
                        {
                            portId = iterToSourceOutput->second.getId();
                        }
                        else
                        {
                            std::cerr << "failed to find input for "
                                      << parameter.getSource().value().uniqueName << "\n";
                        }
                    }
                }
            }

            const auto iter = m_portIdToOutputIndex.at(sourceModelId).find(portId);
            if (iter != std::end(m_portIdToOutputIndex.at(sourceModelId)))
            {
                const auto lid = iter->second;

                for (int i = 0; i < parameter.getSize(); ++i)
                {
                    lookUpIndices[i] = -(lid + i);
                }
                return lookUpIndices;
            }

            throw std::runtime_error("port is not registered as output");
        }

        for (int i = 0; i < parameter.getSize(); ++i)
        {
            lookUpIndices[i] = parameter.getLookUpIndex() + i;
            if (!isLookUpIndexValid(lookUpIndices[i]))
            {
                throw std::runtime_error("look up index is not valid");
            }
        }

        return lookUpIndices;
    }

    int ToCommandStreamVisitor::acquireOutputIndex(Port & port, size_t numComponents)
    {
        const auto iter = m_portIdToOutputIndex[m_modelId].find(port.getId());
        if (iter == std::end(m_portIdToOutputIndex[m_modelId]))
        {
            m_portIdToOutputIndex[m_modelId][port.getId()] = m_currentOutPutIndex;

            m_currentOutPutIndex += static_cast<int>(numComponents);
            return m_portIdToOutputIndex[m_modelId][port.getId()];
        }
        return iter->second;
    }

    bool ToCommandStreamVisitor::isLookUpIndexValid(int & lookUpIndex) const
    {
        if (lookUpIndex < 0)
        {
            auto const index = -lookUpIndex;
            return (index < m_currentOutPutIndex);
        }
        else
        {
            return true;
        }
    }

    void ToCommandStreamVisitor::visit(Begin & beginning)
    {
        if (!m_currentModel)
        {
            throw std::runtime_error("model not set");
        }
        bool const isAssembly =
          m_currentModel->getResourceId() == m_assembly->assemblyModel()->getResourceId();
        if (isAssembly)
        {
            acquireOutputIndex(beginning.getOutputs().at(FieldNames::Pos), 3);

            m_signature << fmt::format("float4 model(float3 {0} ,  PAYLOAD_ARGS)\n{{\n",
                                       beginning.getOutputs().at(FieldNames::Pos).getUniqueName());
        }

        m_partBegin = beginning;
        m_endReached = false;
    }

    void ToCommandStreamVisitor::visit(End & ending)
    {
        m_partEnding = ending;
        bool isAssembly =
          m_currentModel->getResourceId() == m_assembly->assemblyModel()->getResourceId();
        if (!isAssembly)
        {
            return;
        }

        if (!isOutPutOfNodeValid(ending))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_END;
        cmd.id = ending.getId();
        cmd.output[0] = 4;
        cmd.output[1] = 5;
        cmd.output[2] = 6;
        cmd.output[3] = 7;

        cmd.args[0] = getLookUpIndex(ending.parameter().at(FieldNames::Color))[0]; // r
        cmd.args[1] = getLookUpIndex(ending.parameter().at(FieldNames::Color))[1]; // g
        cmd.args[2] = getLookUpIndex(ending.parameter().at(FieldNames::Color))[2]; // b
        cmd.args[3] = getLookUpIndex(ending.parameter().at(FieldNames::Shape))[0];

        m_cmds->getData().push_back(cmd);
        m_resultStatement << "struct Command ending = cmds[sizeOfCmds-1];";
        m_resultStatement
          << "return (float4) (GETPARAM2(ending.args[0]), GETPARAM2(ending.args[1]), "
             "GETPARAM2(ending.args[2]), GETPARAM2(ending.args[3]));\n";

        m_endReached = true;
    }

    void ToCommandStreamVisitor::visit(NodeBase & node)
    {
        throw std::runtime_error(
          fmt::format("Node type for {} not implemented", node.getUniqueName()));

        //   std::cerr << fmt::format("Skipping node  {}, nodetype it is not implemented",
        //   node.getUniqueName())
        //             << std::endl;
    }

    void ToCommandStreamVisitor::visit(BoxMinMax & boxMinMax)
    {
        if (!isOutPutOfNodeValid(boxMinMax))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_BOX_MIN_MAX;
        cmd.id = boxMinMax.getId();

        cmd.output[0] = acquireOutputIndex(boxMinMax.getOutputs().at(FieldNames::Shape), 1);

        cmd.args[0] = getLookUpIndex(boxMinMax.parameter().at(FieldNames::Min))[0];
        cmd.args[1] = getLookUpIndex(boxMinMax.parameter().at(FieldNames::Max))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(ConstantScalar & constantScalar)
    {
        if (!isOutPutOfNodeValid(constantScalar))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_CONSTANT_SCALAR;
        cmd.id = constantScalar.getId();

        cmd.output[0] = acquireOutputIndex(constantScalar.getOutputs().at(FieldNames::Value), 1);
        cmd.args[0] = getLookUpIndex(constantScalar.parameter().at(FieldNames::Value))[0];
        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(ConstantVector & constantVector)
    {
        if (!isOutPutOfNodeValid(constantVector))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_CONSTANT_VECTOR;
        cmd.id = constantVector.getId();

        cmd.output[0] = acquireOutputIndex(constantVector.getOutputs().at(FieldNames::Vector), 3);
        cmd.output[1] = cmd.output[0] + 1;
        cmd.output[2] = cmd.output[0] + 2;

        cmd.args[0] = getLookUpIndex(constantVector.parameter().at(FieldNames::X))[0];
        cmd.args[1] = getLookUpIndex(constantVector.parameter().at(FieldNames::Y))[0];
        cmd.args[2] = getLookUpIndex(constantVector.parameter().at(FieldNames::Z))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(ConstantMatrix & constantMatrix)
    {
        if (!isOutPutOfNodeValid(constantMatrix))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_CONSTANT_MATRIX;
        cmd.id = constantMatrix.getId();

        cmd.output[0] = acquireOutputIndex(constantMatrix.getOutputs()[FieldNames::Matrix], 16);

        cmd.args[0] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M00))[0];
        cmd.args[1] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M01))[0];
        cmd.args[2] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M02))[0];
        cmd.args[3] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M03))[0];
        cmd.args[4] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M10))[0];
        cmd.args[5] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M11))[0];
        cmd.args[6] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M12))[0];
        cmd.args[7] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M13))[0];
        cmd.args[8] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M20))[0];
        cmd.args[9] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M21))[0];
        cmd.args[10] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M22))[0];
        cmd.args[11] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M23))[0];
        cmd.args[12] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M30))[0];
        cmd.args[13] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M31))[0];
        cmd.args[14] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M32))[0];
        cmd.args[15] = getLookUpIndex(constantMatrix.parameter().at(FieldNames::M33))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(ComposeVector & composeVector)
    {
        if (!isOutPutOfNodeValid(composeVector))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_COMPOSE_VECTOR;
        cmd.id = composeVector.getId();

        cmd.output[0] = acquireOutputIndex(composeVector.getOutputs().at(FieldNames::Result), 3);
        cmd.output[1] = cmd.output[0] + 1;
        cmd.output[2] = cmd.output[0] + 2;

        cmd.args[0] = getLookUpIndex(composeVector.parameter().at(FieldNames::X))[0];
        cmd.args[1] = getLookUpIndex(composeVector.parameter().at(FieldNames::Y))[0];
        cmd.args[2] = getLookUpIndex(composeVector.parameter().at(FieldNames::Z))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(ComposeMatrix & composeMatrix)
    {
        if (!isOutPutOfNodeValid(composeMatrix))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_COMPOSE_MATRIX;
        cmd.id = composeMatrix.getId();

        cmd.output[0] = acquireOutputIndex(composeMatrix.getOutputs()[FieldNames::Matrix], 16);

        cmd.args[0] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M00))[0];
        cmd.args[1] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M01))[0];
        cmd.args[2] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M02))[0];
        cmd.args[3] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M03))[0];
        cmd.args[4] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M10))[0];
        cmd.args[5] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M11))[0];
        cmd.args[6] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M12))[0];
        cmd.args[7] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M13))[0];
        cmd.args[8] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M20))[0];
        cmd.args[9] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M21))[0];
        cmd.args[10] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M22))[0];
        cmd.args[11] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M23))[0];
        cmd.args[12] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M30))[0];
        cmd.args[13] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M31))[0];
        cmd.args[14] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M32))[0];
        cmd.args[15] = getLookUpIndex(composeMatrix.parameter().at(FieldNames::M33))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(ComposeMatrixFromColumns & composeMatrixFromColumns)
    {
        if (!isOutPutOfNodeValid(composeMatrixFromColumns))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_COMPOSE_MATRIX_FROM_COLUMNS;
        cmd.id = composeMatrixFromColumns.getId();

        cmd.output[0] =
          acquireOutputIndex(composeMatrixFromColumns.getOutputs()[FieldNames::Matrix], 16);

        cmd.args[0] = getLookUpIndex(composeMatrixFromColumns.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(composeMatrixFromColumns.parameter().at(FieldNames::B))[0];
        cmd.args[2] = getLookUpIndex(composeMatrixFromColumns.parameter().at(FieldNames::C))[0];
        cmd.args[3] = getLookUpIndex(composeMatrixFromColumns.parameter().at(FieldNames::D))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(ComposeMatrixFromRows & composeMatrixFromRows)
    {
        if (!isOutPutOfNodeValid(composeMatrixFromRows))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_COMPOSE_MATRIX_FROM_ROWS;
        cmd.id = composeMatrixFromRows.getId();

        cmd.output[0] =
          acquireOutputIndex(composeMatrixFromRows.getOutputs()[FieldNames::Matrix], 16);

        cmd.args[0] = getLookUpIndex(composeMatrixFromRows.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(composeMatrixFromRows.parameter().at(FieldNames::B))[0];
        cmd.args[2] = getLookUpIndex(composeMatrixFromRows.parameter().at(FieldNames::C))[0];
        cmd.args[3] = getLookUpIndex(composeMatrixFromRows.parameter().at(FieldNames::D))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(SignedDistanceToMesh & signedDistanceToMesh)
    {
        if (!isOutPutOfNodeValid(signedDistanceToMesh))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_SIGNED_DISTANCE_TO_MESH;
        cmd.id = signedDistanceToMesh.getId();

        cmd.output[0] =
          acquireOutputIndex(signedDistanceToMesh.getOutputs().at(FieldNames::Distance), 1);

        cmd.args[0] = getLookUpIndex(signedDistanceToMesh.parameter().at(FieldNames::Pos))[0];
        cmd.args[1] = getLookUpIndex(signedDistanceToMesh.parameter().at(FieldNames::Start))[0];
        cmd.args[2] = getLookUpIndex(signedDistanceToMesh.parameter().at(FieldNames::End))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(UnsignedDistanceToMesh & unsignedDistanceToMesh)
    {
        if (!isOutPutOfNodeValid(unsignedDistanceToMesh))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_UNSIGNED_DISTANCE_TO_MESH;
        cmd.id = unsignedDistanceToMesh.getId();

        cmd.output[0] =
          acquireOutputIndex(unsignedDistanceToMesh.getOutputs().at(FieldNames::Distance), 1);

        cmd.args[0] = getLookUpIndex(unsignedDistanceToMesh.parameter().at(FieldNames::Pos))[0];
        cmd.args[1] = getLookUpIndex(unsignedDistanceToMesh.parameter().at(FieldNames::Start))[0];
        cmd.args[2] = getLookUpIndex(unsignedDistanceToMesh.parameter().at(FieldNames::End))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Addition & addition)
    {
        if (!isOutPutOfNodeValid(addition))
        {
            return;
        }

        Command cmd;
        auto const dimension = addition.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_ADDITION_SCALAR;
            break;
        case 3:
            cmd.type = CT_ADDITION_VECTOR;
            break;
        case 16:
            cmd.type = CT_ADDITION_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = addition.getId();

        cmd.output[0] = acquireOutputIndex(addition.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(addition.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(addition.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Division & division)
    {
        if (!isOutPutOfNodeValid(division))
        {
            return;
        }

        Command cmd;
        auto const dimension = division.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_DIVISION_SCALAR;
            break;
        case 3:
            cmd.type = CT_DIVISION_VECTOR;
            break;
        case 16:
            cmd.type = CT_DIVISION_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = division.getId();

        cmd.output[0] = acquireOutputIndex(division.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(division.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(division.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(DotProduct & dotProduct)
    {
        if (!isOutPutOfNodeValid(dotProduct))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_DOT_PRODUCT;
        cmd.id = dotProduct.getId();

        cmd.output[0] = acquireOutputIndex(dotProduct.getResultOutputPort(), 1);

        cmd.args[0] = getLookUpIndex(dotProduct.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(dotProduct.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(CrossProduct & crossProduct)
    {
        if (!isOutPutOfNodeValid(crossProduct))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_CROSS_PRODUCT;
        cmd.id = crossProduct.getId();

        cmd.output[0] = acquireOutputIndex(crossProduct.getResultOutputPort(), 3);

        cmd.args[0] = getLookUpIndex(crossProduct.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(crossProduct.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(MatrixVectorMultiplication & matrixVectorMultiplication)
    {
        if (!isOutPutOfNodeValid(matrixVectorMultiplication))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_MATRIX_VECTOR_MULTIPLICATION;
        cmd.id = matrixVectorMultiplication.getId();

        cmd.output[0] = acquireOutputIndex(matrixVectorMultiplication.getResultOutputPort(), 3);

        cmd.args[0] = getLookUpIndex(matrixVectorMultiplication.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(matrixVectorMultiplication.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Transpose & transpose)
    {
        if (!isOutPutOfNodeValid(transpose))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_TRANSPOSE;
        cmd.id = transpose.getId();

        cmd.output[0] = acquireOutputIndex(transpose.getOutputs().at(FieldNames::Result), 1);

        cmd.args[0] = getLookUpIndex(transpose.parameter().at(FieldNames::Matrix))[0];
    }

    void ToCommandStreamVisitor::visit(Sine & sine)
    {
        visitImplMathFunction(sine, CT_SINE_SCALAR, CT_SINE_VECTOR, CT_SINE_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Subtraction & subtraction)
    {
        if (!isOutPutOfNodeValid(subtraction))
        {
            return;
        }

        Command cmd;
        auto const dimension = subtraction.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_SUBTRACTION_SCALAR;
            break;
        case 3:
            cmd.type = CT_SUBTRACTION_VECTOR;
            break;
        case 16:
            cmd.type = CT_SUBTRACTION_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = subtraction.getId();

        cmd.output[0] = acquireOutputIndex(subtraction.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(subtraction.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(subtraction.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Multiplication & multiplication)
    {
        if (!isOutPutOfNodeValid(multiplication))
        {
            return;
        }

        Command cmd;

        auto const dimension = multiplication.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_MULTIPLICATION_SCALAR;
            break;
        case 3:
            cmd.type = CT_MULTIPLICATION_VECTOR;
            break;
        case 16:
            cmd.type = CT_MULTIPLICATION_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }

        cmd.id = multiplication.getId();

        cmd.output[0] = acquireOutputIndex(multiplication.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(multiplication.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(multiplication.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Cosine & cosine)
    {
        visitImplMathFunction(cosine, CT_COSINE_SCALAR, CT_COSINE_VECTOR, CT_COSINE_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Tangent & tangent)
    {
        visitImplMathFunction(tangent, CT_TANGENT_SCALAR, CT_TANGENT_VECTOR, CT_TANGENT_MATRIX);
    }

    void ToCommandStreamVisitor::visit(ArcSin & arcSin)
    {
        visitImplMathFunction(arcSin, CT_ARC_SIN_SCALAR, CT_ARC_SIN_VECTOR, CT_ARC_SIN_MATRIX);
    }

    void ToCommandStreamVisitor::visit(ArcCos & arcCos)
    {
        visitImplMathFunction(arcCos, CT_ARC_COS_SCALAR, CT_ARC_COS_VECTOR, CT_ARC_COS_MATRIX);
    }

    void ToCommandStreamVisitor::visit(ArcTan & arcTan)
    {
        visitImplMathFunction(arcTan, CT_ARC_TAN_SCALAR, CT_ARC_TAN_VECTOR, CT_ARC_TAN_MATRIX);
    }

    void ToCommandStreamVisitor::visit(ArcTan2 & arcTan2)
    {
        if (!isOutPutOfNodeValid(arcTan2))
        {
            return;
        }

        Command cmd;
        auto const dimension = arcTan2.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_ARC_TAN2_SCALAR;
            break;
        case 3:
            cmd.type = CT_ARC_TAN2_VECTOR;
            break;
        case 16:
            cmd.type = CT_ARC_TAN2_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = arcTan2.getId();

        cmd.output[0] = acquireOutputIndex(arcTan2.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(arcTan2.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(arcTan2.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Pow & power)
    {
        if (!isOutPutOfNodeValid(power))
        {
            return;
        }

        Command cmd;
        auto const dimension = power.parameter().at(FieldNames::Base).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_POW_SCALAR;
            break;
        case 3:
            cmd.type = CT_POW_VECTOR;
            break;
        case 16:
            cmd.type = CT_POW_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = power.getId();

        cmd.output[0] = acquireOutputIndex(power.getOutputs().at(FieldNames::Value), 1);

        cmd.args[0] = getLookUpIndex(power.parameter().at(FieldNames::Base))[0];
        cmd.args[1] = getLookUpIndex(power.parameter().at(FieldNames::Exponent))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Log & log)
    {
        visitImplMathFunction(log, CT_LOG_SCALAR, CT_LOG_VECTOR, CT_LOG_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Log2 & log2)
    {
        visitImplMathFunction(log2, CT_LOG2_SCALAR, CT_LOG2_VECTOR, CT_LOG2_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Log10 & log10)
    {
        visitImplMathFunction(log10, CT_LOG10_SCALAR, CT_LOG10_VECTOR, CT_LOG10_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Exp & exp)
    {
        visitImplMathFunction(exp, CT_EXP_SCALAR, CT_EXP_VECTOR, CT_EXP_MATRIX);
    }

    void ToCommandStreamVisitor::visit(CosH & cosH)
    {
        visitImplMathFunction(cosH, CT_COSH_SCALAR, CT_COSH_VECTOR, CT_COSH_MATRIX);
    }

    void ToCommandStreamVisitor::visit(SinH & sinH)
    {
        visitImplMathFunction(sinH, CT_SINH_SCALAR, CT_SINH_VECTOR, CT_SINH_MATRIX);
    }

    void ToCommandStreamVisitor::visit(TanH & tanH)
    {
        visitImplMathFunction(tanH, CT_TANH_SCALAR, CT_TANH_VECTOR, CT_TANH_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Clamp & clamp)
    {
        if (!isOutPutOfNodeValid(clamp))
        {
            return;
        }

        Command cmd;
        auto const dimension = clamp.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_CLAMP_SCALAR;
            break;
        case 3:
            cmd.type = CT_CLAMP_VECTOR;
            break;
        case 16:
            cmd.type = CT_CLAMP_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = clamp.getId();

        cmd.output[0] = acquireOutputIndex(clamp.getOutputs().at(FieldNames::Result), dimension);

        cmd.args[0] = getLookUpIndex(clamp.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(clamp.parameter().at(FieldNames::Min))[0];
        cmd.args[2] = getLookUpIndex(clamp.parameter().at(FieldNames::Max))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Select & select)
    {
        if (!isOutPutOfNodeValid(select))
        {
            return;
        }

        Command cmd;
        auto const dimension = select.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_SELECT_SCALAR;
            break;
        case 3:
            cmd.type = CT_SELECT_VECTOR;
            break;
        case 16:
            cmd.type = CT_SELECT_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = select.getId();

        cmd.output[0] = acquireOutputIndex(select.getOutputs().at(FieldNames::Result), dimension);

        cmd.args[0] = getLookUpIndex(select.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(select.parameter().at(FieldNames::B))[0];
        cmd.args[2] = getLookUpIndex(select.parameter().at(FieldNames::C))[0];
        cmd.args[3] = getLookUpIndex(select.parameter().at(FieldNames::D))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Round & round)
    {
        visitImplMathFunction(round, CT_ROUND_SCALAR, CT_ROUND_VECTOR, CT_ROUND_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Ceil & ceil)
    {
        visitImplMathFunction(ceil, CT_CEIL_SCALAR, CT_CEIL_VECTOR, CT_CEIL_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Floor & floor)
    {
        visitImplMathFunction(floor, CT_FLOOR_SCALAR, CT_FLOOR_VECTOR, CT_FLOOR_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Sign & sign)
    {
        visitImplMathFunction(sign, CT_SIGN_SCALAR, CT_SIGN_VECTOR, CT_SIGN_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Fract & fract)
    {
        visitImplMathFunction(fract, CT_FRACT_SCALAR, CT_FRACT_VECTOR, CT_FRACT_MATRIX);
    }

    void ToCommandStreamVisitor::visit(VectorFromScalar & vectorFromScalar)
    {
        if (!isOutPutOfNodeValid(vectorFromScalar))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_VECTOR_FROM_SCALAR;
        cmd.id = vectorFromScalar.getId();

        cmd.output[0] = acquireOutputIndex(vectorFromScalar.getResultOutputPort(), 3);

        cmd.args[0] = getLookUpIndex(vectorFromScalar.parameter().at(FieldNames::A))[0];

        m_cmds->getData().push_back(cmd);
    }
    void ToCommandStreamVisitor::visit(Sqrt & sqrtNode)
    {
        visitImplMathFunction(sqrtNode, CT_SQRT_SCALAR, CT_SQRT_VECTOR, CT_SQRT_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Fmod & modulus)
    {
        if (!isOutPutOfNodeValid(modulus))
        {
            return;
        }

        Command cmd;
        auto const dimension = modulus.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_FMOD_SCALAR;
            break;
        case 3:
            cmd.type = CT_FMOD_VECTOR;
            break;
        case 16:
            cmd.type = CT_FMOD_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = modulus.getId();

        cmd.output[0] = acquireOutputIndex(modulus.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(modulus.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(modulus.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Mod & modulus)
    {
        if (!isOutPutOfNodeValid(modulus))
        {
            return;
        }

        Command cmd;
        auto const dimension = modulus.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_MOD_SCALAR;
            break;
        case 3:
            cmd.type = CT_MOD_VECTOR;
            break;
        case 16:
            cmd.type = CT_MOD_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = modulus.getId();

        cmd.output[0] = acquireOutputIndex(modulus.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(modulus.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(modulus.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Max & maxNode)
    {
        if (!isOutPutOfNodeValid(maxNode))
        {
            return;
        }

        Command cmd;
        auto const dimension = maxNode.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_MAX_SCALAR;
            break;
        case 3:
            cmd.type = CT_MAX_VECTOR;
            break;
        case 16:
            cmd.type = CT_MAX_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = maxNode.getId();

        cmd.output[0] = acquireOutputIndex(maxNode.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(maxNode.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(maxNode.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Min & minNode)
    {
        if (!isOutPutOfNodeValid(minNode))
        {
            return;
        }

        Command cmd;
        auto const dimension = minNode.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_MIN_SCALAR;
            break;
        case 3:
            cmd.type = CT_MIN_VECTOR;
            break;
        case 16:
            cmd.type = CT_MIN_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = minNode.getId();

        cmd.output[0] = acquireOutputIndex(minNode.getResultOutputPort(), dimension);

        cmd.args[0] = getLookUpIndex(minNode.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(minNode.parameter().at(FieldNames::B))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Abs & absNode)
    {
        visitImplMathFunction(absNode, CT_ABS_SCALAR, CT_ABS_VECTOR, CT_ABS_MATRIX);
    }

    void ToCommandStreamVisitor::visit(Mix & mix)
    {
        if (!isOutPutOfNodeValid(mix))
        {
            return;
        }

        Command cmd;
        auto const dimension = mix.parameter().at(FieldNames::A).getSize();
        switch (dimension)
        {
        case 1:
            cmd.type = CT_MIX_SCALAR;
            break;
        case 3:
            cmd.type = CT_MIX_VECTOR;
            break;
        case 16:
            cmd.type = CT_MIX_MATRIX;
            break;
        default:
            throw std::runtime_error("dimension not supported");
        }
        cmd.id = mix.getId();

        cmd.output[0] = acquireOutputIndex(mix.getOutputs().at(FieldNames::Value), dimension);

        cmd.args[0] = getLookUpIndex(mix.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(mix.parameter().at(FieldNames::B))[0];
        cmd.args[2] = getLookUpIndex(mix.parameter().at(FieldNames::Ratio))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Transformation & transformation)
    {
        if (!isOutPutOfNodeValid(transformation))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_TRANSFORMATION;
        cmd.id = transformation.getId();

        cmd.output[0] = acquireOutputIndex(transformation.getOutputs().at(FieldNames::Pos), 3);

        cmd.args[0] = getLookUpIndex(transformation.parameter().at(FieldNames::Pos))[0];
        auto lookUpIndicesTransFormation =
          getLookUpIndex(transformation.parameter().at(FieldNames::Transformation));

        for (int i = 0; i < 16; ++i)
        {
            cmd.args[i + 1] = lookUpIndicesTransFormation[i];
        }

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Resource & resource)
    {
        return;
    }

    void ToCommandStreamVisitor::visit(ImageSampler & imageSampler)
    {
        if (!isOutPutOfNodeValid(imageSampler))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_IMAGE_SAMPLER;
        cmd.id = imageSampler.getId();

        cmd.output[0] = acquireOutputIndex(imageSampler.getOutputs().at(FieldNames::Color), 3);
        cmd.output[1] = acquireOutputIndex(imageSampler.getOutputs().at(FieldNames::Alpha), 1);

        cmd.args[0] = getLookUpIndex(imageSampler.parameter().at(FieldNames::UVW))[0];
        cmd.args[1] = getLookUpIndex(imageSampler.parameter().at(FieldNames::Dimensions))[0];
        cmd.args[2] = getLookUpIndex(imageSampler.parameter().at(FieldNames::Start))[0];
        cmd.args[3] = getLookUpIndex(imageSampler.parameter().at(FieldNames::TileStyleU))[0];
        cmd.args[4] = getLookUpIndex(imageSampler.parameter().at(FieldNames::TileStyleV))[0];
        cmd.args[5] = getLookUpIndex(imageSampler.parameter().at(FieldNames::TileStyleW))[0];
        cmd.args[6] = getLookUpIndex(imageSampler.parameter().at(FieldNames::Filter))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(DecomposeVector & decomposeVector)
    {
        if (!isOutPutOfNodeValid(decomposeVector))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_DECOMPOSE_VECTOR;
        cmd.id = decomposeVector.getId();

        cmd.output[0] = acquireOutputIndex(decomposeVector.getOutputs().at(FieldNames::X), 1);
        cmd.output[1] = acquireOutputIndex(decomposeVector.getOutputs().at(FieldNames::Y), 1);
        cmd.output[2] = acquireOutputIndex(decomposeVector.getOutputs().at(FieldNames::Z), 1);

        cmd.args[0] = getLookUpIndex(decomposeVector.parameter().at(FieldNames::A))[0];
        cmd.args[1] = getLookUpIndex(decomposeVector.parameter().at(FieldNames::A))[1];
        cmd.args[2] = getLookUpIndex(decomposeVector.parameter().at(FieldNames::A))[2];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Inverse & inverse)
    {
        if (!isOutPutOfNodeValid(inverse))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_INVERSE;
        cmd.id = inverse.getId();

        cmd.output[0] = acquireOutputIndex(inverse.getOutputs().at(FieldNames::Result), 16);

        cmd.args[0] = getLookUpIndex(inverse.parameter().at(FieldNames::A))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(Length & length)
    {
        if (!isOutPutOfNodeValid(length))
        {
            return;
        }

        Command cmd;
        cmd.type = CT_LENGTH;
        cmd.id = length.getId();

        cmd.output[0] = acquireOutputIndex(length.getOutputs().at(FieldNames::Result), 1);

        cmd.args[0] = getLookUpIndex(length.parameter().at(FieldNames::A))[0];

        m_cmds->getData().push_back(cmd);
    }

    void ToCommandStreamVisitor::visit(FunctionCall &)
    {
        return;
    }

    void ToCommandStreamVisitor::visit(FunctionGradient &)
    {
        // TODO: Command stream support for FunctionGradient is not yet implemented.
        // FunctionGradient requires multiple function evaluations (central differences)
        // which would need special command stream handling. For now, this node is only
        // supported in OpenCL code generation (ToOCLVisitor).
        return;
    }
} // namespace gladius::nodes
