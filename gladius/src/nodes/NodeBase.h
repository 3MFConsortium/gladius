#pragma once
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "../ResourceManager.h"
#include "../types.h"
#include "Parameter.h"
#include "Port.h"
#include "Primitives.h"
#include "nodesfwd.h"

namespace gladius
{
    class ComputeContext;
    class ResourceContext;
}

namespace gladius::nodes
{
    using ParameterMap = std::map<ParameterName, VariantParameter>;
    using Ports = std::map<PortName, Port>;

    using Outputs = Ports;

    using InputTypeMap = std::unordered_map<ParameterName, std::type_index>;
    using OutputTypeMap = std::unordered_map<PortName, std::type_index>;

    enum class RuleType
    {
        Default,
        Scalar,
        Vector,
        Matrix
    };

    struct TypeRule // Defines which output types to use for which input types
    {
        RuleType type{RuleType::Default};
        InputTypeMap input{};
        OutputTypeMap output{};
    };

    using TypeRules = std::vector<TypeRule>;

    /// Equal Operator for InputTypeMap
    bool operator==(const InputTypeMap & lhs, const InputTypeMap & rhs);
    struct GeneratorContext
    {
        GeneratorContext(SharedResources resourceContext, std::filesystem::path assemblyDir)
            : resourceManager(resourceContext, std::move(assemblyDir))
            , m_resourceContext(resourceContext) {};

        SharedPrimitives primitives{nullptr};
        ResourceManager resourceManager;
        std::filesystem::path basePath{};
        SharedComputeContext computeContext{nullptr};

      private:
        // Keep the resources alive for the lifetime of the GeneratorContext
        SharedResources m_resourceContext;
    };

    class NodeBase
    {
      public:
        NodeBase(NodeName baseName, NodeId internalId, Category category)
            : m_name(std::move(baseName))
            , m_id(internalId)
            , m_category(category)
        {
            m_unique_name = NodeName(m_name + "_" + std::to_string(m_id));
            m_displayName = m_unique_name;
        };

        virtual ~NodeBase() = default;

        ParameterMap & parameter()
        {
            return m_parameter;
        }

        VariantParameter * getParameter(ParameterName const & parameterName)
        {
            auto const iter = m_parameter.find(parameterName);

            if (iter == m_parameter.end())
            {
                return {};
            }

            return &iter->second;
        }

        ParameterMap const & constParameter() const
        {
            return m_parameter;
        }

        [[nodiscard]] auto name() const -> NodeName const &
        {
            return m_name;
        }

        [[nodiscard]] auto getUniqueName() const -> NodeName const &
        {
            return m_unique_name;
        }

        void setUniqueName(const std::string & name)
        {
            m_unique_name = name;
            for (auto & port : m_outputs)
            {
                port.second.setUniqueName(m_unique_name + "_" + port.first);
            }
        }

        NodeName const & getDisplayName() const
        {
            if (m_displayName.empty())
            {
                return m_name;
            }
            return m_displayName;
        }

        void setDisplayName(const NodeName & displayName)
        {
            m_displayName = displayName;
        }

        [[nodiscard]] auto outputs() const -> Outputs const &
        {
            return m_outputs;
        }

        [[nodiscard]] Outputs & getOutputs()
        {
            return m_outputs;
        }

        [[nodiscard]] Outputs const & getOutputs() const
        {
            return m_outputs;
        }

        Port * findOutputPort(PortName const & portName)
        {
            auto const iter = m_outputs.find(portName);
            if (iter == m_outputs.end())
            {
                return nullptr;
            }
            return &iter->second;
        }

        [[nodiscard]] auto getId() const -> NodeId
        {
            return m_id;
        }

        void setId(NodeId id)
        {
            m_id = id;
        }

        virtual void accept(Visitor & /*unused*/) {};

        auto screenPos() -> float2 &
        {
            return m_screenPos;
        }

        [[nodiscard]] auto getCategory() const -> Category
        {
            return m_category;
        }

        void setOrder(NodeId order)
        {
            m_order = order;
        }

        [[nodiscard]] auto getOrder() const -> NodeId
        {
            return m_order;
        }
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            auto & newPort = m_outputs[portName];
            newPort.setShortName(portName);
            newPort.setUniqueName(getUniqueName() + "_" + portName);
            newPort.setTypeIndex(typeIndex);
            newPort.setParent(this);
        }

        VariantParameter * addInput(const ParameterName & inputName)
        {
            auto & newInput = m_parameter[inputName];
            newInput.setParentId(getId());
            return &newInput;
        }

        [[nodiscard]] int getDepth() const
        {
            return m_depth;
        }

        void setDepth(int depth)
        {
            m_depth = depth;
        }

        std::unique_ptr<NodeBase> clone() const
        {
            return std::unique_ptr<NodeBase>(this->cloneImpl());
        }
        void updateNodeIds();

        [[nodiscard]] virtual bool parameterChangeInvalidatesPayload() const
        {
            return false;
        }

        virtual void generate(GeneratorContext & /*generatorContext*/) {};

        virtual void updateMemoryOffsets(GeneratorContext & /*generatorContext*/) {};

        [[nodiscard]] virtual std::string getDescription() const
        {
            return {"Basic node"};
        }

        bool updateTypes(nodes::Model & model);

        virtual void applyTypeRule(const TypeRule & rule)
        {
            // Loop through each expected input in the rule
            for (const auto & [expectedName, expectedType] : rule.input)
            {
                // Check if parameter already exists and has a different type
                const auto iter = m_parameter.find(expectedName);
                OptionalSource source;
                if (iter != m_parameter.end())
                {
                    source = iter->second.getSource();
                }

                if (iter == m_parameter.end() || iter->second.getTypeIndex() != expectedType)
                {
                    // Replace parameter with a new one of the expected type
                    m_parameter[expectedName] = createVariantTypeFromTypeIndex(expectedType);
                    m_parameter[expectedName].setParentId(getId());
                    if (source.has_value())
                    {
                        m_parameter[expectedName].setSource(source.value());
                    }
                }
            }

            // Loop through each expected output in the rule
            for (const auto & [outputName, outputType] : rule.output)
            {
                // Find or create a new output port with the given name
                auto iter = m_outputs.find(outputName);
                if (iter == m_outputs.end())
                {
                    addOutputPort(outputName, outputType);
                }
                else
                {
                    iter->second.setTypeIndex(outputType);
                }
            }

            m_ruleType = rule.type;
            updateNodeIds();
        }

        [[nodiscard]] auto getTag() const -> NodeName const &
        {
            return m_tag;
        }

        void setTag(const NodeName & tag)
        {
            m_tag = tag;
        }

        [[nodiscard]] RuleType getRuleType() const
        {
            return m_ruleType;
        }

      protected:
        ParameterMap m_parameter;
        NodeName m_name;
        NodeName m_unique_name;
        NodeName m_displayName;
        NodeName m_tag;
        NodeId m_id{};
        NodeId m_order{};
        int m_depth{};
        Category m_category{Category::Internal};
        Outputs m_outputs;
        float2 m_screenPos{};
        TypeRules m_typeRules;
        RuleType m_ruleType{RuleType::Default};

      private:
        [[nodiscard]] virtual NodeBase * cloneImpl() const
        {
            return new NodeBase(*this);
        }
    };

    inline void NodeBase::updateNodeIds()
    {
        for (auto & port : m_outputs)
        {
            port.second.setParent(this);
        }

        for (auto & parameter : m_parameter)
        {
            parameter.second.setParentId(getId());
        }
    }

} // namespace gladius::nodes
