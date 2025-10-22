#pragma once

#include "EventLogger.h"
#include "Model.h"
#include "nodesfwd.h"
#include "types.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace gladius::nodes
{
    class FunctionGradient;
    class FunctionCall;
    class Model;
    class Assembly;

    class LowerFunctionGradient
    {
      public:
        using ErrorReporter = std::function<void(std::string const &)>;

        explicit LowerFunctionGradient(Assembly & assembly, events::SharedLogger logger = {});
        LowerFunctionGradient(Assembly & assembly,
                              events::SharedLogger logger,
                              ErrorReporter reporter);

        void run();
        [[nodiscard]] bool hadErrors() const noexcept;

      private:
        struct GradientSignature
        {
            ResourceId referencedFunction{};
            std::string scalarOutput;
            std::string vectorInput;

            bool operator==(GradientSignature const & other) const noexcept;
        };

        struct GradientSignatureHash
        {
            std::size_t operator()(GradientSignature const & key) const noexcept;
        };

        struct GradientTarget
        {
            ResourceId modelId{};
            NodeId nodeId{};
        };

        Assembly & m_assembly;
        events::SharedLogger m_logger;
        ErrorReporter m_errorReporter;
        bool m_hadErrors{false};
        ResourceId m_nextModelId{};
        std::unordered_map<GradientSignature, ResourceId, GradientSignatureHash> m_cache;

        [[nodiscard]] ResourceId allocateModelId();
        [[nodiscard]] SharedModel getLoweredModel(ResourceId id) const;
        [[nodiscard]] std::optional<std::string>
        validateConfiguration(FunctionGradient const & gradient,
                              Model const * referencedModel) const;
        [[nodiscard]] ResourceId
        lowerGradient(FunctionGradient & gradient, Model & parentModel, Model & referencedModel);
        [[nodiscard]] ResourceId ensureLoweredFunction(GradientSignature const & key,
                                                       FunctionGradient & gradient,
                                                       Model & referencedModel);
        SharedModel synthesizeGradientModel(GradientSignature const & key,
                                            FunctionGradient & gradient,
                                            Model & referencedModel,
                                            ResourceId newId);
        void replaceGradientWithCall(FunctionGradient & gradient,
                                     Model & parentModel,
                                     SharedModel const & loweredModel);
        void rewireConsumers(Model & model, Port & from, Port & to);
        void copyParameter(Model & model,
                           VariantParameter const & sourceParam,
                           VariantParameter & targetParam);
        static std::string sanitizeName(std::string value);
        void reportError(std::string const & message);
    };
} // namespace gladius::nodes
