#pragma once
#include "ClonableNode.h"
#include "NodeBase.h"
#include "NodesWithSpezializedAccesors.h"
#include "Parameter.h"
#include "nodesfwd.h"

#include "Primitives.h"
#include <filesystem>
#include <fmt/format.h>
#include <limits>
#include <map>

#include "ImageStackResource.h"
#include "ResourceContext.h"
#include "ResourceKey.h"
#include "ResourceManager.h"
#include "VdbResource.h"

namespace gladius::nodes
{

    TypeRules operatorFunctionRules();
    TypeRules functionRules();

    TypeRules twoParameterFuncRules();

    class Begin : public ClonableNode<Begin>
    {
      public:
        Begin()
            : Begin({})
        {
            setDisplayName("Inputs");
        }

        explicit Begin(NodeId id)
            : ClonableNode<Begin>(NodeName("Input"), id, Category::Internal)
        {
            TypeRule posRule = {RuleType::Default, InputTypeMap{}, OutputTypeMap{}};
            m_typeRules = {posRule};
            applyTypeRule(posRule);

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"A Begin node provides the function arguments. It always provides \"cs\" "
                    "(=coordinate system), the point in space at which the function is "
                    "evaluated. Note that if you apply transformations to implicit geometries you "
                    "are changing the coordinate system rather then the transformation of the "
                    "geometry itself."};
        }

        /// @brief Begin nodes are exempt from input validation as they are input markers
        [[nodiscard]] bool isExemptFromInputValidation() const override
        {
            return true;
        }

        // name does not match, but it is called when the model is updated
        void updateMemoryOffsets(GeneratorContext &) override
        {
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }
    };

    class End : public ClonableNode<End>
    {
      public:
        End()
            : End({})
        {
            setDisplayName("Outputs");
        }

        explicit End(NodeId id)
            : ClonableNode<End>(NodeName("Output"), id, Category::Internal)
        {

            TypeRule rule = {RuleType::Default, InputTypeMap{}, OutputTypeMap{}};

            m_typeRules = {rule};
            applyTypeRule(rule);

            updateNodeIds();
            screenPos().x = 500;
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"A End node consumes the calculated distance (shape) and color. \"End\" can be "
                    "seen as the end of a function."};
        }

        /// @brief End nodes are exempt from input validation as they are output markers
        [[nodiscard]] bool isExemptFromInputValidation() const override
        {
            return true;
        }

        // name does not match, but it is called when the model is updated
        void updateMemoryOffsets(GeneratorContext &) override
        {
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }
    };

    class ConstantScalar : public ClonableNode<ConstantScalar>
    {
      public:
        ConstantScalar()
            : ConstantScalar({})
        {
        }

        explicit ConstantScalar(NodeId id)
            : ClonableNode<ConstantScalar>(NodeName("ConstantScalar"), id, Category::Misc)
        {
            m_typeRules = {{RuleType::Default,
                            InputTypeMap{{FieldNames::Value, ParameterTypeIndex::Float}},
                            OutputTypeMap{{FieldNames::Value, ParameterTypeIndex::Float}}}};
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
            m_parameter.at(FieldNames::Value).setInputSourceRequired(false);
        }

        [[nodiscard]] float getValue() const
        {
            auto value = m_parameter.at(FieldNames::Value).getValue();
            if (const auto val = std::get_if<float>(&value))
            {
                return *val;
            }
            return 0.f;
        }

        [[nodiscard]] Port & getValueOutputPort()
        {
            return m_outputs.at(FieldNames::Value);
        }

        /// @brief Constant nodes are exempt from input validation as they provide constant values
        [[nodiscard]] bool isExemptFromInputValidation() const override
        {
            return true;
        }

        // name does not match, but it is called when the model is updated
        void updateMemoryOffsets(GeneratorContext &) override
        {
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }
    };

    class ConstantVector : public ClonableNode<ConstantVector>
    {
      public:
        ConstantVector()
            : ConstantVector({})
        {
        }

        explicit ConstantVector(NodeId id)
            : ClonableNode<ConstantVector>(NodeName("ConstantVector"), id, Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::X, ParameterTypeIndex::Float},
                                          {FieldNames::Y, ParameterTypeIndex::Float},
                                          {FieldNames::Z, ParameterTypeIndex::Float}},
                             OutputTypeMap{{FieldNames::Vector, ParameterTypeIndex::Float3}}};
            m_typeRules = {rule};
            applyTypeRule(rule);
            updateNodeIds();
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }

        [[nodiscard]] float3 getValue() const
        {
            auto x = m_parameter.at(FieldNames::X).getValue();
            auto y = m_parameter.at(FieldNames::Y).getValue();
            auto z = m_parameter.at(FieldNames::Z).getValue();
            if (const auto xVal = std::get_if<float>(&x))
            {
                if (const auto yVal = std::get_if<float>(&y))
                {
                    if (const auto zVal = std::get_if<float>(&z))
                    {
                        return float3{*xVal, *yVal, *zVal};
                    }
                }
            }
            return float3{};
        }

        // name does not match, but it is called when the model is updated
        void updateMemoryOffsets(GeneratorContext &) override
        {
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }

        /// @brief Constant nodes are exempt from input validation as they provide constant values
        [[nodiscard]] bool isExemptFromInputValidation() const override
        {
            return true;
        }

        [[nodiscard]] Port & getVectorOutputPort()
        {
            return m_outputs.at(FieldNames::Vector);
        }
    };

    class ConstantMatrix : public ClonableNode<ConstantMatrix>
    {
      public:
        ConstantMatrix()
            : ConstantMatrix({})
        {
        }

        explicit ConstantMatrix(NodeId id)
            : ClonableNode<ConstantMatrix>(NodeName("ConstantMatrix"), id, Category::Misc)
        {
            // considering m00 to m33
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::M00, ParameterTypeIndex::Float},
                                          {FieldNames::M01, ParameterTypeIndex::Float},
                                          {FieldNames::M02, ParameterTypeIndex::Float},
                                          {FieldNames::M03, ParameterTypeIndex::Float},
                                          {FieldNames::M10, ParameterTypeIndex::Float},
                                          {FieldNames::M11, ParameterTypeIndex::Float},
                                          {FieldNames::M12, ParameterTypeIndex::Float},
                                          {FieldNames::M13, ParameterTypeIndex::Float},
                                          {FieldNames::M20, ParameterTypeIndex::Float},
                                          {FieldNames::M21, ParameterTypeIndex::Float},
                                          {FieldNames::M22, ParameterTypeIndex::Float},
                                          {FieldNames::M23, ParameterTypeIndex::Float},
                                          {FieldNames::M30, ParameterTypeIndex::Float},
                                          {FieldNames::M31, ParameterTypeIndex::Float},
                                          {FieldNames::M32, ParameterTypeIndex::Float},
                                          {FieldNames::M33, ParameterTypeIndex::Float}},
                             OutputTypeMap{{FieldNames::Matrix, ParameterTypeIndex::Matrix4}}};
            m_typeRules = {rule};
            applyTypeRule(rule);
            updateNodeIds();

            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }

        [[nodiscard]] Matrix4x4 getValue() const
        {
            Matrix4x4 mat;
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    std::string const m_ij = fmt::format("m{}{}", i, j);
                    auto value = m_parameter.at(m_ij).getValue();
                    if (const auto val = std::get_if<float>(&value))
                    {
                        mat[i][j] = *val;
                    }
                }
            }
            return mat;
        }

        // name does not match, but it is called when the model is updated
        void updateMemoryOffsets(GeneratorContext &) override
        {
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }

        /// @brief Constant nodes are exempt from input validation as they provide constant values
        [[nodiscard]] bool isExemptFromInputValidation() const override
        {
            return true;
        }

        [[nodiscard]] Port & getMatrixOutputPort()
        {
            return m_outputs.at(FieldNames::Matrix);
        }
    };

    class DecomposeVector : public ClonableNode<DecomposeVector>
    {
      public:
        DecomposeVector()
            : DecomposeVector({})
        {
        }

        explicit DecomposeVector(NodeId id)
            : ClonableNode<DecomposeVector>(NodeName("DecomposeVector"), id, Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3}},
                             OutputTypeMap{{FieldNames::X, ParameterTypeIndex::Float},
                                           {FieldNames::Y, ParameterTypeIndex::Float},
                                           {FieldNames::Z, ParameterTypeIndex::Float}}};
            m_typeRules = {rule};
            applyTypeRule(rule);

            updateNodeIds();
        }
    };

    class ComposeVector : public ClonableNode<ComposeVector>
    {
      public:
        ComposeVector()
            : ComposeVector({})
        {
        }

        explicit ComposeVector(NodeId id)
            : ClonableNode<ComposeVector>(NodeName("ComposeVector"), id, Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::X, ParameterTypeIndex::Float},
                                          {FieldNames::Y, ParameterTypeIndex::Float},
                                          {FieldNames::Z, ParameterTypeIndex::Float}},
                             OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}};
            m_typeRules = {rule};
            applyTypeRule(rule);

            updateNodeIds();
        }
    };

    class ComposeMatrix : public ClonableNode<ComposeMatrix>
    {
      public:
        ComposeMatrix()
            : ComposeMatrix({})
        {
        }

        explicit ComposeMatrix(NodeId id)
            : ClonableNode<ComposeMatrix>(NodeName("ComposeMatrix"), id, Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::M00, ParameterTypeIndex::Float},
                                          {FieldNames::M01, ParameterTypeIndex::Float},
                                          {FieldNames::M02, ParameterTypeIndex::Float},
                                          {FieldNames::M03, ParameterTypeIndex::Float},
                                          {FieldNames::M10, ParameterTypeIndex::Float},
                                          {FieldNames::M11, ParameterTypeIndex::Float},
                                          {FieldNames::M12, ParameterTypeIndex::Float},
                                          {FieldNames::M13, ParameterTypeIndex::Float},
                                          {FieldNames::M20, ParameterTypeIndex::Float},
                                          {FieldNames::M21, ParameterTypeIndex::Float},
                                          {FieldNames::M22, ParameterTypeIndex::Float},
                                          {FieldNames::M23, ParameterTypeIndex::Float},
                                          {FieldNames::M30, ParameterTypeIndex::Float},
                                          {FieldNames::M31, ParameterTypeIndex::Float},
                                          {FieldNames::M32, ParameterTypeIndex::Float},
                                          {FieldNames::M33, ParameterTypeIndex::Float}},
                             OutputTypeMap{{FieldNames::Matrix, ParameterTypeIndex::Matrix4}}};

            m_typeRules = {rule};
            applyTypeRule(rule);

            updateNodeIds();
        }
    };

    class DecomposeMatrix : public ClonableNode<DecomposeMatrix>
    {
      public:
        DecomposeMatrix()
            : DecomposeMatrix({})
        {
        }

        explicit DecomposeMatrix(NodeId id)
            : ClonableNode<DecomposeMatrix>(NodeName("DecomposeMatrix"), id, Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::Matrix, ParameterTypeIndex::Matrix4}},
                             OutputTypeMap{{FieldNames::M00, ParameterTypeIndex::Float},
                                           {FieldNames::M01, ParameterTypeIndex::Float},
                                           {FieldNames::M02, ParameterTypeIndex::Float},
                                           {FieldNames::M03, ParameterTypeIndex::Float},
                                           {FieldNames::M10, ParameterTypeIndex::Float},
                                           {FieldNames::M11, ParameterTypeIndex::Float},
                                           {FieldNames::M12, ParameterTypeIndex::Float},
                                           {FieldNames::M13, ParameterTypeIndex::Float},
                                           {FieldNames::M20, ParameterTypeIndex::Float},
                                           {FieldNames::M21, ParameterTypeIndex::Float},
                                           {FieldNames::M22, ParameterTypeIndex::Float},
                                           {FieldNames::M23, ParameterTypeIndex::Float},
                                           {FieldNames::M30, ParameterTypeIndex::Float},
                                           {FieldNames::M31, ParameterTypeIndex::Float},
                                           {FieldNames::M32, ParameterTypeIndex::Float},
                                           {FieldNames::M33, ParameterTypeIndex::Float}}};

            m_typeRules = {rule};
            applyTypeRule(rule);

            updateNodeIds();
        }
    };

    class ComposeMatrixFromColumns : public ClonableNode<ComposeMatrixFromColumns>
    {
      public:
        ComposeMatrixFromColumns()
            : ComposeMatrixFromColumns({})
        {
        }

        explicit ComposeMatrixFromColumns(NodeId id)
            : ClonableNode<ComposeMatrixFromColumns>(NodeName("ComposeMatrixFromColumns"),
                                                     id,
                                                     Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::Col0, ParameterTypeIndex::Float3},
                                          {FieldNames::Col1, ParameterTypeIndex::Float3},
                                          {FieldNames::Col2, ParameterTypeIndex::Float3},
                                          {FieldNames::Col3, ParameterTypeIndex::Float3}},
                             OutputTypeMap{{FieldNames::Matrix, ParameterTypeIndex::Matrix4}}};

            m_typeRules = {rule};
            applyTypeRule(rule);
            updateNodeIds();
        }
    };

    class ComposeMatrixFromRows : public ClonableNode<ComposeMatrixFromRows>
    {
      public:
        ComposeMatrixFromRows()
            : ComposeMatrixFromRows({})
        {
        }

        explicit ComposeMatrixFromRows(NodeId id)
            : ClonableNode<ComposeMatrixFromRows>(NodeName("ComposeMatrixFromRows"),
                                                  id,
                                                  Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{{FieldNames::Row0, ParameterTypeIndex::Float3},
                                          {FieldNames::Row1, ParameterTypeIndex::Float3},
                                          {FieldNames::Row2, ParameterTypeIndex::Float3},
                                          {FieldNames::Row3, ParameterTypeIndex::Float3}},
                             OutputTypeMap{{FieldNames::Matrix, ParameterTypeIndex::Matrix4}}};

            m_typeRules = {rule};
            applyTypeRule(rule);

            updateNodeIds();
        }
    };

    class SignedDistanceToMesh : public ClonableNode<SignedDistanceToMesh>
    {
      public:
        SignedDistanceToMesh();
        explicit SignedDistanceToMesh(NodeId id);
        void updateMemoryOffsets(GeneratorContext & generatorContext);
    };

    class SignedDistanceToBeamLattice : public ClonableNode<SignedDistanceToBeamLattice>
    {
      public:
        SignedDistanceToBeamLattice();
        explicit SignedDistanceToBeamLattice(NodeId id);
        void updateMemoryOffsets(GeneratorContext & generatorContext);
    };

    class UnsignedDistanceToMesh : public ClonableNode<UnsignedDistanceToMesh>
    {
      public:
        UnsignedDistanceToMesh();
        explicit UnsignedDistanceToMesh(NodeId id);
        void updateMemoryOffsets(GeneratorContext & generatorContext);
    };

    class FunctionCall : public ClonableNode<FunctionCall>
    {
      public:
        FunctionCall()
            : FunctionCall({})
        {
        }

        explicit FunctionCall(NodeId id)
            : ClonableNode<FunctionCall>(NodeName("FunctionCall"), id, Category::Misc)
        {
            TypeRule rule = {RuleType::Default,
                             InputTypeMap{
                               {FieldNames::FunctionId, ParameterTypeIndex::ResourceId},
                             },
                             OutputTypeMap{}};

            m_typeRules = {rule};
            applyTypeRule(rule);

            updateInternalOutputs();

            m_parameter.at(FieldNames::FunctionId).setInputSourceRequired(false);
        }

        using ArgumentList =
          std::vector<std::pair<ParameterMap::key_type, ParameterMap::mapped_type *>>;
        // replace with view when avilable
        ArgumentList getArguments() const
        {
            ArgumentList result;
            for (auto parameter : m_parameter)
            {
                if (parameter.second.isArgument())
                {
                    result.push_back({parameter.first, &parameter.second});
                }
            }
            return result;
        }

        void updateMemoryOffsets(GeneratorContext &) override
        {
            resolveFunctionId();
        }

        void resolveFunctionId()
        {
            auto functionIdParameter = m_parameter.at(FieldNames::FunctionId);

            auto functionSource = functionIdParameter.getSource();
            if (!functionSource.has_value())
            {
                auto variantResId = functionIdParameter.getValue();
                if (const auto resId = std::get_if<ResourceId>(&variantResId))
                {
                    m_functionId = *resId;
                }
                return;
            }

            auto * sourcePort = functionSource.value().port;
            if (!sourcePort)
            {
                throw std::runtime_error("The functionId of the FunctionCall node " +
                                         getDisplayName() +
                                         " needs the value of a Resource node as an input");
            }

            auto sourceNode = sourcePort->getParent();

            if (!sourceNode)
            {
                throw std::runtime_error(
                  fmt::format("The functionId of the FunctionCall node {} needs the value of a "
                              "Resource node as an input",
                              getDisplayName()));
            }
            auto variantResId = sourceNode->parameter().at(FieldNames::ResourceId).getValue();
            if (const auto resId = std::get_if<ResourceId>(&variantResId))
            {
                m_functionId = *resId;
            }
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Calls a function."};
        }

        [[nodiscard]] ResourceId getFunctionId() const
        {
            return m_functionId;
        }

        void setFunctionId(ResourceId functionId)
        {
            m_functionId = functionId;

            auto & functionIdParameter = m_parameter.at(FieldNames::FunctionId);
            functionIdParameter.setValue(functionId);
        }

        const VariantParameter & getInputFunctionId() const
        {
            return m_parameter.at(FieldNames::FunctionId);
        }

        void updateInputsAndOutputs(Model & referencedModel);

      private:
        ResourceId m_functionId;

        void updateInternalOutputs()
        {
            updateNodeIds();
        }
    };

    class Addition : public CloneableABtoResult<Addition>
    {
      public:
        Addition()
            : Addition({})
        {
        }

        explicit Addition(NodeId id)
            : CloneableABtoResult<Addition>(NodeName("Addition"), id, Category::Math)
        {

            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the sum of A and B."};
        }
    };

    class Multiplication : public CloneableABtoResult<Multiplication>
    {
      public:
        Multiplication()
            : Multiplication({})
        {
        }

        explicit Multiplication(NodeId id)
            : CloneableABtoResult<Multiplication>(NodeName("Multiplication"), id, Category::Math)
        {

            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the product of A and B."};
        }
    };

    class Subtraction : public CloneableABtoResult<Subtraction>
    {
      public:
        Subtraction()
            : Subtraction({})
        {
        }

        explicit Subtraction(NodeId id)
            : CloneableABtoResult<Subtraction>(NodeName("Subtraction"), id, Category::Math)
        {

            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the difference of A and B."};
        }
    };

    class Division : public CloneableABtoResult<Division>
    {
      public:
        Division()
            : Division({})
        {
        }

        explicit Division(NodeId id)
            : CloneableABtoResult<Division>(NodeName("Division"), id, Category::Math)
        {

            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns A divided by B."};
        }
    };

    class Sine : public CloneableAtoResult<Sine>
    {
      public:
        Sine()
            : Sine({})
        {
        }

        explicit Sine(NodeId id)
            : CloneableAtoResult<Sine>(NodeName("Sine"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the sine of x. Note that x is in radians."};
        }
    };

    class Cosine : public CloneableAtoResult<Cosine>
    {
      public:
        Cosine()
            : Cosine({})
        {
        }

        explicit Cosine(NodeId id)
            : CloneableAtoResult<Cosine>(NodeName("Cosine"), id, Category::Math)
        {
            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the cosine of x. Note that x is in radians."};
        }
    };

    class Tangent : public CloneableAtoResult<Tangent>
    {
      public:
        Tangent()
            : Tangent({})
        {
        }

        explicit Tangent(NodeId id)
            : CloneableAtoResult<Tangent>(NodeName("Tangent"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the tangent of x. Note that x is in radians."};
        }
    };

    class SinH : public CloneableAtoResult<SinH>
    {
      public:
        SinH()
            : SinH({})
        {
        }

        explicit SinH(NodeId id)
            : CloneableAtoResult<SinH>(NodeName("SinH"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the hyperbolic sine of x. Note that x is in radians."};
        }
    };

    class CosH : public CloneableAtoResult<CosH>
    {
      public:
        CosH()
            : CosH({})
        {
        }

        explicit CosH(NodeId id)
            : CloneableAtoResult<CosH>(NodeName("CosH"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the hyperbolic cosine of x. Note that x is in radians."};
        }
    };

    class TanH : public CloneableAtoResult<TanH>
    {
      public:
        TanH()
            : TanH({})
        {
        }

        explicit TanH(NodeId id)
            : CloneableAtoResult<TanH>(NodeName("TanH"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the hyperbolic tangent of x. Note that x is in radians."};
        }
    };

    class ArcSin : public CloneableAtoResult<ArcSin>
    {
      public:
        ArcSin()
            : ArcSin({})
        {
        }

        explicit ArcSin(NodeId id)
            : CloneableAtoResult<ArcSin>(NodeName("ArcSin"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns arc sine of x."};
        }
    };

    class ArcCos : public CloneableAtoResult<ArcCos>
    {
      public:
        ArcCos()
            : ArcCos({})
        {
        }

        explicit ArcCos(NodeId id)
            : CloneableAtoResult<ArcCos>(NodeName("ArcCos"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns arc cosine of x."};
        }
    };

    class ArcTan : public CloneableAtoResult<ArcTan>
    {
      public:
        ArcTan()
            : ArcTan({})
        {
        }

        explicit ArcTan(NodeId id)
            : CloneableAtoResult<ArcTan>(NodeName("ArcTan"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns arc tangent of x."};
        }
    };

    class ArcTan2 : public CloneableABtoResult<ArcTan2>
    {
      public:
        ArcTan2()
            : ArcTan2({})
        {
        }

        explicit ArcTan2(NodeId id)
            : CloneableABtoResult<ArcTan2>(NodeName("ArcTan2"), id, Category::Math)
        {

            m_typeRules = twoParameterFuncRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the four quadrant arc tangent of y/x, using the signs of both "
                    "arguments to determine the quadrant of the return value."};
        }
    };

    class Pow : public ClonableNode<Pow>
    {
      public:
        Pow()
            : Pow({})
        {
        }

        explicit Pow(NodeId id)
            : ClonableNode<Pow>(NodeName("Pow"), id, Category::Math)
        {

            m_parameter[FieldNames::Base] = VariantParameter(0.f, ContentType::Generic);
            m_parameter[FieldNames::Exponent] = VariantParameter(0.f, ContentType::Generic);
            addOutputPort(FieldNames::Value, ParameterTypeIndex::Float);

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns Base^Exponent. "};
        }
    };

    class Exp : public CloneableAtoResult<Exp>
    {
      public:
        Exp()
            : Exp({})
        {
        }

        explicit Exp(NodeId id)
            : CloneableAtoResult<Exp>(NodeName("Exp"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the natural exponentiation of x."};
        }
    };

    class Log : public CloneableAtoResult<Log>
    {
      public:
        Log()
            : Log({})
        {
        }

        explicit Log(NodeId id)
            : CloneableAtoResult<Log>(NodeName("Log"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the natural logarithm of x."};
        }
    };

    class Log2 : public CloneableAtoResult<Log2>
    {
      public:
        Log2()
            : Log2({})
        {
        }

        explicit Log2(NodeId id)
            : CloneableAtoResult<Log2>(NodeName("Log2"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the base 2 logarithm of x."};
        }
    };

    class Log10 : public CloneableAtoResult<Log10>
    {
      public:
        Log10()
            : Log10({})
        {
        }

        explicit Log10(NodeId id)
            : CloneableAtoResult<Log10>(NodeName("Log10"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the base 10 logarithm of x."};
        }
    };

    class Select : public ClonableNode<Select>
    {
      public:
        Select()
            : Select({})
        {
        }

        explicit Select(NodeId id)
            : ClonableNode<Select>(NodeName("Select"), id, Category::Math)
        {
            // result = A < B ? C : D
            m_typeRules = {
              TypeRule{RuleType::Scalar,
                       InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float},
                                    {FieldNames::B, ParameterTypeIndex::Float},
                                    {FieldNames::C, ParameterTypeIndex::Float},
                                    {FieldNames::D, ParameterTypeIndex::Float}},
                       OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}},
              TypeRule{RuleType::Vector,
                       InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
                                    {FieldNames::B, ParameterTypeIndex::Float3},
                                    {FieldNames::C, ParameterTypeIndex::Float3},
                                    {FieldNames::D, ParameterTypeIndex::Float3}},
                       OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}},
              TypeRule{RuleType::Matrix,
                       InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4},
                                    {FieldNames::B, ParameterTypeIndex::Matrix4},
                                    {FieldNames::C, ParameterTypeIndex::Matrix4},
                                    {FieldNames::D, ParameterTypeIndex::Matrix4}},
                       OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Matrix4}}}};
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns A if Condition is true, otherwise returns B."};
        }
    };

    class Clamp : public ClonableNode<Clamp>
    {
      public:
        Clamp()
            : Clamp({})
        {
        }

        explicit Clamp(NodeId id)
            : ClonableNode<Clamp>(NodeName("Clamp"), id, Category::Math)
        {

            m_typeRules = {
              TypeRule{RuleType::Scalar,
                       InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float},
                                    {FieldNames::Min, ParameterTypeIndex::Float},
                                    {FieldNames::Max, ParameterTypeIndex::Float}},
                       OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}},
              TypeRule{RuleType::Vector,
                       InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
                                    {FieldNames::Min, ParameterTypeIndex::Float3},
                                    {FieldNames::Max, ParameterTypeIndex::Float3}},
                       OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}},
              TypeRule{RuleType::Matrix,
                       InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4},
                                    {FieldNames::Min, ParameterTypeIndex::Matrix4},
                                    {FieldNames::Max, ParameterTypeIndex::Matrix4}},
                       OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Matrix4}}}};
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Clamps x to the range [min, max]."};
        }
    };

    class Sqrt : public CloneableAtoResult<Sqrt>
    {
      public:
        Sqrt()
            : Sqrt({})
        {
        }

        explicit Sqrt(NodeId id)
            : CloneableAtoResult<Sqrt>(NodeName("Sqrt"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the square root of x."};
        }
    };

    class Fmod : public CloneableABtoResult<Fmod>
    {
      public:
        Fmod()
            : Fmod({})
        {
        }

        explicit Fmod(NodeId id)
            : CloneableABtoResult<Fmod>(NodeName("Fmod"), id, Category::Math)
        {

            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns A - B * truncated(A/B)"};
        }
    };

    class Mod : public CloneableABtoResult<Mod>
    {
      public:
        Mod()
            : Mod({})
        {
        }

        explicit Mod(NodeId id)
            : CloneableABtoResult<Mod>(NodeName("Mod"), id, Category::Math)
        {

            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns  A - B * floor(A/B)"};
        }
    };

    class Max : public CloneableABtoResult<Max>
    {
      public:
        Max()
            : Max({})
        {
        }

        explicit Max(NodeId id)
            : CloneableABtoResult<Max>(NodeName("Max"), id, Category::Math)
        {

            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the maximum of A and B"};
        }
    };

    class Min : public CloneableABtoResult<Min>
    {
      public:
        Min()
            : Min({})
        {
        }

        explicit Min(NodeId id)
            : CloneableABtoResult<Min>(NodeName("Min"), id, Category::Math)
        {
            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the minimum of A and B"};
        }
    };

    class Abs : public CloneableAtoResult<Abs>
    {
      public:
        Abs()
            : Abs({})
        {
        }

        explicit Abs(NodeId id)
            : CloneableAtoResult<Abs>(NodeName("Abs"), id, Category::Math)
        {

            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the absolute value of A"};
        }
    };

    class Length : public CloneableAtoResult<Length>
    {
      public:
        Length()
            : Length({})
        {
        }

        explicit Length(NodeId id)
            : CloneableAtoResult<Length>(NodeName("Length"), id, Category::Math)
        {
            TypeRule vectorToScalar = {
              RuleType::Default,
              InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3}},
              OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

            m_typeRules = {vectorToScalar};
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the length of vector A"};
        }
    };

    class Mix : public CloneableABtoResult<Mix>
    {
      public:
        Mix()
            : Mix({})
        {
        }

        explicit Mix(NodeId id)
            : CloneableABtoResult<Mix>(NodeName("Mix"), id, Category::Math)
        {
            m_typeRules = operatorFunctionRules();
            applyTypeRule(m_typeRules.front());

            addOutputPort(FieldNames::Shape, ParameterTypeIndex::Float);

            updateNodeIds();
        }
    };

    class Transformation : public ClonableNode<Transformation>
    {
      public:
        Transformation()
            : Transformation({})
        {
        }

        explicit Transformation(NodeId id)
            : ClonableNode<Transformation>(NodeName("Transformation"), id, Category::Internal)
        {
            TypeRule vectorMatToVector = {
              RuleType::Default,
              InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                           {FieldNames::Transformation, ParameterTypeIndex::Matrix4}},
              OutputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3}}};

            m_typeRules = {vectorMatToVector};
            applyTypeRule(m_typeRules.front());

            updateNodeIds();

            m_parameter.at(FieldNames::Transformation).setInputSourceRequired(false);
        }

        // name does not match, but it is called when the model is updated
        void updateMemoryOffsets(GeneratorContext &) override
        {
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }

        [[nodiscard]] bool isExemptFromInputValidation() const override
        {
            return true;
        }

      private:
    };

    class DotProduct : public CloneableABtoResult<DotProduct>
    {
      public:
        DotProduct()
            : DotProduct({})
        {
        }

        explicit DotProduct(NodeId id)
            : CloneableABtoResult<DotProduct>(NodeName("DotProduct"), id, Category::Math)
        {

            TypeRule vectorToScalar = {
              RuleType::Default,
              InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
                           {FieldNames::B, ParameterTypeIndex::Float3}},
              OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

            m_typeRules = {vectorToScalar};
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the dot product of A and B"};
        }
    };

    class CrossProduct : public CloneableABtoResult<CrossProduct>
    {
      public:
        CrossProduct()
            : CrossProduct({})
        {
        }

        explicit CrossProduct(NodeId id)
            : CloneableABtoResult<CrossProduct>(NodeName("CrossProduct"), id, Category::Math)
        {
            TypeRule vectorToVector = {
              RuleType::Default,
              InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
                           {FieldNames::B, ParameterTypeIndex::Float3}},
              OutputTypeMap{{FieldNames::Vector, ParameterTypeIndex::Float3}}};

            m_typeRules = {vectorToVector};
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the cross product of A and B"};
        }
    };

    class MatrixVectorMultiplication : public CloneableABtoResult<MatrixVectorMultiplication>
    {
      public:
        MatrixVectorMultiplication()
            : MatrixVectorMultiplication({})
        {
        }

        explicit MatrixVectorMultiplication(NodeId id)
            : CloneableABtoResult<MatrixVectorMultiplication>(
                NodeName("MatrixVectorMultiplication"),
                id,
                Category::Math)
        {
            TypeRule matrixVectorToVector = {
              RuleType::Default,
              InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4},
                           {FieldNames::B, ParameterTypeIndex::Float3}},
              OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}};

            m_typeRules = {matrixVectorToVector};
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the product of Matrix and Vector"};
        }
    };

    class Transpose : public ClonableNode<Transpose>
    {
      public:
        Transpose()
            : Transpose({})
        {
        }

        explicit Transpose(NodeId id)
            : ClonableNode<Transpose>(NodeName("Transpose"), id, Category::Math)
        {
            TypeRule matrixToMatrix = {
              RuleType::Default,
              InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4}},
              OutputTypeMap{{FieldNames::Matrix, ParameterTypeIndex::Matrix4}}};

            m_typeRules = {matrixToMatrix};
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the transpose of Matrix"};
        }
    };

    class Inverse : public ClonableNode<Inverse>
    {
      public:
        Inverse()
            : Inverse({})
        {
        }

        explicit Inverse(NodeId id)
            : ClonableNode<Inverse>(NodeName("Inverse"), id, Category::Math)
        {
            TypeRule matrixToMatrix = {
              RuleType::Default,
              InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4}},
              OutputTypeMap{{FieldNames::Matrix, ParameterTypeIndex::Matrix4}}};

            m_typeRules = {matrixToMatrix};
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the inverse of Matrix"};
        }
    };

    class Resource : public ClonableNode<Resource>
    {
      public:
        Resource()
            : Resource({})
        {
        }

        explicit Resource(NodeId id)
            : ClonableNode<Resource>(NodeName("Resource"), id, Category::Misc)
        {
            m_typeRules = {{RuleType::Default,
                            InputTypeMap{{FieldNames::ResourceId, ParameterTypeIndex::ResourceId}},
                            OutputTypeMap{{FieldNames::Value, ParameterTypeIndex::ResourceId}}}};
            NodeBase::applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] ResourceId getResourceId() const
        {

            auto varResId = m_parameter.at(FieldNames::ResourceId).getValue();
            if (const auto resId = std::get_if<ResourceId>(&varResId))
            {
                return *resId;
            }
            else
            {
                throw std::runtime_error(fmt::format(
                  "The ResourceId of the Resource node {} is not set ", getDisplayName()));
            }
        }

        const Port & getOutputValue() const
        {
            return NodeBase::m_outputs.at(FieldNames::Value);
        }

        void setResourceId(ResourceId resId)
        {
            m_resourceId = resId;
            m_parameter.at(FieldNames::ResourceId) = VariantParameter(resId);
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the ResourceId of the Resource node"};
        }

        // name does not match, but it is called when the model is updated
        void updateMemoryOffsets(GeneratorContext &) override
        {
            for (auto & param : m_parameter)
            {
                param.second.setInputSourceRequired(false);
            }
        }

      private:
        ResourceId m_resourceId;
    };

    class ImageSampler : public ClonableNode<ImageSampler>
    {
      public:
        ImageSampler()
            : ImageSampler({})
        {
        }

        explicit ImageSampler(NodeId id)
            : ClonableNode<ImageSampler>(NodeName("ImageSampler"), id, Category::Misc)
        {
            m_typeRules = {{RuleType::Default,
                            InputTypeMap{{FieldNames::ResourceId, ParameterTypeIndex::ResourceId},
                                         {FieldNames::UVW, ParameterTypeIndex::Float3},
                                         {FieldNames::Filter, ParameterTypeIndex::Int},
                                         {FieldNames::TileStyleU, ParameterTypeIndex::Int},
                                         {FieldNames::TileStyleV, ParameterTypeIndex::Int},
                                         {FieldNames::TileStyleW, ParameterTypeIndex::Int},
                                         {FieldNames::Dimensions, ParameterTypeIndex::Float3},
                                         {FieldNames::Start, ParameterTypeIndex::Int},
                                         {FieldNames::End, ParameterTypeIndex::Int}},
                            OutputTypeMap{{FieldNames::Color, ParameterTypeIndex::Float3},
                                          {FieldNames::Alpha, ParameterTypeIndex::Float}}}};
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        ResourceId getImageResourceId()
        {
            auto imageResourceIdParameter = m_parameter.at(FieldNames::ResourceId);

            auto imageResourceIdSource = imageResourceIdParameter.getSource();
            if (!imageResourceIdSource.has_value())
            {
                throw std::runtime_error(
                  fmt::format("The ResourceId of the ImageSampler node {} needs the value of a "
                              "Resource node as an input",
                              getDisplayName()));
            }
            auto sourcePort = imageResourceIdSource.value().port;
            if (!sourcePort)
            {
                throw std::runtime_error(
                  fmt::format("The ResourceId of the ImageSampler node {} needs the value of a "
                              "Resource node as an input",
                              getDisplayName()));
            }
            auto sourceNode = sourcePort->getParent();

            if (!sourceNode)
            {
                throw std::runtime_error(
                  fmt::format("The ResourceId of the ImageSampler node {} needs the value of a "
                              "Resource node as an input",
                              getDisplayName()));
            }
            auto variantResId = sourceNode->parameter().at(FieldNames::ResourceId).getValue();
            if (const auto resId = std::get_if<ResourceId>(&variantResId))
            {
                return *resId;
            }
            else
            {
                throw std::runtime_error(
                  fmt::format("The ResourceId of the ImageSampler node {} needs the value of a "
                              "Resource node as an input",
                              getDisplayName()));
            }
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Samples the image with the given ResourceId at the given UVW coordinate"};
        }

        [[nodiscard]] SamplingFilter getFilter()
        {
            auto varFilter = m_parameter.at(FieldNames::Filter).getValue();
            if (const auto filter = std::get_if<int>(&varFilter))
            {
                return static_cast<SamplingFilter>(*filter);
            }
            else
            {
                throw std::runtime_error(
                  fmt::format("The Filter of the ImageSampler node {} needs the value of a "
                              "Filter node as an input",
                              getDisplayName()));
            }
        }

        [[nodiscard]] TextureTileStyle getTileStyleU()
        {
            auto varTileStyle = m_parameter.at(FieldNames::TileStyleU).getValue();
            if (const auto tileStyle = std::get_if<int>(&varTileStyle))
            {
                return static_cast<TextureTileStyle>(*tileStyle);
            }
            else
            {
                throw std::runtime_error(
                  fmt::format("The TileStyleU of the ImageSampler node {} needs the value of a "
                              "TileStyle node as an input",
                              getDisplayName()));
            }
        }

        [[nodiscard]] TextureTileStyle getTileStyleV()
        {
            auto varTileStyle = m_parameter.at(FieldNames::TileStyleV).getValue();
            if (const auto tileStyle = std::get_if<int>(&varTileStyle))
            {
                return static_cast<TextureTileStyle>(*tileStyle);
            }
            else
            {
                throw std::runtime_error(
                  fmt::format("The TileStyleV of the ImageSampler node {} needs the value of a "
                              "TileStyle node as an input",
                              getDisplayName()));
            }
        }

        [[nodiscard]] TextureTileStyle getTileStyleW()
        {
            auto varTileStyle = m_parameter.at(FieldNames::TileStyleW).getValue();
            if (const auto tileStyle = std::get_if<int>(&varTileStyle))
            {
                return static_cast<TextureTileStyle>(*tileStyle);
            }
            else
            {
                throw std::runtime_error(
                  fmt::format("The TileStyleW of the ImageSampler node {} needs the value of a "
                              "TileStyle node as an input",
                              getDisplayName()));
            }
        }

        void updateMemoryOffsets(GeneratorContext & generatorContext) override
        {
            // if (!(m_outputs.at(FieldNames::Color).isUsed() ||
            //       m_outputs.at(FieldNames::Alpha).isUsed()))
            // {
            //     return;
            // }

            m_parameter[FieldNames::Start].hide();
            m_parameter[FieldNames::End].hide();
            m_parameter[FieldNames::Dimensions].hide();

            m_parameter[FieldNames::Start].setInputSourceRequired(false);
            m_parameter[FieldNames::End].setInputSourceRequired(false);
            m_parameter[FieldNames::Dimensions].setInputSourceRequired(false);

            m_parameter[FieldNames::Filter].setInputSourceRequired(false);
            m_parameter[FieldNames::TileStyleU].setInputSourceRequired(false);
            m_parameter[FieldNames::TileStyleV].setInputSourceRequired(false);
            m_parameter[FieldNames::TileStyleW].setInputSourceRequired(false);

            auto imageResourceId = getImageResourceId();

            auto key = ResourceKey{imageResourceId};

            try
            {
                auto & resMan = generatorContext.resourceManager;
                auto & res = resMan.getResource(key);
                res.setInUse(true);
                ImageStackResource * imageStack = dynamic_cast<ImageStackResource *>(&res);
                VdbResource * vdbResource = dynamic_cast<VdbResource *>(&res);

                if (imageStack)
                {
                    m_parameter.at(FieldNames::Start).setValue(imageStack->getStartIndex());
                    m_parameter.at(FieldNames::End).setValue(imageStack->getEndIndex());

                    float3 dimensions = {static_cast<float>(imageStack->getWidth()),
                                         static_cast<float>(imageStack->getHeight()),
                                         static_cast<float>(imageStack->getNumSheets())};
                    m_parameter.at(FieldNames::Dimensions).setValue(dimensions);

                    m_numberOfChannels = imageStack->getNumChannels();
                }
                else if (vdbResource)
                {
                    m_parameter.at(FieldNames::Start).setValue(vdbResource->getStartIndex());
                    m_parameter.at(FieldNames::End).setValue(vdbResource->getEndIndex());

                    float3 dimensions = vdbResource->getGridSize();
                    m_parameter.at(FieldNames::Dimensions).setValue(dimensions);

                    m_numberOfChannels = 1;
                    m_isVdbGrid = true;
                }
                else
                {
                    throw std::runtime_error(
                      fmt::format("The resource referenced by ResourceId of the ImageSampler node "
                                  "{} needs to be a ImageStackResource node",
                                  getDisplayName()));
                }

                m_parameter.at(FieldNames::Start).setValue(res.getStartIndex());
                m_parameter.at(FieldNames::End).setValue(res.getEndIndex());
            }
            catch (...)
            {
                m_parameter.at(FieldNames::Start).setValue(0);
                m_parameter.at(FieldNames::End).setValue(0);
            }
        }

        [[nodiscard]] bool isVdbGrid()
        {
            return m_isVdbGrid;
        }

      private:
        bool m_isVdbGrid = false;
        size_t m_numberOfChannels = 4;
    };

    class Round : public CloneableAtoResult<Round>
    {
      public:
        Round()
            : Round({})
        {
        }

        explicit Round(NodeId id)
            : CloneableAtoResult<Round>(NodeName("Round"), id, Category::Math)
        {
            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Rounds the value to the nearest integer"};
        }
    };

    class Ceil : public CloneableAtoResult<Ceil>
    {
      public:
        Ceil()
            : Ceil({})
        {
        }

        explicit Ceil(NodeId id)
            : CloneableAtoResult<Ceil>(NodeName("Ceil"), id, Category::Math)
        {
            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the smallest integer value not less than A"};
        }
    };

    class Floor : public CloneableAtoResult<Floor>
    {
      public:
        Floor()
            : Floor({})
        {
        }

        explicit Floor(NodeId id)
            : CloneableAtoResult<Floor>(NodeName("Floor"), id, Category::Math)
        {
            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the largest integer value not greater than A"};
        }
    };

    class Fract : public CloneableAtoResult<Fract>
    {
      public:
        Fract()
            : Fract({})
        {
        }

        explicit Fract(NodeId id)
            : CloneableAtoResult<Fract>(NodeName("Fract"), id, Category::Math)
        {
            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the fractional part of A"};
        }
    };

    class Sign : public CloneableAtoResult<Sign>
    {
      public:
        Sign()
            : Sign({})
        {
        }

        explicit Sign(NodeId id)
            : CloneableAtoResult<Sign>(NodeName("Sign"), id, Category::Math)
        {
            m_typeRules = functionRules();
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns the sign of A"};
        }
    };

    class VectorFromScalar : public CloneableAtoResult<VectorFromScalar>
    {
      public:
        VectorFromScalar()
            : VectorFromScalar({})
        {
        }

        explicit VectorFromScalar(NodeId id)
            : CloneableAtoResult<VectorFromScalar>(NodeName("VectorFromScalar"), id, Category::Math)
        {
            m_typeRules = {
              TypeRule{RuleType::Default,
                       InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float}},
                       OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}}};
            applyTypeRule(m_typeRules.front());
            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Returns a vector with the given value in all components"};
        }
    };

    class BoxMinMax : public ClonableNode<BoxMinMax>
    {
      public:
        BoxMinMax()
            : BoxMinMax({})
        {
        }

        explicit BoxMinMax(NodeId id)
            : ClonableNode<BoxMinMax>(NodeName("BoxMinMax"), id, Category::Internal)
        {

            m_typeRules = {{RuleType::Vector,
                            InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                                         {FieldNames::Min, ParameterTypeIndex::Float3},
                                         {FieldNames::Max, ParameterTypeIndex::Float3}},
                            OutputTypeMap{{FieldNames::Shape, ParameterTypeIndex::Float}}}};
            applyTypeRule(m_typeRules.front());

            updateNodeIds();
        }

        [[nodiscard]] std::string getDescription() const override
        {
            return {"Sdf of an box from A to B."};
        }
    };

} // namespace gladius::nodes
