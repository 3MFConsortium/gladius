#pragma once

#include "NodeBase.h"

#include "ClonableNode.h"

#include <concepts>

namespace gladius::nodes
{

    template <typename T>
    concept IsDerivedFromNodeBase = std::derived_from<T, NodeBase>;

    // Mixed in class adding specialized accessors
    template <IsDerivedFromNodeBase Base>
    class WithInputA : public Base
    {
      public:
        WithInputA(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        void setInputA(Port & port)
        {
            NodeBase::m_parameter[FieldNames::A].setInputFromPort(port);
        }

        const VariantParameter & getInputA() const
        {
            return NodeBase::m_parameter.at(FieldNames::A);
        }
    };

    template <IsDerivedFromNodeBase Base>
    class WithInputB : public Base
    {
      public:
        WithInputB(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        void setInputB(Port & port)
        {
            NodeBase::m_parameter[FieldNames::B].setInputFromPort(port);
        }

        const VariantParameter & getInputB() const
        {
            return NodeBase::m_parameter.at(FieldNames::B);
        }
    };

    template <IsDerivedFromNodeBase Base>
    class WithOutputResult : public Base
    {
      public:
        WithOutputResult(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "result" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getResultOutputPort()
        {
            if (!m_cachedResultPort)
            {
                updateCachedResultPort();
            }
            if (!m_cachedResultPort)
            {
                throw std::runtime_error("Result output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedResultPort;
        }

        /// @brief Get the "result" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getResultOutputPort() const
        {
            if (!m_cachedResultPort)
            {
                updateCachedResultPort();
            }
            if (!m_cachedResultPort)
            {
                throw std::runtime_error("Result output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedResultPort;
        }

        /// @brief Check if result output port exists without throwing
        [[nodiscard]] bool hasResultOutputPort() const
        {
            if (!m_cachedResultPort)
            {
                updateCachedResultPort();
            }
            return m_cachedResultPort != nullptr;
        }

        // Legacy accessor for backwards compatibility
        const Port & getOutputResult() const
        {
            return getResultOutputPort();
        }

      protected:
        /// @brief Update cached pointer to result output port
        void updateCachedResultPort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Result);
            m_cachedResultPort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Result)
            {
                updateCachedResultPort();
            }
        }

        void applyTypeRule(const TypeRule & rule)
        {
            Base::applyTypeRule(rule);
            // Check if result port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Result)
                {
                    updateCachedResultPort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedResultPort{nullptr};
    };

    /// @brief Mixin providing specialized access to "value" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputValue : public Base
    {
      public:
        WithOutputValue(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "value" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getValueOutputPort()
        {
            if (!m_cachedValuePort)
            {
                updateCachedValuePort();
            }
            if (!m_cachedValuePort)
            {
                throw std::runtime_error("Value output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedValuePort;
        }

        /// @brief Get the "value" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getValueOutputPort() const
        {
            if (!m_cachedValuePort)
            {
                updateCachedValuePort();
            }
            if (!m_cachedValuePort)
            {
                throw std::runtime_error("Value output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedValuePort;
        }

        /// @brief Check if value output port exists without throwing
        [[nodiscard]] bool hasValueOutputPort() const
        {
            if (!m_cachedValuePort)
            {
                updateCachedValuePort();
            }
            return m_cachedValuePort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to value output port
        void updateCachedValuePort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Value);
            m_cachedValuePort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Value)
            {
                updateCachedValuePort();
            }
        }

        void applyTypeRule(const TypeRule & rule)
        {
            Base::applyTypeRule(rule);
            // Check if value port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Value)
                {
                    updateCachedValuePort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedValuePort{nullptr};
    };

    /// @brief Mixin providing specialized access to "vector" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputVector : public Base
    {
      public:
        WithOutputVector(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "vector" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getVectorOutputPort()
        {
            if (!m_cachedVectorPort)
            {
                updateCachedVectorPort();
            }
            if (!m_cachedVectorPort)
            {
                throw std::runtime_error("Vector output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedVectorPort;
        }

        /// @brief Get the "vector" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getVectorOutputPort() const
        {
            if (!m_cachedVectorPort)
            {
                updateCachedVectorPort();
            }
            if (!m_cachedVectorPort)
            {
                throw std::runtime_error("Vector output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedVectorPort;
        }

        /// @brief Check if vector output port exists without throwing
        [[nodiscard]] bool hasVectorOutputPort() const
        {
            if (!m_cachedVectorPort)
            {
                updateCachedVectorPort();
            }
            return m_cachedVectorPort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to vector output port
        void updateCachedVectorPort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Vector);
            m_cachedVectorPort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Vector)
            {
                updateCachedVectorPort();
            }
        }

        void applyTypeRule(const TypeRule & rule)
        {
            Base::applyTypeRule(rule);
            // Check if vector port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Vector)
                {
                    updateCachedVectorPort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedVectorPort{nullptr};
    };

    /// @brief Mixin providing specialized access to "matrix" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputMatrix : public Base
    {
      public:
        WithOutputMatrix(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "matrix" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getMatrixOutputPort()
        {
            if (!m_cachedMatrixPort)
            {
                updateCachedMatrixPort();
            }
            if (!m_cachedMatrixPort)
            {
                throw std::runtime_error("Matrix output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedMatrixPort;
        }

        /// @brief Get the "matrix" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getMatrixOutputPort() const
        {
            if (!m_cachedMatrixPort)
            {
                updateCachedMatrixPort();
            }
            if (!m_cachedMatrixPort)
            {
                throw std::runtime_error("Matrix output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedMatrixPort;
        }

        /// @brief Check if matrix output port exists without throwing
        [[nodiscard]] bool hasMatrixOutputPort() const
        {
            if (!m_cachedMatrixPort)
            {
                updateCachedMatrixPort();
            }
            return m_cachedMatrixPort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to matrix output port
        void updateCachedMatrixPort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Matrix);
            m_cachedMatrixPort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Matrix)
            {
                updateCachedMatrixPort();
            }
        }

        void applyTypeRule(const TypeRule & rule) override
        {
            Base::applyTypeRule(rule);
            // Check if matrix port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Matrix)
                {
                    updateCachedMatrixPort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedMatrixPort{nullptr};
    };

    /// @brief Mixin providing specialized access to "shape" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputShape : public Base
    {
      public:
        WithOutputShape(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "shape" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getShapeOutputPort()
        {
            if (!m_cachedShapePort)
            {
                updateCachedShapePort();
            }
            if (!m_cachedShapePort)
            {
                throw std::runtime_error("Shape output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedShapePort;
        }

        /// @brief Get the "shape" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getShapeOutputPort() const
        {
            if (!m_cachedShapePort)
            {
                updateCachedShapePort();
            }
            if (!m_cachedShapePort)
            {
                throw std::runtime_error("Shape output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedShapePort;
        }

        /// @brief Check if shape output port exists without throwing
        [[nodiscard]] bool hasShapeOutputPort() const
        {
            if (!m_cachedShapePort)
            {
                updateCachedShapePort();
            }
            return m_cachedShapePort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to shape output port
        void updateCachedShapePort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Shape);
            m_cachedShapePort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Shape)
            {
                updateCachedShapePort();
            }
        }

        void applyTypeRule(const TypeRule & rule) override
        {
            Base::applyTypeRule(rule);
            // Check if shape port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Shape)
                {
                    updateCachedShapePort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedShapePort{nullptr};
    };

    /// @brief Mixin providing specialized access to "distance" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputDistance : public Base
    {
      public:
        WithOutputDistance(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "distance" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getDistanceOutputPort()
        {
            if (!m_cachedDistancePort)
            {
                updateCachedDistancePort();
            }
            if (!m_cachedDistancePort)
            {
                throw std::runtime_error("Distance output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedDistancePort;
        }

        /// @brief Get the "distance" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getDistanceOutputPort() const
        {
            if (!m_cachedDistancePort)
            {
                updateCachedDistancePort();
            }
            if (!m_cachedDistancePort)
            {
                throw std::runtime_error("Distance output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedDistancePort;
        }

        /// @brief Check if distance output port exists without throwing
        [[nodiscard]] bool hasDistanceOutputPort() const
        {
            if (!m_cachedDistancePort)
            {
                updateCachedDistancePort();
            }
            return m_cachedDistancePort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to distance output port
        void updateCachedDistancePort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Distance);
            m_cachedDistancePort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Distance)
            {
                updateCachedDistancePort();
            }
        }

        void applyTypeRule(const TypeRule & rule) override
        {
            Base::applyTypeRule(rule);
            // Check if distance port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Distance)
                {
                    updateCachedDistancePort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedDistancePort{nullptr};
    };

    /// @brief Mixin providing specialized access to "color" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputColor : public Base
    {
      public:
        WithOutputColor(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "color" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getColorOutputPort()
        {
            if (!m_cachedColorPort)
            {
                updateCachedColorPort();
            }
            if (!m_cachedColorPort)
            {
                throw std::runtime_error("Color output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedColorPort;
        }

        /// @brief Get the "color" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getColorOutputPort() const
        {
            if (!m_cachedColorPort)
            {
                updateCachedColorPort();
            }
            if (!m_cachedColorPort)
            {
                throw std::runtime_error("Color output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedColorPort;
        }

        /// @brief Check if color output port exists without throwing
        [[nodiscard]] bool hasColorOutputPort() const
        {
            if (!m_cachedColorPort)
            {
                updateCachedColorPort();
            }
            return m_cachedColorPort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to color output port
        void updateCachedColorPort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Color);
            m_cachedColorPort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Color)
            {
                updateCachedColorPort();
            }
        }

        void applyTypeRule(const TypeRule & rule) override
        {
            Base::applyTypeRule(rule);
            // Check if color port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Color)
                {
                    updateCachedColorPort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedColorPort{nullptr};
    };

    /// @brief Mixin providing specialized access to "alpha" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputAlpha : public Base
    {
      public:
        WithOutputAlpha(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "alpha" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getAlphaOutputPort()
        {
            if (!m_cachedAlphaPort)
            {
                updateCachedAlphaPort();
            }
            if (!m_cachedAlphaPort)
            {
                throw std::runtime_error("Alpha output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedAlphaPort;
        }

        /// @brief Get the "alpha" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getAlphaOutputPort() const
        {
            if (!m_cachedAlphaPort)
            {
                updateCachedAlphaPort();
            }
            if (!m_cachedAlphaPort)
            {
                throw std::runtime_error("Alpha output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedAlphaPort;
        }

        /// @brief Check if alpha output port exists without throwing
        [[nodiscard]] bool hasAlphaOutputPort() const
        {
            if (!m_cachedAlphaPort)
            {
                updateCachedAlphaPort();
            }
            return m_cachedAlphaPort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to alpha output port
        void updateCachedAlphaPort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Alpha);
            m_cachedAlphaPort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Alpha)
            {
                updateCachedAlphaPort();
            }
        }

        void applyTypeRule(const TypeRule & rule) override
        {
            Base::applyTypeRule(rule);
            // Check if alpha port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Alpha)
                {
                    updateCachedAlphaPort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedAlphaPort{nullptr};
    };

    /// @brief Mixin providing specialized access to "pos" output port
    template <IsDerivedFromNodeBase Base>
    class WithOutputPos : public Base
    {
      public:
        WithOutputPos(const NodeName & baseName, NodeId internalId, Category category)
            : Base(baseName, internalId, category)
        {
        }

        /// @brief Get the "pos" output port, returns reference (throws if not found)
        [[nodiscard]] Port & getPosOutputPort()
        {
            if (!m_cachedPosPort)
            {
                updateCachedPosPort();
            }
            if (!m_cachedPosPort)
            {
                throw std::runtime_error("Pos output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedPosPort;
        }

        /// @brief Get the "pos" output port (const version), returns reference (throws if not
        /// found)
        [[nodiscard]] Port const & getPosOutputPort() const
        {
            if (!m_cachedPosPort)
            {
                updateCachedPosPort();
            }
            if (!m_cachedPosPort)
            {
                throw std::runtime_error("Pos output port not found in node: " +
                                         this->getDisplayName());
            }
            return *m_cachedPosPort;
        }

        /// @brief Check if pos output port exists without throwing
        [[nodiscard]] bool hasPosOutputPort() const
        {
            if (!m_cachedPosPort)
            {
                updateCachedPosPort();
            }
            return m_cachedPosPort != nullptr;
        }

      protected:
        /// @brief Update cached pointer to pos output port
        void updateCachedPosPort() const
        {
            // Don't update cache during destruction
            if (this->m_outputs.empty())
                return;

            auto iter = this->m_outputs.find(FieldNames::Pos);
            m_cachedPosPort =
              (iter != this->m_outputs.end()) ? const_cast<Port *>(&iter->second) : nullptr;
        }

        /// @brief Override base class methods to update cache when outputs change
        void addOutputPort(const PortName & portName, std::type_index typeIndex)
        {
            Base::addOutputPort(portName, typeIndex);
            if (portName == FieldNames::Pos)
            {
                updateCachedPosPort();
            }
        }

        void applyTypeRule(const TypeRule & rule) override
        {
            Base::applyTypeRule(rule);
            // Check if pos port was modified
            for (const auto & [outputName, outputType] : rule.output)
            {
                if (outputName == FieldNames::Pos)
                {
                    updateCachedPosPort();
                    break;
                }
            }
        }

      private:
        mutable Port * m_cachedPosPort{nullptr};
    };

    template <IsDerivedFromNodeBase Base>
    using WithInputAB = WithInputB<WithInputA<Base>>;
    template <typename Base>
    using CloneableABtoResult = WithOutputResult<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoResult = WithOutputResult<WithInputA<ClonableNode<Base>>>;

    // Additional optimized combinations using specialized output mixins
    template <typename Base>
    using CloneableABtoValue = WithOutputValue<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoValue = WithOutputValue<WithInputA<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableABtoVector = WithOutputVector<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoVector = WithOutputVector<WithInputA<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableABtoMatrix = WithOutputMatrix<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoMatrix = WithOutputMatrix<WithInputA<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableABtoShape = WithOutputShape<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoShape = WithOutputShape<WithInputA<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableABtoDistance = WithOutputDistance<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoDistance = WithOutputDistance<WithInputA<ClonableNode<Base>>>;

    // Additional optimized combinations for color, alpha, and pos outputs
    template <typename Base>
    using CloneableABtoColor = WithOutputColor<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoColor = WithOutputColor<WithInputA<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableABtoAlpha = WithOutputAlpha<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoAlpha = WithOutputAlpha<WithInputA<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableABtoPos = WithOutputPos<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoPos = WithOutputPos<WithInputA<ClonableNode<Base>>>;

    // Combination mixins for nodes with multiple specific output types
    template <IsDerivedFromNodeBase Base>
    using WithOutputColorAlpha = WithOutputAlpha<WithOutputColor<Base>>;

    template <typename Base>
    using CloneableABtoColorAlpha = WithOutputColorAlpha<WithInputAB<ClonableNode<Base>>>;

    template <typename Base>
    using CloneableAtoColorAlpha = WithOutputColorAlpha<WithInputA<ClonableNode<Base>>>;
}