#include "ExpressionParser.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <muParser.h>
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

        /// Constructor to set up custom functions
        Impl()
        {
            setupCustomFunctions();
        }

        bool parseExpression(std::string const & expressionStr)
        {
            try
            {
                m_expression = expressionStr;

                // Preprocess the expression to replace component access notation
                std::string preprocessedExpr = preprocessComponentAccess(expressionStr);
                m_parser.SetExpr(preprocessedExpr);

                // Try to evaluate with dummy variables to validate syntax
                // First, extract variables from the preprocessed expression
                auto variables = extractVariables(preprocessedExpr);

                // Set dummy values for all variables
                for (auto const & var : variables)
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
            catch (mu::Parser::exception_type const & e)
            {
                m_lastError = e.GetMsg();
                m_hasValidExpression = false;
                return false;
            }
            catch (std::exception const & e)
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

            // Extract variables from the original expression (with dot notation)
            // This preserves the component access for the graph converter
            return extractVariablesFromOriginal(m_expression);
        }

        std::string getExpressionString() const
        {
            if (!m_hasValidExpression)
            {
                return "";
            }

            return m_expression;
        }

        double evaluate(std::map<std::string, double> const & variableValues)
        {
            if (!m_hasValidExpression)
            {
                throw std::runtime_error("No valid expression to evaluate");
            }

            // Set variable values
            for (auto const & [name, value] : variableValues)
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
        /// Setup custom functions for muParser
        void setupCustomFunctions()
        {
            // Add exp function (exponential)
            m_parser.DefineFun("exp", [](double x) -> double { return std::exp(x); });

            // Add clamp function (clamp value between min and max)
            m_parser.DefineFun("clamp",
                               [](double x, double minVal, double maxVal) -> double
                               { return std::min(std::max(x, minVal), maxVal); });

            // Define common mathematical constants
            // Ensure tokens like 'pi' and 'e' are recognized as constants by muParser
            try
            {
                m_parser.DefineConst("pi", 3.141592653589793238462643383279502884L);
                m_parser.DefineConst("e", 2.718281828459045235360287471352662497L);
            }
            catch (...)
            {
                // Defining constants should not throw, but ignore if it does
            }
        }

        std::string preprocessComponentAccess(std::string const & expression) const
        {
            std::string result = expression;

            // First, check for any dot notation patterns
            std::regex anyDotRegex(R"(([a-zA-Z][a-zA-Z0-9_]*)\.([a-zA-Z0-9_]+))");
            std::smatch match;
            std::string::const_iterator searchStart = expression.cbegin();

            while (std::regex_search(searchStart, expression.cend(), match, anyDotRegex))
            {
                std::string component = match[2].str();

                // Only accept single character components x, y, z
                if (component.length() != 1 ||
                    (component != "x" && component != "y" && component != "z"))
                {
                    // Invalid component - return original expression to let muParser fail
                    return expression;
                }

                searchStart = match.suffix().first;
            }

            // Now replace valid component access notation with underscore notation
            std::regex componentRegex(R"(([a-zA-Z][a-zA-Z0-9_]*)\.([xyz]))");
            result = std::regex_replace(result, componentRegex, "$1_$2");

            return result;
        }
        std::vector<std::string> extractVariablesFromOriginal(std::string const & expression) const
        {
            std::vector<std::string> variables;

            // First find component access patterns (var.x, var.y, var.z)
            std::regex componentRegex(R"(([a-zA-Z][a-zA-Z0-9_]*)\.([xyz]))");
            std::smatch match;

            std::string::const_iterator searchStart = expression.cbegin();
            while (std::regex_search(searchStart, expression.cend(), match, componentRegex))
            {
                std::string fullMatch = match.str(); // e.g., "pos.x"

                // Add the full component access as a variable
                if (std::find(variables.begin(), variables.end(), fullMatch) == variables.end())
                {
                    variables.push_back(fullMatch);
                }

                searchStart = match.suffix().first;
            }

            // Then find regular variables (avoiding those that are part of component access)
            std::regex varRegex(R"([a-zA-Z][a-zA-Z0-9_]*)");

            searchStart = expression.cbegin();
            while (std::regex_search(searchStart, expression.cend(), match, varRegex))
            {
                std::string var = match.str();

                // Skip known functions
                if (var != "sin" && var != "cos" && var != "tan" && var != "exp" && var != "log" &&
                    var != "sqrt" && var != "abs" && var != "pi" && var != "e" && var != "pow" &&
                    var != "min" && var != "max" && var != "atan2" && var != "fmod" &&
                    var != "clamp")
                {
                    // Check if this variable is part of a component access
                    bool isPartOfComponentAccess = false;
                    size_t absolutePos =
                      std::distance(expression.cbegin(), searchStart) + match.position(0);

                    // Check if this variable is followed by ".x", ".y", or ".z"
                    if (absolutePos + var.length() < expression.length() &&
                        expression[absolutePos + var.length()] == '.')
                    {
                        if (absolutePos + var.length() + 1 < expression.length())
                        {
                            char component = expression[absolutePos + var.length() + 1];
                            if (component == 'x' || component == 'y' || component == 'z')
                            {
                                isPartOfComponentAccess = true;
                            }
                        }
                    }

                    // Also check if this variable is a component character (x,y,z) preceded by a
                    // dot
                    if ((var == "x" || var == "y" || var == "z") && var.length() == 1)
                    {
                        if (absolutePos > 0 && expression[absolutePos - 1] == '.')
                        {
                            isPartOfComponentAccess = true;
                        }
                    }

                    // Only add if it's not part of component access and not already in the list
                    if (!isPartOfComponentAccess &&
                        std::find(variables.begin(), variables.end(), var) == variables.end())
                    {
                        variables.push_back(var);
                    }
                }

                searchStart = match.suffix().first;
            }

            return variables;
        }

        std::vector<std::string> extractVariables(std::string const & expression) const
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
                if (var != "sin" && var != "cos" && var != "tan" && var != "exp" && var != "log" &&
                    var != "sqrt" && var != "abs" && var != "pi" && var != "e" && var != "clamp")
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

    bool ExpressionParser::parseExpression(std::string const & expression)
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

    double ExpressionParser::evaluate(std::map<std::string, double> const & variables)
    {
        return m_impl->evaluate(variables);
    }

} // namespace gladius
