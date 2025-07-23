#include "ExpressionParser.h"

#include <muParser.h>
#include <exception>
#include <regex>

namespace gladius
{
    class ExpressionParser::Impl
    {
    public:
        mu::Parser m_parser;
        std::string m_expression;
        std::string m_lastError;
        bool m_hasValidExpression = false;
        std::map<std::string, double> m_variables;

        bool parseExpression(std::string const& expressionStr)
        {
            try
            {
                m_expression = expressionStr;
                m_parser.SetExpr(expressionStr);
                
                // Try to evaluate with dummy variables to validate syntax
                // First, extract variables from the expression
                auto variables = extractVariables(expressionStr);
                
                // Set dummy values for all variables
                for (auto const& var : variables)
                {
                    m_variables[var] = 0.0;
                    m_parser.DefineVar(var, &m_variables[var]);
                }
                
                // Try to evaluate to check for syntax errors
                m_parser.Eval();
                
                m_hasValidExpression = true;
                m_lastError.clear();
                return true;
            }
            catch (mu::Parser::exception_type const& e)
            {
                m_lastError = e.GetMsg();
                m_hasValidExpression = false;
                return false;
            }
            catch (std::exception const& e)
            {
                m_lastError = e.what();
                m_hasValidExpression = false;
                return false;
            }
        }

        std::vector<std::string> getVariables() const
        {
            if (!m_hasValidExpression)
            {
                return {};
            }

            return extractVariables(m_expression);
        }

        std::string getExpressionString() const
        {
            if (!m_hasValidExpression)
            {
                return "";
            }
            
            return m_expression;
        }

        double evaluate(std::map<std::string, double> const& variableValues)
        {
            if (!m_hasValidExpression)
            {
                throw std::runtime_error("No valid expression to evaluate");
            }

            // Set variable values
            for (auto const& [name, value] : variableValues)
            {
                auto it = m_variables.find(name);
                if (it != m_variables.end())
                {
                    it->second = value;
                }
            }

            return m_parser.Eval();
        }

    private:
        std::vector<std::string> extractVariables(std::string const& expression) const
        {
            std::vector<std::string> variables;
            
            // Simple regex to find variable names (letters followed by optional digits/letters)
            std::regex varRegex(R"([a-zA-Z][a-zA-Z0-9_]*)");
            std::smatch match;
            
            std::string::const_iterator searchStart = expression.cbegin();
            while (std::regex_search(searchStart, expression.cend(), match, varRegex))
            {
                std::string var = match.str();
                
                // Skip known functions (extend this list as needed)
                if (var != "sin" && var != "cos" && var != "tan" && 
                    var != "exp" && var != "log" && var != "sqrt" &&
                    var != "abs" && var != "pi" && var != "e")
                {
                    // Check if variable is already in the list
                    if (std::find(variables.begin(), variables.end(), var) == variables.end())
                    {
                        variables.push_back(var);
                    }
                }
                
                searchStart = match.suffix().first;
            }
            
            return variables;
        }
    };

    ExpressionParser::ExpressionParser()
        : m_impl(std::make_unique<Impl>())
    {
    }

    ExpressionParser::~ExpressionParser() = default;

    bool ExpressionParser::parseExpression(std::string const& expression)
    {
        return m_impl->parseExpression(expression);
    }

    std::string ExpressionParser::getLastError() const
    {
        return m_impl->m_lastError;
    }

    bool ExpressionParser::hasValidExpression() const
    {
        return m_impl->m_hasValidExpression;
    }

    std::string ExpressionParser::getExpressionString() const
    {
        return m_impl->getExpressionString();
    }

    std::vector<std::string> ExpressionParser::getVariables() const
    {
        return m_impl->getVariables();
    }

    double ExpressionParser::evaluate(std::map<std::string, double> const& variables)
    {
        return m_impl->evaluate(variables);
    }

} // namespace gladius
