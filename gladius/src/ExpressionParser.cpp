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
        // Map index in preprocessed expression -> index in original expression
        std::vector<size_t> m_indexMap;

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

                // Quick upfront checks for commonly misused/unsupported syntax to provide
                // immediate, actionable feedback before invoking muParser.
                // 1) Unsupported power operator '^' (use pow(a,b) instead)
                auto caretPos = m_expression.find('^');
                if (caretPos != std::string::npos)
                {
                    m_lastError = buildHintWithCaret(
                      "Operator '^' for power is not supported. Use pow(base, exponent) instead.",
                      caretPos);
                    m_hasValidExpression = false;
                    return false;
                }

                // 2) Comments are not supported
                auto slComment = m_expression.find("//");
                auto mlComment = m_expression.find("/*");
                if (slComment != std::string::npos || mlComment != std::string::npos)
                {
                    size_t p = (slComment != std::string::npos) ? slComment : mlComment;
                    m_lastError =
                      buildHintWithCaret("Comments are not supported in expressions.", p);
                    m_hasValidExpression = false;
                    return false;
                }

                // Preprocess the expression to replace component access notation and build index
                // map
                std::string preprocessedExpr;
                if (!preprocessComponentAccessWithMapping(
                      expressionStr, preprocessedExpr, m_indexMap))
                {
                    // preprocess function already populated m_lastError
                    m_hasValidExpression = false;
                    return false;
                }

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
                // Compose a rich error with position, token and a caret in the ORIGINAL expression
                // muParser reports positions based on the expression given to it (preprocessed),
                // so we translate that position back to the user's original string using
                // m_indexMap.
                try
                {
                    int pos = e.GetPos(); // 1-based index in muParser
                    size_t prePos = (pos > 0) ? static_cast<size_t>(pos - 1) : 0;
                    size_t origPos = (prePos < m_indexMap.size()) ? m_indexMap[prePos] : prePos;
                    std::string token = e.GetToken();

                    std::string header = e.GetMsg();
                    if (!token.empty())
                    {
                        header += std::string(" (near token '") + token + "')";
                    }
                    m_lastError = buildHintWithCaret(header, origPos);
                }
                catch (...)
                {
                    m_lastError = e.GetMsg();
                }
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
        // Preprocess component access and build index mapping back to original string.
        // Returns false if an invalid component is found (and sets m_lastError accordingly).
        bool preprocessComponentAccessWithMapping(std::string const & expression,
                                                  std::string & out,
                                                  std::vector<size_t> & indexMap) const
        {
            // Validate any dot-notation components first and capture the first offending position
            std::regex anyDotRegex(R"(([a-zA-Z][a-zA-Z0-9_]*)\.([a-zA-Z0-9_]+))");
            std::smatch match;
            std::string::const_iterator searchStart = expression.cbegin();
            while (std::regex_search(searchStart, expression.cend(), match, anyDotRegex))
            {
                std::string component = match[2].str();
                if (component.length() != 1 ||
                    (component != "x" && component != "y" && component != "z"))
                {
                    size_t pos =
                      static_cast<size_t>(match.position(2)) +
                      static_cast<size_t>(std::distance(expression.cbegin(), searchStart));
                    // Build an error directly referencing the original expression
                    const_cast<Impl *>(this)->m_lastError =
                      const_cast<Impl *>(this)->buildHintWithCaret(
                        std::string("Invalid vector component '.") + component +
                          "'. Only .x, .y, .z are allowed.",
                        pos);
                    return false;
                }
                searchStart = match.suffix().first;
            }

            // Build the preprocessed string and index map; default is identity mapping
            out.clear();
            indexMap.clear();
            out.reserve(expression.size());
            indexMap.reserve(expression.size());

            // Replace valid component access 'name.x' -> 'name_x' while keeping a mapping
            for (size_t i = 0; i < expression.size();)
            {
                if (std::isalpha(static_cast<unsigned char>(expression[i])) || expression[i] == '_')
                {
                    // Parse identifier
                    size_t start = i;
                    size_t j = i + 1;
                    while (j < expression.size() &&
                           (std::isalnum(static_cast<unsigned char>(expression[j])) ||
                            expression[j] == '_'))
                    {
                        ++j;
                    }
                    // If followed by .x/.y/.z, replace '.' with '_' and copy component
                    if (j + 1 < expression.size() && expression[j] == '.' &&
                        (expression[j + 1] == 'x' || expression[j + 1] == 'y' ||
                         expression[j + 1] == 'z'))
                    {
                        // Copy identifier
                        for (size_t k = start; k < j; ++k)
                        {
                            out.push_back(expression[k]);
                            indexMap.push_back(k);
                        }
                        // Replace '.' with '_'
                        out.push_back('_');
                        indexMap.push_back(j); // map '_' to '.' position
                        // Copy component letter
                        out.push_back(expression[j + 1]);
                        indexMap.push_back(j + 1);
                        i = j + 2;
                        continue;
                    }
                    // Else, copy the identifier as-is
                    for (size_t k = start; k < j; ++k)
                    {
                        out.push_back(expression[k]);
                        indexMap.push_back(k);
                    }
                    i = j;
                    continue;
                }
                // Default: copy single char
                out.push_back(expression[i]);
                indexMap.push_back(i);
                ++i;
            }
            return true;
        }

        std::string buildHintWithCaret(std::string const & message, size_t pos) const
        {
            std::string out;
            out.reserve(message.size() + m_expression.size() + 32);
            out += message;
            out += "\n\n";
            out += m_expression;
            out += "\n";
            // Clamp caret position to expression length
            size_t caret = std::min(pos, m_expression.size());
            for (size_t i = 0; i < caret; ++i)
            {
                out += (m_expression[i] == '\t') ? '\t' : ' ';
            }
            out += "^";
            out += "  (position ";
            out += std::to_string(caret);
            out += ")";
            return out;
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
                    var != "sqrt" && var != "abs" && var != "pi" && var != "e" && var != "clamp" &&
                    var != "pow")
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
