#pragma once

#include "Assembly.h"
#include "Parameter.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    struct ValidationError
    {
        std::string message;
        std::string model;
        std::string node;
        std::string port;
        std::string parameter;
    };

    using ValidationErrors = std::vector<ValidationError>;

    class Validator
    {
      public:
        Validator() = default;
        ~Validator() = default;

        [[nodiscard]] bool validate(Assembly & assembly);

        [[nodiscard]] ValidationErrors const & getErrors() const;

      private:
        void validateModel(Model & model, Assembly & assembly);
        void validateNode(NodeBase & node, Model & model, Assembly & assembly);
        void validateNodeImpl(NodeBase & node, Model & model);

        void validateNode(FunctionCall & node, Model & model, Assembly & assembly);
        ValidationErrors m_errors;
    };
}