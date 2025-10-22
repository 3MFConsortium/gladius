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
    class NormalizeDistanceField;
    class Model;
    class Assembly;

    class LowerNormalizeDistanceField
    {
      public:
        using ErrorReporter = std::function<void(std::string const &)>;

        explicit LowerNormalizeDistanceField(Assembly & assembly, events::SharedLogger logger = {});
        LowerNormalizeDistanceField(Assembly & assembly,
                                    events::SharedLogger logger,
                                    ErrorReporter reporter);

        void run();
        [[nodiscard]] bool hadErrors() const noexcept;

      private:
        struct NormalizeTarget
        {
            ResourceId modelId{};
            NodeId nodeId{};
        };

        Assembly & m_assembly;
        events::SharedLogger m_logger;
        ErrorReporter m_errorReporter;
        bool m_hadErrors{false};
        ResourceId m_nextModelId{};

        [[nodiscard]] ResourceId allocateModelId();
        void lowerNormalizeNode(NormalizeDistanceField & normalizeNode, Model & parentModel);
        [[nodiscard]] std::optional<ResourceId>
        extractOrWrapDistanceSource(NormalizeDistanceField & normalizeNode, Model & parentModel);
        SharedModel createHelperFunction(NormalizeDistanceField & normalizeNode,
                                         Model & parentModel,
                                         ResourceId newId);
        void replaceNormalizeWithComposition(NormalizeDistanceField & normalizeNode,
                                             Model & parentModel,
                                             ResourceId helperFunctionId);
        void rewireConsumers(Model & model, Port & from, Port & to);
        void copyParameter(Model & model,
                           VariantParameter const & sourceParam,
                           VariantParameter & targetParam);
        static std::string sanitizeName(std::string value);
        void reportError(std::string const & message);
    };
} // namespace gladius::nodes
