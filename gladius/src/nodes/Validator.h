#pragma once

#include "Assembly.h"
#include "Parameter.h"

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
        void validateModel(Model & model);
        void validateNode(NodeBase & node, Model & model);
        ValidationErrors m_errors;
    };
}