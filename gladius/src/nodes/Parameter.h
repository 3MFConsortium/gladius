#pragma once

#include <gpgpu.h>
#include <sstream>

#include "BuildParameter.h"
#include "Port.h"
#include "ResourceKey.h"
#include "nodesfwd.h"
#include "types.h"

#include <array>
#include <string>
#include <variant>

namespace gladius::nodes
{

    struct ParameterTypeIndex
    {
        inline static auto const Int = std::type_index(typeid(int));
        inline static auto const ResourceId = std::type_index(typeid(uint32_t));
        inline static auto const Float = std::type_index(typeid(float));
        inline static auto const Float3 = std::type_index(typeid(float3));
        inline static auto const String = std::type_index(typeid(std::string));
        inline static auto const Matrix4 = std::type_index(typeid(Matrix4x4));
    };

    struct Source
    {
        PortId portId{};
        NodeId nodeId{};
        PortName uniqueName{};
        PortName shortName{};
        std::type_index type = ParameterTypeIndex::Float;
        Port * port = nullptr;
    };

    using OptionalSource = std::optional<Source>;

    using VariantType =
      std::variant<float, float3, Matrix4x4, int, std::string, ResourceKey, ResourceId>;

    template <class V>
    std::type_info const & getVariantType(V const & v)
    {
        return std::visit([](auto && x) -> decltype(auto) { return typeid(x); }, v);
    }

    class IParameter
    {
      public:
        virtual auto toString() -> std::string
        {
            return "";
        }

        virtual ~IParameter() = default;

        virtual void setInputFromPort(Port & /*unused*/) {};
        virtual void setSource(OptionalSource const & source) = 0;

        virtual auto getSource() -> OptionalSource &
        {
            return m_source;
        };

        void setId(ParameterId id)
        {
            m_parameterId = id;
        }

        [[nodiscard]] auto getId() const -> ParameterId
        {
            return m_parameterId;
        }

        void setParentId(NodeId parentId)
        {
            m_parentId = parentId;
        }

        [[nodiscard]] auto getParentId() const -> NodeId
        {
            return m_parentId;
        }

        virtual auto getContentType() const -> ContentType = 0;

        virtual void setLookUpIndex(int index) = 0;
        virtual auto getLookUpIndex() -> int = 0;

        /// returns the number of components
        virtual int getSize() const = 0;

        virtual bool isArgument() const = 0;

        void setModifiable(bool modifiable)
        {
            m_isModifiable = modifiable;
        }

        [[nodiscard]] auto isModifiable() const -> bool
        {
            return m_isModifiable;
        }

        virtual std::type_index getTypeIndex() const = 0;

      protected:
        OptionalSource m_source{};
        ParameterId m_parameterId{-1};
        NodeId m_parentId{};
        bool m_isModifiable = true;
    };

    template <typename T>
    class Parameter : public IParameter
    {
      public:
        Parameter() = default;

        // default copy constructor
        Parameter(Parameter const &) = default;

        // default move constructor
        Parameter(Parameter &&) = default;

        // default copy assignment operator
        Parameter & operator=(Parameter const &) = default;

        explicit Parameter(T val)
            : m_value(std::move(val))
        {
            if constexpr (std::is_same_v<T, VariantType>)
            {
                if (auto const pval = std::get_if<float>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Float;
                    m_contentType = ContentType::Length;
                }
                else if (auto const pval = std::get_if<nodes::float3>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Float3;
                    m_contentType = ContentType::Length;
                }
                else if (auto const pval = std::get_if<Matrix4x4>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Matrix4;
                    m_contentType = ContentType::Transformation;
                }
                else if (auto const pval = std::get_if<int>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Int;
                    m_contentType = ContentType::Index;
                }
                else if (auto const pval = std::get_if<std::string>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::String;
                    m_contentType = ContentType::Text;
                }
                else if (auto const pval = std::get_if<uint32_t>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::ResourceId;
                    m_contentType = ContentType::Index;
                }
            }
        };

        explicit Parameter(T val, ContentType contentType)
            : m_value(std::move(val))
            , m_contentType(contentType)
        {
            if constexpr (std::is_same_v<T, VariantType>)
            {
                if (auto const pval = std::get_if<float>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Float;
                }
                else if (auto const pval = std::get_if<nodes::float3>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Float3;
                }
                else if (auto const pval = std::get_if<Matrix4x4>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Matrix4;
                }
                else if (auto const pval = std::get_if<int>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::Int;
                }
                else if (auto const pval = std::get_if<std::string>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::String;
                }
                else if (auto const pval = std::get_if<uint32_t>(&m_value))
                {
                    m_typeIndex = ParameterTypeIndex::ResourceId;
                }
            }
        };

        auto toString() -> std::string override
        {
            if constexpr (std::is_same_v<T, VariantType>)
            {
                if (m_source)
                {
                    return m_source.value().uniqueName;
                }

                if (auto const pval = std::get_if<float>(&m_value))
                {
                    if (isModifiable())
                    {
                        return " parameter[" + std::to_string(m_lookUpIndex) + "] ";
                    }
                    else
                    {
                        return std::to_string(*pval);
                    }
                }

                if (auto const pval = std::get_if<int>(&m_value))
                {
                    return " parameter[" + std::to_string(m_lookUpIndex) + "] ";
                }

                if (auto const pval = std::get_if<nodes::float3>(&m_value))
                {
                    if (isModifiable())
                    {
                        return " parameter[" + std::to_string(m_lookUpIndex) + "], parameter[" +
                               std::to_string(m_lookUpIndex + 1) + "], parameter[" +
                               std::to_string(m_lookUpIndex + 2) + "] ";
                    }
                    else
                    {
                        return std::to_string(pval->x) + ", " + std::to_string(pval->y) + ", " +
                               std::to_string(pval->z);
                    }
                }

                if (nodes::Matrix4x4 * const pval = std::get_if<nodes::Matrix4x4>(&m_value))
                {
                    if (isModifiable())
                    {
                        std::stringstream strStream;
                        strStream << "parameter[" << std::to_string(m_lookUpIndex) << "]";
                        for (int i = 1; i < 16; ++i)
                        {
                            strStream << ", parameter[" << std::to_string(m_lookUpIndex + i) << "]";
                        }

                        return strStream.str();
                    }
                    else
                    {
                        nodes::Matrix4x4 const & val = *pval;
                        return std::to_string(val[0][0]) + ", " + std::to_string(val[0][1]) + ", " +
                               std::to_string(val[0][2]) + ", " + std::to_string(val[0][3]) + ", " +
                               std::to_string(val[1][0]) + ", " + std::to_string(val[1][1]) + ", " +
                               std::to_string(val[1][2]) + ", " + std::to_string(val[1][3]) + ", " +
                               std::to_string(val[2][0]) + ", " + std::to_string(val[2][1]) + ", " +
                               std::to_string(val[2][2]) + ", " + std::to_string(val[2][3]) + ", " +
                               std::to_string(val[3][0]) + ", " + std::to_string(val[3][1]) + ", " +
                               std::to_string(val[3][2]) + ", " + std::to_string(val[3][3]);
                    }
                }
                return var_to_string(m_value);
            }
            else
            {
                if (m_source)
                {
                    return m_source.value().uniqueName;
                }
                return to_string(m_value);
            }
        }

        void setValue(T val)
        {
            m_value = std::move(val);
            m_source.reset();
        }

        [[nodiscard]] auto getValue() const -> T
        {
            return m_value;
        }

        auto Value() -> T &
        {
            return m_value;
        }

        void setInputFromPort(Port & port) override
        {
            Source newSource;
            newSource.uniqueName = port.getUniqueName();
            newSource.shortName = port.getShortName();
            newSource.portId = port.getId();
            newSource.nodeId = port.getParentId();
            newSource.type = port.getTypeIndex();
            newSource.port = &port;
            m_source.emplace(newSource);
        }

        OptionalSource & getSource() override
        {
            return m_source;
        }

        OptionalSource const & getConstSource() const
        {
            return m_source;
        }

        void setSource(OptionalSource const & source) override
        {
            m_source = source;
        }

        void setLookUpIndex(int index) override
        {
            m_lookUpIndex = index;
        }

        auto getLookUpIndex() -> int override
        {
            if (m_source)
            {
                return -m_source.value().portId;
            }
            return m_lookUpIndex;
        }

        [[nodiscard]] auto getContentType() const -> ContentType override
        {
            return m_contentType;
        }

        [[nodiscard]] bool isArgument() const override
        {
            return m_isArgument;
        }

        void marksAsArgument()
        {
            m_isArgument = true;
        }

        void hide()
        {
            m_visible = false;
        }

        void show()
        {
            m_visible = true;
        }

        [[nodiscard]] bool isVisible()
        {
            return m_visible;
        }

        std::string const & getArgumentAssoziation() const
        {
            return m_argumentAssoziation;
        }

        void setArgumentAssoziation(std::string argumentAssozation)
        {
            marksAsArgument();
            m_argumentAssoziation = std::move(argumentAssozation);
        }

        [[nodiscard]] int getSize() const override
        {
            if constexpr (std::is_same_v<T, VariantType>)
            {

                if (auto const pval = std::get_if<float>(&m_value))
                {
                    return 1;
                }

                if (auto const pval = std::get_if<int>(&m_value))
                {
                    return 1;
                }

                if (auto const pval = std::get_if<nodes::float3>(&m_value))
                {
                    return 3;
                }

                if (auto const pval = std::get_if<Matrix4x4>(&m_value))
                {
                    return 16;
                }
            }

            return 1;
        }

        [[nodiscard]] std::type_index getTypeIndex() const override
        {
            return m_typeIndex;
        }

        void setConsumedByFunction(bool consumed)
        {
            m_isConsumedByFunction = consumed;
        }

        [[nodiscard]] bool isConsumedByFunction() const
        {
            return m_isConsumedByFunction;
        }

        void setValid(bool valid)
        {
            m_isValid = valid;
        }

        [[nodiscard]] bool isValid() const
        {
            return m_isValid;
        }

        void setInputSourceRequired(bool required)
        {
            m_inputSourceRequired = required;
        }

        [[nodiscard]] bool isInputSourceRequired() const
        {
            return m_inputSourceRequired;
        }

      private:
        T m_value{};
        int m_lookUpIndex{}; // The location in the parameter buffer on the device
        ContentType m_contentType{ContentType::Generic};
        bool m_isArgument{false};
        bool m_visible{true};
        std::string m_argumentAssoziation;
        std::type_index m_typeIndex = ParameterTypeIndex::Float;
        bool m_isConsumedByFunction{
          false}; // If true, this parameter is consumed by a function call

        bool m_isValid{true};
        bool m_inputSourceRequired{true};
    };

    using VariantParameter = Parameter<VariantType>;
    VariantParameter createVariantTypeFromTypeIndex(std::type_index typeIndex);
    auto to_string(float3 const & val) -> std::string;
    auto var_to_string(VariantType const & val) -> std::string;

} // namespace gladius::nodes
